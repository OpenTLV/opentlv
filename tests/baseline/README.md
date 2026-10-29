# Pre-refactor behavioral baseline

This is the evidence package for
[#645](https://github.com/OpenTLV/opentlv/issues/645), captured from the published
[Core v0.8.0 release](https://github.com/OpenTLV/opentlv/releases/tag/v0.8.0)
at **`345c77acb29266036df5f27d04ce315bb61853cc`**. Binding separation and CI
normalization are included in that release. The subsequent #644 module contract
documentation is not part of the reference source.

The [release run](https://github.com/OpenTLV/opentlv/actions/runs/40500582050)
invoked `core-full.yml` before publishing. Its preserved
[release manifest](v0.8.0/release-manifest.json) records `tier: full`, the exact
commit, configurations, workflow definitions and all twelve successful quality
checks. This is the enclosing **Core Release** run, not a separately dispatched
Full CI run. [Release metadata](v0.8.0/release.json) and
[run metadata](v0.8.0/ci-run.json) were read from GitHub when capturing this package.
These snapshots and their checksums are provenance records, not a new attestation.

**Approval pending.** Reviewers must explicitly approve this baseline on #645 or
its pull request before [#646](https://github.com/OpenTLV/opentlv/issues/646)
(Issue 3) begins. Successful release CI, local capture and replay do not imply
that approval. No library source, public API, ABI or algorithm changes are made.

## Evidence

| Record | Contents |
| --- | --- |
| [Reference identity](v0.8.0/reference.json) | Tag, exact commit, release and Full CI links, approval boundary |
| [Environment](v0.8.0/environment.json) | Local OS, compiler/tool versions, dependency commits, CMake options and effective feature configuration |
| [Source inventory](v0.8.0/source-inventory.json) | Byte sizes and SHA-256 of every file in the immutable Git archive, including unit/integration tests, builtin fixtures, Query/property infrastructure and benchmark workloads, plus release archive identity metadata |
| [Test results](v0.8.0/test-results.json) | Every executed CTest scenario, status and observed duration |
| [CTest transcript](v0.8.0/logs/ctest.txt) | Original test run output; configure/build transcripts are in the same directory |
| [Fuzz corpus](v0.8.0/fuzz-corpus.json) | All 164 existing seed files, original paths, exact bytes as hex and checksums |
| [Generator fixtures](v0.8.0/generator/) | 32 cases in each of ten property configurations, metadata, ordered candidates, seed, case index and expected bytes |
| [Query evidence](v0.8.0/query/) | Fixed corpus and generated trees/queries with variables and expected identities/scalars or diagnostics |
| [Behavior observations](v0.8.0/behavior.json) | Document mutation/serialization, Schema outcomes and Codec encode/decode, including delegated diagnostic detail |
| [Diagnostic codes](v0.8.0/diagnostic-codes.json) | Numeric public enum inventory and Query limit names read from the release source |
| [Performance](v0.8.0/performance.json) | Raw Google Benchmark context, repetitions and aggregates for Reader decode workloads |
| [Capture tools](v0.8.0/capture-tools.json) | Hashes of the observation/capture code and #644 mapping used to produce the evidence |
| [Checksums](v0.8.0/checksums.json) | Complete evidence-file inventory and SHA-256; CI rejects changes, missing files and unexpected files |

The full release test suite captures Document mutation and serialization,
Schema validation, Codec conversions and builtin protocol vectors. Their
existing assertions remain the oracle: the source inventory pins those exact
scenarios, not only their test names. The standalone [probe](probe.cpp) adds
reviewable concrete outputs without modifying those scenarios or Core.

The captured local configuration passes **1,300 CTest scenarios**. The separate
Query replay passes **171 fixed fixtures and 1,056 generated comparisons**
(11,637 native executions, no unavailable-provider fixtures). The diagnostic
inventory contains **126 codes/names across 16 families**. These are local
reference results; hosted platform and quality-gate evidence remains in the
release manifest and linked run.

### Regression corpora and Generator

Fuzz seed bytes are preserved as hex to avoid newline conversion or formatting
hooks corrupting binary inputs. Recreate a seed with
`bytes.fromhex(entry["hex"])` at its recorded relative path. The baseline records
existing seeds; it does not claim a new fuzz campaign. Release Full CI supplies
the hosted fuzz evidence.

Property fixtures preserve all 320 successful cases, not only failures. They
exercise Fixed narrow/wide big-endian/wide little-endian/empty, BER, DER, EMV,
Bluetooth LTV, NFC Type 2 and NFC with NULL/Terminator controls. Their metadata
records SplitMix64, `TLV_GENERATOR_VERSION`, the candidate-domain version and
ordered tag/value limits. The Generator adapter and all configuration code are
pinned in the source inventory. NFC control bytes are a recorded post-generation
transformation; they are not output directly by `tlv_generate()`.

The original [property scope](../property/README.md) still applies: these are
wire round trips, not semantic validity assertions. CER, DHCP and LLDP have no
property candidate domain at this release; their native unit/integration vectors
are included in the full test run. Query additionally retains 32 generated trees
and the fixed conformance corpus with independently computed expected results.

### Diagnostics and the Issue 1 mapping

The collector checks every family selected by
[#644's mapping](../../docs/development/modules/diagnostic-mapping.json) against
the **release headers**, including enums that the future architecture will
remove. `diagnostic-codes.json` is the old-code input to that mapping; it does not
replace the planned domain/family ownership decisions.

`behavior.json` contains actual observations from the public C API:

- BER Document parsing, leaf replacement and exact serialized output, including
  invalidated ancestor/leaf locations and a sibling's retained original offset.
- Valid Schema input, an invalid child length and a truncated child. The last
  two record location and path availability, Schema expected/actual lengths or the
  delegated Reader operation, raw length bytes and available/required bounds.
- Schema-backed numeric Codec encoding/decoding and a rejected width, retaining
  the Codec operation and delegated Schema detail and the unchanged destination.

These are representative Schema-to-Reader and Codec-to-Schema cause chains in
the release's embedded-detail representation. They do not pretend the planned
generic cause-frame API already exists. Query diagnostic expectations and the
full native diagnostic tests supply the wider regression coverage. Unset fields
retain their `has_*` flags; pointer addresses and object padding are not recorded.

### Informational performance

`performance.json` measures the existing `reader_decode/` workloads through
Reader, Format and their Field implementations: flat/nested BER, Fixed TLV/LTV,
variable fields, sparse count fields and packed LLDP. Workload definitions and
timing boundaries are described in the pinned [benchmark guide](../../benchmarks/README.md).
The capture uses a Release static build, three repetitions and 0.05 seconds
minimum measurement time, retaining individual runs and aggregate statistics.

Captured median CPU time per complete input (microseconds, not per element):

| Reader workload | Median CPU time (us) |
| --- | ---: |
| BER flat small | 1457.09 |
| BER flat large | 10.51 |
| BER nested definite | 15.75 |
| BER nested indefinite | 89.20 |
| Fixed TLV | 959.83 |
| Fixed LTV | 1312.06 |
| Variable TLV | 1880.92 |
| Variable sparse | 1591.79 |
| Packed LLDP | 1536.63 |

These local WSL measurements are informational only. They are not a performance
gate or an optimization claim. Compare future results on the same hardware,
toolchain, configuration and workload, with an idle host and longer/repeated runs
when necessary. The release manifest's performance comparison against 0.7.1 is
separate hosted release evidence.

## Verify, reproduce and compare

Integrity verification requires Python 3 and Git with the reference commit and
tag available. It does not build or access the network:

```sh
python scripts/test_behavioral_baseline.py
python scripts/behavioral_baseline.py verify
```

For a fresh capture, use Linux x64 (including WSL), Python 3.8+, Git, GCC/G++,
CMake and Ninja. CMake fetches GoogleTest v1.18.0 and Google Benchmark v1.9.4;
their resolved commits are recorded. Use an empty output directory and a
persistent work directory; the script separates its source export and build:

```sh
python3 scripts/behavioral_baseline.py capture \
  --work build/baseline-reproduction \
  --output build/baseline-reproduction-evidence \
  --provenance tests/baseline/v0.8.0
python3 scripts/behavioral_baseline.py replay \
  --source build/baseline-reproduction/source \
  --build build/baseline-reproduction/build
```

Capture first validates release prerequisites, exports the exact Git commit,
and adds `opentlv-source.txt` using the existing release archive contract
(`version=0.8.0` and the full reference commit). This keeps version generation
independent of any parent Git checkout. The inventory includes those derived
metadata bytes alongside the 923 original source files.
Capture verifies every source byte if reusing an export, builds and runs all tests,
collects concrete observations, then seals the evidence inventory. It fails on
failed/skipped tests, incomplete corpora or failed benchmark workloads. If a
capture fails, keep its logs and use a new output directory for the retry.
An existing output directory is never overwritten.

Replay compares all Generator bytes, the concrete behavior/diagnostic JSON and
the Query fixtures/results with this package. To evaluate a future candidate,
pass its source and compatible Linux x64 static build to `replay`, and separately
run its complete CTest suite. The probe uses the v0.8.0 public API; an intentional
API migration needs a reviewed adapter while retaining these original outputs.
Performance and test timings are not compared for exact equality. Capture never
approves or refreshes the checked-in baseline automatically.
