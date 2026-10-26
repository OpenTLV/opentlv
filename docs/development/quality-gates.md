# Core quality gates

Core owns C/C++ behavior, standard conformance, memory safety and public build
contracts. Independent CLI and Lua repositories own their own tests. Existing
repository-local binding and documentation workflows remain separate; this change
does not replace their coverage or the Query 1.0 acceptance contract.

## Entry points

| Entry point | Trigger | Required checks |
| --- | --- | --- |
| `core-pr.yml` | PR to `main` | Linux only: Clang Debug unit/integration tests, formatting/contract checks, clang-tidy, cppcheck, one shared C/C++ public consumer configuration, CodeQL |
| `core-main.yml` | Push to `main` | One Linux shared C/C++ build and external consumer integration smoke configuration |
| `core-full.yml` | Manual dispatch or reusable call from Release Gate | The complete Full matrix below, including all required static checks |
| `core-release.yml` | Manual dispatch from `main`, one `tag` input such as `v0.1.0` | Closed changelog, locked candidate and stable baseline, Full CI and performance comparison in the same run, verified artifacts, checksums, attestations and publication |

Core Pull Request CI replaces Core Fast CI. It has no branch-push trigger, so an
open PR has one Core validation run per update. Main CI checks integration health
after merge without repeating static analysis, unit suites or platform matrices.
Full CI has no push or scheduled trigger. Reusable workers still support manual
diagnosis, but a standalone worker pass does not produce Full CI evidence.
Manual **Static Analysis** runs all three source checks.

PR and Main CI use Linux runners exclusively. Windows verification belongs to Full CI;
macOS is not part of the Core PR matrix. Independent binding workflows retain
their existing platform policies and are outside this issue's scope.

`core-checks.yml` owns the job inventory. Its always-running `Core quality gate`
rejects failures, cancellations, missing jobs and unexpected skips. Only explicit
Full-only top-level jobs may be skipped in PR CI. Its artifact identifies the
checkout SHA, repository, workflow run and attempt. It retains the exact worker
workflow definitions, their hashes, resolved tier/release-tag inputs and hashes
of configuration helpers. These describe the declared tested configurations;
binary package records additionally capture actual compiler and build metadata.
The nested `Static analysis gate` requires pre-commit, clang-tidy and cppcheck
to succeed in both PR and Full CI, without path filters. Main has its own
always-running gate requiring its exact smoke job to succeed. Release reruns
every Full configuration for its exact candidate; historical PR results cannot
substitute for this evidence.

| Full verification | Configuration |
| --- | --- |
| Native builds | Clang Debug/Release and C-only examples; GCC C++11 Debug/C++23 Release; MSVC x64/x86 C++11 Debug/C++23 Release |
| Coverage | Clang Debug unit and integration tests |
| Source checks | Formatting/contract checks, clang-tidy 18, cppcheck 2.21.0 and security-extended CodeQL |
| Public consumers | Linux/Windows, shared/static, C-only/C++ through FetchContent and relocated installations |
| Capability contracts | Shared and static profiles |
| Properties | Default and minimal capabilities, 1,000 deterministic cases |
| Sanitizers and Query | ASan/UBSan Core suites, Query conformance/adversarial evidence, native x86 boundaries, MSan campaigns, eight frontend/set/C++ plan configurations |
| Fuzzing | All existing C API targets with saved seed corpora and bounded ASan/UBSan campaigns |
| Memcheck | Complete native selection, excluding tests labelled `no-memcheck` |
| ABI | Existing libabigail policy and ABI-checker mutation self-tests |

ABI incompatibility remains advisory before 1.0 under the existing
[ABI policy](abi-compatibility.md). ABI checker mutation self-tests are not a
general Core source mutation campaign; no such campaign is claimed by this gate.
Benchmarks and Callgrind run automatically only through Release Gate. Performance
changes are advisory; missing, failed or incomparable measurements block release.

## CodeQL analysis scope

CodeQL analyzes the C/C++ translation units its tracer sees during the build:
OpenTLV production sources, first-party tests and, in Full CI, examples.
GoogleTest is fetched third-party test infrastructure and is excluded by
compiling its `gtest`, `gtest_main`, `gmock` and `gmock_main` targets before
CodeQL initialization. The traced build then reuses those objects and fails if it
compiles any fetched source under `build/_deps/`. GoogleTest headers included by
first-party tests are still extracted with those tests.

The exclusion is a build boundary, not a result filter. Query suites, rules and
first-party findings are not suppressed: PR CI keeps the default suite, Full CI
keeps `security-extended`, and `paths-ignore` is not used because it does not
control C/C++ extraction. GoogleTest sources are not modified. Findings reported
in remaining third-party headers are triaged individually.

