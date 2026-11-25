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

The 2026-11-23 audit found no required status checks. Before configuring
required checks, verify the `fixed-tlv` and `workflow-analyzers` check names
and source app ID on a pull request, then follow the [governance baseline]
for requiring them and an up-to-date branch. Verify the signatures on the
merge commit and the introduced commits on GitHub.

## Continuous integration

The [Fixed TLV workflow] runs these checks on every pull request to `main`
and every push to `main`.

- `fixed-tlv` compiles the public headers, runs CTest in Debug, Release and
  sanitizer builds, compares two GCC coverage reports, tests build-tree and
  relocated packages for static and shared libraries, and runs the bounded
  fuzz targets on Ubuntu 24.04.
- `callgrind` runs the [instruction-count probe](testing.md#linux-callgrind)
  twice on Ubuntu 24.04 x86-64 and checks that each workload reproduces its count.
- `resource-usage` checks for heap allocation during Fixed TLV calls and
  compares footprint reports from two clean Release builds for each of the
  static and shared configurations. [Allocation and footprint checks](testing.md#heap-allocation-and-binary-size)
- `windows-x86` builds and executes the Win32 test suites with MSVC and
  verifies the [x86 ABI and native-size boundary cases](testing.md#32-bit-x86).
- `workflow-analyzers` checks the workflow files with actionlint and zizmor.
  Pull requests and manual runs use zizmor offline. Pushes to `main` and
  scheduled runs use a read-only token in the zizmor step to check action
  pins against GitHub. All jobs use read-only repository access, and checkout
  disables credential persistence.

Full CI runs every Monday at 06:43 UTC. To run it manually, open **Actions**,
select **Fixed TLV**, choose **Run workflow**, and select the branch to check.
Full CI adds a Clang 18 public-header check to `fixed-tlv`. It also runs
`windows-package`, which builds static and shared packages with MSVC and tests
their C and C++ consumers against the build tree and a relocated installation.
For a release candidate, verify that all jobs passed for the exact `main`
commit.

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
[repository ruleset]: https://github.com/OpenTLV/opentlv/rules/27461544
[maintainer audit]: https://github.com/OpenTLV/.github/issues/10
[governance baseline]: https://github.com/OpenTLV/.github/blob/main/docs/governance.md
[Fixed TLV workflow]: ../.github/workflows/fixed-tlv.yml
[header and package checks]: architecture.md#verification
[fixed TLV test commands]: testing.md
[dependency graph export]: https://api.github.com/repos/OpenTLV/opentlv/dependency-graph/sbom
[private vulnerability reporting]: https://api.github.com/repos/OpenTLV/opentlv/private-vulnerability-reporting
[Advisories page]: https://github.com/OpenTLV/opentlv/security/advisories
[Reporting verification]: https://github.com/OpenTLV/.github/issues/28
