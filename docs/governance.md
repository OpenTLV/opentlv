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
branch. A newer PR run can cancel an earlier run for that PR. Each push and
Full run has its own concurrency group.

| Job and scope | PR to `main` | Push to `main` | Manual Full | Weekly Full |
| --- | --- | --- | --- | --- |
| `fixed-tlv` · GCC 13 and Clang 18 x64 Debug/Release contracts, ASan/UBSan with Clang Debug, public headers, coverage, Linux static/shared packages and symbols, bounded fuzzing | GCC headers | GCC headers | GCC + Clang headers | GCC + Clang headers |
| `windows-contracts` · MSVC Win32 Release and x64 Debug contracts, ABI and native-size boundaries | Run both | Run both | Run both | Run both |
| `resource-usage` · allocation probes and two clean footprint builds for each of static and shared linkage | Run both | Run both | Run both | Run both |
| `callgrind` · compare the candidate with the reviewed baseline for instruction counts and binary size, publish timing observations | Run | Run | Run | Run |
| `windows-package` · MSVC static/shared C99 and C++17 consumers of the build tree and relocated installation | Skip | Skip | Run | Run |
| `first-party-analysis` · clang-format, clang-tidy, Cppcheck and deliberate C99/C++17 violation checks | Run | Run | Run | Run |
| `workflow-analyzers` · gate regression tests, actionlint, zizmor | Offline audit | Online audit | Offline audit | Online audit |
| `fixed-tlv-quality` · aggregate result | Evaluate | Evaluate | Evaluate | Evaluate |

The performance and resource jobs run before merge so reviewers can inspect
regressions and proposed baseline updates. Full CI adds the second header
compiler and Windows package checks within the same workflow. If one
resource or Windows contract matrix leg fails, its sibling can finish and publish its report.
Actions use full commit pins and read-only repository access. Checkout
disables credential persistence.

`fixed-tlv-quality` directly depends on all seven worker job IDs. Its
`always()` condition lets it inspect [GitHub's job results] after a worker
fails or skips. The gate requires `success` from every selected worker,
including both matrix jobs. The only expected `skipped` result is
`windows-package` on a PR or push. A failed, cancelled, missing or
unexpected result fails the gate. Keep the workflow's `needs` list and
[gate policy](../tests/check_ci_gate.py) in sync when adding jobs.

Run `python3 -B -m unittest discover -s tests -p test_check_ci_gate.py`
to exercise successful, failed, skipped and cancelled results for each
trigger. For hosted validation, inspect the PR run and the subsequent push
run against the matrix. To run Full CI, choose **Actions → Fixed TLV → Run
workflow**, select the candidate branch and check the run's SHA. For
release evidence, record a successful Full run for the exact `main`
candidate, including its [performance and resource artifacts](performance-baseline.md#hand-off-release-evidence).
Record the `first-party-analysis` result from that run alongside those artifacts.
Its [local reproduction steps](testing.md#formatting-and-static-analysis) use
the same tool versions and negative checks.

To reproduce the build checks, follow the [header and package checks]
and [fixed TLV test commands]. For the Full CI header check on Ubuntu 24.04,
run `CC=clang-18 CXX=clang++-18 python3 tests/check_public_api.py`.
To reproduce `windows-package`, run both package commands on Windows
with the `Visual Studio 17 2022` CMake generator and the C++ build tools.

To check workflow changes locally on Ubuntu 24.04 x86-64, open Bash at the
repository root and run `export RUNNER_TEMP="$(mktemp -d)"`. Execute the
commands from **Check workflow syntax and expressions**, **Install zizmor**,
and **Audit workflow security offline** in the [Fixed TLV workflow]. These
steps verify the actionlint archive's SHA-256 before extraction and use pip
hash verification to install the zizmor wheel from
[the analyzer requirements](../.github/requirements-actions.txt).

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
[GitHub's job results]: https://docs.github.com/en/actions/reference/workflows-and-actions/contexts#needs-context
[header and package checks]: architecture.md#verification
[fixed TLV test commands]: testing.md
[dependency graph export]: https://api.github.com/repos/OpenTLV/opentlv/dependency-graph/sbom
[private vulnerability reporting]: https://api.github.com/repos/OpenTLV/opentlv/private-vulnerability-reporting
[Advisories page]: https://github.com/OpenTLV/opentlv/security/advisories
[Reporting verification]: https://github.com/OpenTLV/.github/issues/28