Disabling `OPENTLV_BUILD_TESTS` for CodeQL was rejected. Measured on the CodeQL
configurations, it would remove every first-party test, including all C++
translation units in PR CI, the only ones that analyze the header-only `tlv++`
layer there:

| Configuration | Production | First-party tests | Examples | GoogleTest |
| --- | --- | --- | --- | --- |
| PR, before | 89 | 150 | 0 | 4 |
| PR, tests disabled | 89 | 0 | 0 | 0 |
| PR, current | 89 | 150 | 0 | 0 |
| Full, before | 89 | 150 | 36 | 4 |
| Full, tests disabled | 89 | 0 | 36 | 0 |
| Full, current | 89 | 150 | 36 | 0 |

Counts are unique compiled source files. First-party test coverage is unchanged.

## Release artifacts

1. Close `CHANGELOG.md` for the intended tag and merge it to `main`. Its first
   release heading must be exactly `## [v0.1.0] - YYYY-MM-DD` for tag `v0.1.0`,
   with a valid date and at least one change. No occurrence of `Unreleased` is
   allowed anywhere, including an empty heading or `**Unreleased**`. The gate
   checks this before starting Full CI. Keep historical release sections intact.
2. For 1.0.0 and later, including prereleases, first run the existing
   [Query release candidate gate](query-release.md) for the exact candidate SHA.
   The latest manual candidate run for that SHA must succeed. Its facade evidence
   requirement is preserved; an inventory report is insufficient. Preparing that
   evidence remains the existing Query process.
3. Run **Core Release** with branch **main** and its only input **tag**, for
   example `v0.1.0` or `v0.1.0-rc.1`. Unprefixed values are rejected. Do not create
   the tag beforehand: existing tags are rejected, even at the same commit.
   The workflow uses the exact SHA selected at dispatch, even if main advances.
4. Preflight locks the candidate and the previous published stable release's tag,
   release ID and exact commit. Core Release calls Full CI for the candidate with
   a private local candidate tag in each build checkout. It then compares
   performance, builds/tests/packages approved libraries, validates a source
   extraction and creates attestations.
5. Publication revalidates the original candidate and baseline lock, the exact
   Full CI run/attempt, performance evidence, artifact completeness and hashes,
   and verifies each producer attestation. Only then
   does it push the new `v`-prefixed tag and create a GitHub release; prerelease
   tags create prereleases. Library version numbers and asset names omit `v`.

There is no skip-verification input, alternate SHA input or publication from a PR
workflow. Existing tags and release assets are not replaced. An external-service
failure after the tag push can leave a tag without a release or a partial release;
inspect it and recover manually. A new dispatch rejects an already-created tag.
Rerun all jobs when retrying verification: artifacts are scoped to the run attempt,
so evidence from an earlier attempt cannot authorize publishing.

Historical unprefixed tags remain readable by CMake; all new release inputs use
the `v` prefix. During development, keep the usual `Unreleased` changelog section
and close it only when preparing the release commit.

The initial binary matrix is Linux x64 (Ubuntu 24.04/GCC) and Windows x64
(Windows 2022/MSVC), each shared and static, Release configuration, including C++11
headers and default Core capabilities. Windows packages use the dynamic MSVC
runtime (`MultiThreadedDLL`). These packages do not promise compatibility with
every Linux libc or Windows toolchain. The manifest records the compiler, runtime,
runner image and Core options. Full CI also tests x86, but x86 and macOS are not published binary
configurations in this issue.

Each release contains:

- `OpenTLV-<version>-source.tar.gz`: exact Git archive plus `opentlv-source.txt`
  identifying the release version and full commit. CMake validates this data and
  preserves the version without `.git`, even inside an unrelated parent checkout.
- Four `OpenTLV-<version>-<os>-x64-<linkage>` ZIP/TGZ packages and their JSON
  configuration records. Packaged libraries are tested before packaging; canonical
  install contents and relocated C/C++ consumers are verified afterward.
- `performance.json` and `performance.md`: exact candidate/baseline identity,
  measured comparisons, configuration and toolchain metadata.
- `performance-data.tar.gz`: raw benchmark samples, Callgrind profiles, immutable
  workload sources, source manifests, compilation metadata and logs.
- `release-manifest.json`: source/binary/performance digests and the same-run
  Full CI and performance evidence.
- `SHA256SUMS`: digests for archives, configuration records and manifest.

Use the named `-source.tar.gz` release asset for Verified Source consumption.
GitHub's automatically generated source ZIP/TGZ links do not contain the injected
archive metadata and are not substitutes for this verified artifact.

GitHub stores signed provenance for every published file, including the manifest
and checksum list. Consumers should check both checksums and provenance:

