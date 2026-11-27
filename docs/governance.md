# Repository governance

## Community defaults

OpenTLV/opentlv inherits the organization's contribution, conduct, security,
and support policies, all five issue forms, and the pull request template.
There are no local overrides. Keep the project license and any applicable
notices in this repository. See the [adoption guide] for how GitHub resolves
shared files.

On 2026-11-23, [Community Standards] linked the shared contribution, conduct,
security, and pull request files. The [security policy] displayed private
reporting instructions, and the chooser's [community support destination]
was reachable. On 2026-11-17, the maintainer saw all five forms and both
contact links in the chooser and accepted the shared PR template. Prefilling
the description of a new PR still needs direct observation. [Adoption record]

| Issue form | Default in this repository |
| --- | --- |
| Bug report | `bug` label |
| Feature request | `enhancement` label |
| Task | `Task` organization issue type |
| Chore | `chore` label |
| Epic | `Epic` organization issue type |

All three [repository labels] were present on 2026-11-23. The maintainer
confirmed both organization issue types and saw the expected defaults
preselected in the inherited forms on 2026-11-18. Automatic assignment on
submission and the chooser as seen by a signed-in non-maintainer remain
untested. [Form verification] · [Adoption record]

## Merging to main

Use a pull request with signed commits and resolve review conversations before
merging. Use a regular merge commit to preserve the branch history. The
[effective rules for main] and the [repository ruleset] showed the following
on 2026-11-23.

- The active ruleset targets only `refs/heads/main`. It requires signatures
  and a pull request, permits only merge commits, and blocks force pushes
  and branch deletion.
- The solo-maintainer configuration requires zero approving reviews and
  requires conversation resolution. Code Owner approval, last-push approval,
  and linear history are disabled.
- The [maintainer audit] recorded an empty bypass list. The public ruleset
  response on 2026-11-23 did not expose that list.

On 2026-11-25, the [ruleset inventory including organization rules] returned
one active repository ruleset, `main-protection` (ID `27461544`). The
[effective rules for main] contained no required status checks. A repository
admin can enable the merge check under the [governance baseline] as follows.

1. Let **Fixed TLV** finish on a pull request containing the gate. Verify
   the exact check-run name `fixed-tlv-quality`, conclusion `success` and
   source **GitHub Actions** (app ID `15368`) on that PR.
2. In **Settings → Rules → Rulesets → main-protection → Edit**, enable
   **Require status checks to pass**. Add `fixed-tlv-quality` with
   **GitHub Actions** as the expected source. Enable **Require branches to
   be up to date before merging** and save the active ruleset. This single
   context covers the worker jobs listed below.
3. Inspect the [effective rules for main]. The `required_status_checks`
   rule must show `strict_required_status_checks_policy: true` and a check
   with `context: fixed-tlv-quality` and `integration_id: 15368`. Verify on
   a PR without bypass that the merge is blocked while the gate is pending
   or failing and that a successful gate satisfies the check on the current
   revision. Verify the signatures on the merge commit and introduced commits.

## Continuous integration

The [Fixed TLV workflow] runs once per event. A PR tests the proposed merge
into `main`. A push tests the resulting `main` commit. Full CI runs on manual
dispatch for the selected ref and every Monday at 06:43 UTC on the default
branch. The release workflow calls the same Full CI jobs on its dispatch
commit. A newer PR run can cancel an earlier run for that PR. Each push
and Full run has its own concurrency group.

| Job and scope | PR to `main` | Push to `main` | Manual Full | Weekly Full |
| --- | --- | --- | --- | --- |
| `fixed-tlv` · GCC 13 and Clang 18 x64 Debug/Release contracts, Clang ASan/UBSan, GCC Debug Memcheck, public headers, coverage, Linux static/shared packages and symbols, bounded fuzzing | GCC headers | GCC headers | GCC + Clang headers | GCC + Clang headers |
| `windows-contracts` · MSVC Win32 Release and x64 Debug contracts, ABI and native-size boundaries | Run both | Run both | Run both | Run both |
| `resource-usage` · allocation probes and two clean footprint builds for each of static and shared linkage | Run both | Run both | Run both | Run both |
| `callgrind` · compare the candidate with the reviewed baseline for instruction counts and binary size, publish timing observations | Run | Run | Run | Run |
| `windows-package` · MSVC Win32 and x64 static/shared symbols and C99 and C++17 consumers of the build tree, original install prefix and relocated prefix | Skip | Skip | Run both | Run both |
| `first-party-analysis` · clang-format, clang-tidy, Cppcheck and deliberate C99/C++17 violation checks | Run | Run | Run | Run |
| `workflow-analyzers` · gate regression tests, signed release rehearsal, actionlint, zizmor | Offline audit | Online audit | Offline audit | Online audit |
| `fixed-tlv-quality` · aggregate result | Evaluate | Evaluate | Evaluate | Evaluate |

