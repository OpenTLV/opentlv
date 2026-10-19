# Development workflow

OpenTLV uses `main` and short-lived feature branches. Create a feature branch,
implement and test the change, update `CHANGELOG.md` under `Unreleased`, and
submit a pull request to `main`.

Core now uses **Fast CI**, **Full CI**, and a **Release Quality Gate**. See
[Core quality gates](quality-gates.md) for the exact check inventory, triggers,
release artifacts, provenance verification and timing evidence.

Fast CI runs only on Linux for PRs and development branch pushes without an open
PR. It requires clang-tidy and cppcheck on every run before merge. Main pushes,
scheduled runs and manual Full CI requests execute the Full matrix without
repeating these two analyzers. Formatting/contract checks and CodeQL run in both
tiers.
Run **Core Release** manually from `main` with its single **tag** input, such as
`v0.1.0` or `v0.1.0-rc.1`. The latest changelog section must match that exact tag,
include a valid date and changes, and contain no `Unreleased` anywhere in the file.
The workflow runs Full CI for the exact dispatch commit before packaging and
publication, then creates the tag and release. Do not push the tag in advance.

Binding and documentation workflows retain their independent responsibilities.
Documentation publication remains owned by the standalone portal; see
[Publishing](../../CONTRIBUTING.md#publishing). The existing
[Query release checklist](query-release.md) still applies to the 1.0 contract.

For local formatting, static analysis and documentation validation, see
[CONTRIBUTING.md](../../CONTRIBUTING.md).
