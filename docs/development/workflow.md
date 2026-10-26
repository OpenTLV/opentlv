# Development workflow

OpenTLV uses `main` and short-lived feature branches. Create a feature branch,
implement and test the change, update `CHANGELOG.md` under `Unreleased`, and
submit a pull request to `main`.

Core uses **Pull Request CI**, **Main CI**, **Full CI**, and a **Release Gate**. See
[Core quality gates](quality-gates.md) for the exact check inventory, triggers,
release artifacts, provenance verification and timing evidence.

Pull Request CI runs only on Linux for PRs targeting `main`, including clang-tidy,
cppcheck, CodeQL and essential tests. Main pushes run only Linux build/integration
smoke checks and CodeQL, which closes fixed code-scanning alerts on `main`. Full CI runs manually or through Release Gate and repeats all
required source checks alongside the complete native and memory-safety matrix.
Development branch pushes do not start a duplicate Core PR run.
Run **Core Release** manually from `main` with its single **tag** input, such as
`v0.1.0` or `v0.1.0-rc.1`. The latest changelog section must match that exact tag,
include a valid date and changes, and contain no `Unreleased` anywhere in the file.
The workflow runs Full CI for the exact dispatch commit, compares benchmarks and
Callgrind against the locked previous stable release, and validates packages and
attestations before creating the tag and release. Performance changes are initially
advisory; missing or failed verification blocks publication. Do not push the tag
in advance. Automatic performance measurements run only through Release Gate.

Binding and documentation workflows retain their independent responsibilities.
Documentation publication remains owned by the standalone portal; see
[Publishing](../../CONTRIBUTING.md#publishing). The existing
[Query release checklist](query-release.md) still applies to the 1.0 contract.

For local formatting, static analysis and documentation validation, see
[CONTRIBUTING.md](../../CONTRIBUTING.md).