The performance and resource jobs run before merge so reviewers can inspect
regressions and proposed baseline updates. The Linux package check builds
the [source archive](release-artifacts.md) twice and tests static and shared
packages from the extracted files. Full CI retains the verified archive and
adds the second header compiler and Windows package checks. If one
resource or Windows contract matrix leg fails, its sibling can finish and publish its report.
Actions use full commit pins and read-only repository access. Checkout
disables credential persistence.

`fixed-tlv-quality` directly depends on all seven worker job IDs. Its
`always()` condition lets it inspect [GitHub's job results] after a worker
fails or skips. The gate requires `success` from every selected worker,
including all matrix legs. The only expected `skipped` result is
`windows-package` on a PR or push. A failed, cancelled, missing or
unexpected result fails the gate. Keep the workflow's `needs` list and
[gate policy](../tests/check_ci_gate.py) in sync when adding jobs.

Run `python3 -B -m unittest discover -s tests -p test_check_ci_gate.py`
to exercise successful, failed, skipped and cancelled results for each
trigger. For hosted validation, inspect the PR run and the subsequent push
run against the matrix. To run Full CI, choose **Actions → Fixed TLV → Run
workflow**, select the candidate branch and check the run's SHA. For
release evidence, record a successful Full run for the exact `main`
candidate and retain the artifacts listed in the
[Full CI handoff](quality.md#full-ci-and-release-evidence).

To reproduce the build checks, follow the [header and package checks]
and [fixed TLV test commands]. For the Full CI header check on Ubuntu 24.04,
run `CC=clang-18 CXX=clang++-18 python3 tests/check_public_api.py`.
To reproduce `windows-package`, run both package commands with the
`Visual Studio 17 2022` CMake generator and C++ build tools. Set
`CMAKE_GENERATOR_PLATFORM` to `Win32` for the first pair and `x64` for the second.

To check workflow changes locally on Ubuntu 24.04 x86-64, open Bash at the
repository root and run `export RUNNER_TEMP="$(mktemp -d)"`. Execute the
commands from **Check workflow syntax and expressions**, **Install zizmor**,
and **Audit workflow security offline** in the [Fixed TLV workflow]. These
steps verify the actionlint archive's SHA-256 before extraction and use pip
hash verification to install the zizmor wheel from
[the analyzer requirements](../.github/requirements-actions.txt).

## Release automation

The [Release workflow] uses the changelog extractor and signed-tag publisher
from `OpenTLV/.github` at `db813248c730dd29b217ad90d6a85a562ed98cd6`.
It checks out the release tools separately from the candidate. The
[organization release policy] defines the signing, tag protection and retry rules.

### Preview a candidate

1. Merge the candidate through the protected `main` branch. In
   `CHANGELOG.md`, put `## [<version>] - YYYY-MM-DD` above earlier
   releases and below the optional `## [Unreleased]` section. Use the
   complete SemVer version, such as `0.1.0-rc.1` or `0.1.0`. Its numeric
   major, minor and patch must match the candidate's CMake project version.
2. Choose **Actions → Release → Run workflow**, select `main`, enter that
   `version` without a leading `v`, and leave `publish` unchecked. To
   preview a specific reviewed commit, also set `candidate_sha` to its
   full SHA. Preparation checks that GitHub reports the ref as protected
   (`GITHUB_REF_PROTECTED`), that it is `refs/heads/main` and that any
   supplied SHA matches the dispatch. It extracts the first dated section
   and resolves relative links against that commit.
3. Review the version, candidate SHA and extracted notes in the run summary, then
   check the result of `fixed-tlv-quality`. The Release run calls the
   [Fixed TLV workflow] at that same SHA and runs the online zizmor audit.
   A direct manual Fixed TLV run uses the offline audit. The archive check builds
   and tests the source distribution for the requested version, including
   any prerelease suffix. Keep the [Full CI evidence](quality.md#full-ci-and-release-evidence).
   A preview ends after Full CI, before the signing key or publisher is used.

To rehearse locally, use Python 3.11 or newer, Git, GnuPG and a checkout
of `OpenTLV/.github` at the pinned commit above. From the candidate
repository, run the following in Bash with `release_tools` set to that
checkout's absolute path.

```bash
mkdir -p build/issue_104/tmp
TMPDIR="$PWD/build/issue_104/tmp" python3 -B tests/check_release_workflow.py "$release_tools"
```

The rehearsal uses disposable candidate commits, a local Git remote and
an in-memory Releases API. It checks changelog links, signed tag targets,
prerelease and final flags, and retries.

### Configure the publisher

On 2026-11-27 the [ruleset inventory including organization rules] listed
only `main-protection`. The [release environment endpoint] returned 404
to an unauthenticated request. Before the first publication, a repository
administrator must configure and verify the following controls.

- Restrict the `release` environment to the single **Branch** rule `main`.
  Leave the tag rule list empty. Install a dedicated release App on this
  repository and keep it off branch bypass lists. Allow the commit of
  `actions/create-github-app-token` pinned in the [Release workflow].
- In that environment set variables `RELEASE_APP_CLIENT_ID` and
  `RELEASE_SIGNING_FINGERPRINT`, and secrets `RELEASE_APP_PRIVATE_KEY`
  (App PEM) and `RELEASE_SIGNING_KEY` (armored OpenPGP secret key).
  Record the approved signing fingerprint, key owner and public verification
  key independently of the workflow. The release App needs **Contents** and
  **Workflows** write permissions so it can tag the tested commit even if
  workflow files on `main` change during the run. The workflow's default
  token has read-only contents permission.
- Apply the two active tag rulesets in the [organization release policy].
  Restrict updates and deletions on all tags with an empty bypass list.
  Restrict creation of `v*` tags with only the installed release App as an
  **Always allow** bypass actor. Keep both exclusion lists empty and verify
  the effective rules with an account that can inspect inherited rules and
  bypass actors.

### Publish or retry

When the release is approved, dispatch **Release** on `main` with the
matching `version`, the approved full commit SHA in `candidate_sha`, and
`publish` checked. Preparation stops before Full CI if this SHA is missing
or if `main` advanced between approval and dispatch. A matching candidate
runs Full CI again. Publication uses a single concurrency group for all
versions. Only its `publish` job enters the protected `release` environment.

The publisher signs an annotated `v<version>` tag, verifies its signer,
embedded tag name and direct commit target, then pushes the verified
object to GitHub. The GitHub Release title is `OpenTLV <version>` and
its body comes from the dated changelog entry. A SemVer prerelease
component sets GitHub's prerelease flag. A version with only build
metadata is a final release.

After an interruption, choose **Re-run failed jobs** on the original run
to retain its candidate SHA even if `main` has advanced. The publisher
reuses a matching signed tag and creates a missing Release. If both
already exist, it checks the tag and the Release's title, body and flags.
A conflicting tag or Release stops publication for maintainer review.

For `0.1.0`, complete [artifact integrity and provenance](https://github.com/OpenTLV/opentlv/issues/105)
and [hosted CI and readiness evidence](https://github.com/OpenTLV/opentlv/issues/106)
before the [final publication](https://github.com/OpenTLV/opentlv/issues/107).

## CodeQL coverage

The [maintainer audit] recorded the assignment of `OpenTLV-org-config-1`
to this repository. GitHub runs CodeQL through default setup. On
2026-11-25, the [CodeQL run on main] at `2ff402e` completed analysis for
GitHub Actions and Python. Its job list contained no C/C++ analysis.
Public API requests for the [default setup configuration], attached
[organization security configuration] and [analysis results] required
authentication. The effective organization settings, C/C++ findings and
dismissals could not be verified in this audit.

A repository admin, security manager or organization owner can enable C/C++
in **Settings → Advanced Security → CodeQL analysis → View CodeQL
configuration → Edit**. Under **Languages**, add C/C++ alongside the enabled
languages and save. Saving starts an analysis. If an enforced organization
configuration controls this setting, its owner must make the change.
Default setup uses build mode `none` for C/C++ and infers include paths
and compiler flags. [GitHub's default setup instructions] · [C/C++ build modes]

For v0.1.0 release evidence, record the analyzed `main` commit, the
successful C/C++ analysis run and its query suite. In **Security and
quality → Code scanning → Tool status**, download the analyzed-files CSV
and check extraction for `src/core/`, `src/io/` and the public headers in
`include/opentlv/`.
Record missing coverage and extraction warnings. Review alerts and
dismissals on the same revision, including the rationale for any accepted
finding. Keep sensitive details in the private reporting channel described
in the [security policy]. The managed CodeQL run has its own status outside
`fixed-tlv-quality`. [Tool status and file coverage]

[CodeQL run on main]: https://github.com/OpenTLV/opentlv/actions/runs/45643501512
[default setup configuration]: https://api.github.com/repos/OpenTLV/opentlv/code-scanning/default-setup
[organization security configuration]: https://api.github.com/repos/OpenTLV/opentlv/code-security-configuration
[analysis results]: https://api.github.com/repos/OpenTLV/opentlv/code-scanning/analyses
[GitHub's default setup instructions]: https://docs.github.com/en/code-security/how-tos/find-and-fix-code-vulnerabilities/manage-your-configuration/edit-default-setup
[C/C++ build modes]: https://docs.github.com/en/code-security/reference/code-scanning/codeql/build-options-for-compiled-languages
[Tool status and file coverage]: https://docs.github.com/en/code-security/how-tos/find-and-fix-code-vulnerabilities/manage-your-configuration/use-the-tools-status-page-for-code-scanning

## Actions and dependency security

The [maintainer audit] recorded the following repository settings by
2026-11-17. The Actions and Dependabot settings require authenticated access
for a fresh inspection.

| Control | Verified setting |
| --- | --- |
| Allowed actions | Organization actions plus `actions/checkout@*` and `actions/setup-python@*`. Actions must be pinned to full commit SHAs. |
| Default `GITHUB_TOKEN` | Read access to contents and packages. The option allowing GitHub Actions to create or approve pull requests is disabled. |
| Fork workflows | All external contributors require approval. |
| Dependency security | Dependency graph, Dependabot alerts, and Dependabot security updates enabled. |

On 2026-11-23, GitHub returned the repository's [dependency graph export] and
reported [private vulnerability reporting] enabled. The public [Advisories page]
displayed **Report a vulnerability**. Opening the form as a signed-in
non-maintainer remains unverified. [Reporting verification]

[adoption guide]: https://github.com/OpenTLV/.github/blob/main/docs/adoption.md
[Community Standards]: https://github.com/OpenTLV/opentlv/community
[security policy]: https://github.com/OpenTLV/opentlv/security/policy
[community support destination]: https://github.com/OpenTLV/opentlv/discussions/categories/q-a
[Adoption record]: https://github.com/OpenTLV/.github/blob/main/docs/adoption.md#follow-up-evidence-and-remaining-checks
[repository labels]: https://github.com/OpenTLV/opentlv/labels
[Form verification]: https://github.com/OpenTLV/.github/issues/42
[effective rules for main]: https://api.github.com/repos/OpenTLV/opentlv/rules/branches/main
[ruleset inventory including organization rules]: https://api.github.com/repos/OpenTLV/opentlv/rulesets?includes_parents=true
[repository ruleset]: https://github.com/OpenTLV/opentlv/rules/27461544
[maintainer audit]: https://github.com/OpenTLV/.github/issues/10
[governance baseline]: https://github.com/OpenTLV/.github/blob/main/docs/governance.md
[Fixed TLV workflow]: ../.github/workflows/fixed-tlv.yml
[Release workflow]: ../.github/workflows/release.yml
[release environment endpoint]: https://api.github.com/repos/OpenTLV/opentlv/environments/release
[organization release policy]: https://github.com/OpenTLV/.github/blob/db813248c730dd29b217ad90d6a85a562ed98cd6/docs/releases.md
[GitHub's job results]: https://docs.github.com/en/actions/reference/workflows-and-actions/contexts#needs-context
[header and package checks]: architecture.md#verification
[fixed TLV test commands]: testing.md
[dependency graph export]: https://api.github.com/repos/OpenTLV/opentlv/dependency-graph/sbom
[private vulnerability reporting]: https://api.github.com/repos/OpenTLV/opentlv/private-vulnerability-reporting
[Advisories page]: https://github.com/OpenTLV/opentlv/security/advisories
[Reporting verification]: https://github.com/OpenTLV/.github/issues/28
