# Core quality gates

Core owns C/C++ behavior, standard conformance, memory safety and public build
contracts. Independent CLI and Lua repositories own their own tests. Existing
repository-local binding and documentation workflows remain separate; this change
does not replace their coverage or the Query 1.0 acceptance contract.

## Entry points

| Entry point | Trigger | Required checks |
| --- | --- | --- |
| `core-fast.yml` | PR to `main`; development branch push without an open PR to `main` | Linux only: Clang Debug unit/integration tests, formatting/contract checks, clang-tidy and cppcheck on every run, one shared C/C++ public consumer configuration, CodeQL |
| `core-full.yml` | Push to `main`, Sunday 02:17 UTC, manual dispatch | The complete Full matrix below; clang-tidy and cppcheck run only in Fast |
| `core-release.yml` | Manual dispatch from `main`, one `tag` input such as `v0.1.0` | Closed changelog, candidate identity, Full CI in the same run, verified source and binary assembly, checksums, producer attestations, publication |

Fast branch-push runs check for an existing PR before starting builds, avoiding
the previous property-suite push/PR duplication. Main uses Full CI. The clang-tidy
and cppcheck analyzers run in Fast before merge and are not repeated in Full or
release runs. Opening a PR just
after a branch push can still produce two runs; concurrency cancels superseded
runs for the same ref. Reusable workers support manual diagnosis, but a standalone
worker pass does not produce Full CI evidence. Manual **Static Analysis** runs
default to Fast checks; selecting `full` runs only its pre-commit checks.

Fast CI uses Linux runners exclusively. Windows verification belongs to Full CI;
macOS is not part of the Core Fast matrix. Independent binding workflows retain
their existing platform policies and are outside this issue's scope.

`core-checks.yml` owns the job inventory. Its always-running `Core quality gate`
rejects failures, cancellations, missing jobs and unexpected skips. Only explicit
Full-only top-level jobs may be skipped in Fast CI. Its artifact identifies the
checkout SHA, repository, workflow run and attempt. The nested `Static analysis
gate` requires pre-commit, clang-tidy and cppcheck to succeed in Fast, without path
filters. In Full it requires pre-commit to succeed and both analyzers to be
skipped. Full and release evidence therefore excludes these two analyzers; it
does not import an earlier Fast result. Every selected Full configuration runs,
including on manually selected refs.

| Full verification | Configuration |
| --- | --- |
| Native builds | Clang Debug/Release and C-only examples; GCC C++11 Debug/C++23 Release; MSVC x64/x86 C++11 Debug/C++23 Release |
| Coverage | Clang Debug unit and integration tests |
| Source checks | Formatting/contract checks and security-extended CodeQL; clang-tidy and cppcheck run only in Fast before merge |
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
Benchmarks and Callgrind remain independent advisory workflows. Noisy timing
thresholds do not become release blockers.

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
4. Core Release runs Full CI for that commit with a private local candidate tag
   in each build checkout. It then builds, tests and packages approved libraries,
   validates a source extraction and creates attestations.
5. Publication revalidates identity, the exact Full CI run/attempt, artifact
   completeness and hashes, and verifies each producer attestation. Only then
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
- `release-manifest.json`: source/binary digests and the same-run Full CI evidence.
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
bypass actors. CodeQL remains in Fast and Full CI with the existing analysis
category. No repository settings were changed by this implementation. If requiring
CI status checks for merges, select the new always-running Core quality gate after
its first hosted PR run; do not require Full-only worker checks.

Documentation publication remains owned by the standalone portal. Binding CI,
documentation validation and Core-to-Docs notifications remain independent.
The publisher uses `GITHUB_TOKEN`; its tag push does not trigger other workflows.
Any separate binding or portal release must follow that component's own process.
A Core library gate pass is not a claim that every independent binding or the
documentation portal has passed.

## Performance evidence

The [baseline snapshot](ci-baseline.json) contains actual GitHub job timestamps and
run URLs for the pre-change main commit
`1ed4e0f63360755472bdebee1fa1e1609b190e98`. That commit changed documentation and
does not represent the full native-change matrix. Do not compare it directly to
a native PR or to the new comprehensive Full CI and claim an improvement.

```sh
python -B scripts/ci_timings.py --baseline docs/development/ci-baseline.json \
  --output build/ci-baseline-summary.json
```

Each orchestrated run uploads `core-timings-<sha>-<attempt>` after its checks,
including failed runs. It records job duration totals and the execution span from
the first job start to the last check completion, including intervening scheduling
gaps but excluding initial queue time. These are runner-time observations, not
billed-minute estimates. Collection cannot turn failed checks into success.

Compare multiple runs with the same event, changed-component cohort, toolchain,
cache state and runner class. Report Fast feedback latency separately from Full
verification cost. Post-change hosted timings and a measured before/after comparison
remain required before closing the performance acceptance criterion.

## Local checks

```sh
python -B scripts/test_core_release.py
actionlint
pre-commit run --all-files
```

Regression tests reject incomplete, stale and modified evidence and verify source
archive round trips and CMake metadata validation. Local checks cannot exercise
GitHub OIDC signing, hosted permissions or publication. Validate Fast/Full runs and
a controlled prerelease on GitHub before declaring the release gate operational.