```sh
sha256sum --check SHA256SUMS
gh attestation verify OpenTLV-0.8.0-source.tar.gz --repo OpenTLV/opentlv \
  --signer-workflow OpenTLV/opentlv/.github/workflows/core-release.yml \
  --source-digest <full-release-commit-sha>
```

A checksum alone does not authenticate a producer. See GitHub's
[artifact attestation documentation](https://docs.github.com/en/actions/concepts/security/artifact-attestations).
Downstream consumption is owned by downstream issues.

## Repository settings and publication boundary

The workflow prevents its publisher from running without required checks.
Administrators can still change workflows, tags, permissions or publish manually;
workflow YAML cannot prohibit those actions. Configure release-tag rules and
publisher access in repository settings when enabling production publication.
Do not claim administrator-proof enforcement from the workflow alone.

The live ruleset inspected on 2026-10-19 requires CodeQL scanning on `main`, with no
bypass actors. CodeQL runs in PR and Full CI with the existing analysis
category. No repository settings were changed by this implementation. If requiring
CI status checks for merges, select the new always-running Core quality gate after
its first hosted PR run; do not require Full-only worker checks.

Documentation publication remains owned by the standalone portal. Binding CI,
documentation validation and Core-to-Docs notifications remain independent.
The publisher uses `GITHUB_TOKEN`; its tag push does not trigger other workflows.
Any separate binding or portal release must follow that component's own process.
A Core library gate pass is not a claim that every independent binding or the
documentation portal has passed.

## Release performance verification

The previous approved stable release means the highest published, non-draft,
non-prerelease SemVer release below the candidate's base version. Historical
unprefixed tags are accepted. Preflight resolves its tag to a commit, verifies
remote identity and candidate ancestry, and freezes the selection for this run.
Publication rechecks that same release ID and tag; a newer release published
meanwhile does not silently replace it. A missing stable baseline or incompatible
workload API fails verification rather than falling back to `main` or the candidate.

Both revisions are extracted from exact Git commits and measured on one runner
using the stable release's identical workload sources, a pinned Google Benchmark
revision and identical Clang 18 Release C99/C++11 static configurations. Seven
complete benchmark passes alternate baseline/candidate order. Median CPU times
and observed ranges distinguish improvements, regressions and overlapping-range
noise; these descriptive ranges are not statistical confidence intervals.
All 16 Callgrind workloads compare instruction counts for identical inputs and
2,000 iterations, with output/checksum and capability parity checks.

No percentage threshold blocks an initial release. Measurement/build failures,
missing samples, changed workload inventories, invalid values or mismatched
identity do block publication. Strict budgets and approved exceptions remain
future policy work. Neither PR, Main, standalone Full nor branch/tag pushes run
performance measurements. Local benchmark and Callgrind tools remain available.

Every release publishes checksummed, attested raw and summarized results. Keep
these version-associated assets unchanged. Legacy stable releases without such
assets are remeasured from their frozen source revision and explicitly identified
as the baseline of the new report; no historical measurement is invented. Future
comparisons also remeasure the previous stable source alongside the candidate to
avoid treating timing data from different machines as equivalent.

## CI execution timings

The [baseline snapshot](ci-baseline.json) contains actual GitHub job timestamps and
run URLs for the pre-change main commit
`1ed4e0f63360755472bdebee1fa1e1609b190e98`. That commit changed documentation and
does not represent the full native-change matrix. Do not compare it directly to
a native PR or to the comprehensive Full CI and claim an improvement.

```sh
python -B scripts/ci_timings.py --baseline docs/development/ci-baseline.json \
  --output build/ci-baseline-summary.json
```

PR and Full upload `core-timings-<sha>-<attempt>` for their Core checks. Main and
Release additionally collect their complete attempts as
`core-main-timings-<sha>-<attempt>` and `core-release-timings-<sha>-<attempt>`.
Collectors run after success or failure. They record job duration totals and the execution span from
the first job start to the last check completion, including intervening scheduling
gaps but excluding initial queue time. These are runner-time observations, not
billed-minute estimates. Collection cannot turn failed checks into success.

Compare multiple runs with the same event, changed-component cohort, toolchain,
cache state and runner class. Report PR feedback latency separately from Main/Full
verification cost. Post-change hosted timings and a measured before/after comparison
remain required before closing the performance acceptance criterion.

## Local checks

```sh
python -B scripts/test_core_release.py
python -B scripts/test_core_performance.py
python -B scripts/test_ci_workflows.py
actionlint
pre-commit run --all-files
```

Regression tests reject incomplete, stale and modified evidence and verify source
archive round trips and CMake metadata validation. Local checks cannot exercise
GitHub OIDC signing, hosted permissions or publication. Validate PR/Main/Full runs and
a controlled prerelease on GitHub before declaring the release gate operational.
