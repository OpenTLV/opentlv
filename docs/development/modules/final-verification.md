# Final modular verification

This is the verification procedure and completion record for
[#655](https://github.com/OpenTLV/opentlv/issues/655), the final gate of
[#643](https://github.com/OpenTLV/opentlv/issues/643).

## Reference and completion boundary

The immutable reference is **v0.8.0**, commit
`345c77acb29266036df5f27d04ce315bb61853cc`. On 2026-11-02 the GitHub tag
still resolved to that commit and the enclosing
[release/Full CI run 40500582050](https://github.com/OpenTLV/opentlv/actions/runs/40500582050)
was completed successfully. The sealed [baseline](../../../tests/baseline/README.md)
contains the exact source inventory, release quality manifest, 1,300 original
CTest scenarios, 164 fuzz seeds, 320 Generator cases, Query observations and
126 diagnostic codes/names in 16 families. Verification checks its checksums
and source archive; it never updates the reference to make a comparison pass.

The local candidate starts at merged commit
`7ff1e96e00ac6cfdc189f742020ab42e5e5eec4e`, with the uncommitted verification
changes for #655. Reports record the parent commit, working-tree status and
hashes of the actual source/test inputs. No library functionality, algorithms,
public declarations or external downstream repositories change in #655.

**Epic completion remains pending.** On 2026-11-02 the epic had no comments,
so there was no dated authoritative start-gate comment to reference. Naming
prerequisite [#702](https://github.com/OpenTLV/opentlv/issues/702) was still open.
The historical baseline's approval field remains unchanged. A later approval
record must describe the actual chronology rather than claim the gate preceded
implementation.

[Core Full CI 41441413926](https://github.com/OpenTLV/opentlv/actions/runs/41441413926)
passed at `3392bcc2050c433ff3ef21b105be1e8adf8df4c3`, the final #733 branch
commit. This is evidence for the preceding implementation, not the new #655
verification changes. Closure requires a successful Full CI run at the final
candidate commit and reconciliation of the missing start gate. Use `Refs #655`
until these acceptance criteria are satisfied.

## Local verification results (2026-11-02)

The candidate was tested on Linux x64 under WSL2 with GCC 13.3.0, C99 and
C++23; fuzz replay used Clang 18 with ASan/UBSan. Local evidence is retained
under `build/issue655/`. CI produces equivalent reports in the artifacts listed
below, identified by its own candidate SHA and attempt.

| Gate | Observed result |
| --- | --- |
| Module inventory and graph | All 13 public modules and 8 integrations registered; no boundary exceptions |
| Fresh isolated static/shared builds | All 19 compiled targets and their declared closures passed |
| Export ownership and dependencies | Static symbol dependencies and exact shared exports passed |
| Relocated installed packages | 38 C/C++ consumer configurations passed for each linkage; combined selection ran both APIs |
| Obsolete module-selection options | All 15 removed selectors rejected with either ON or OFF |
| Native, integration, builtin and facade tests | 109 CTest suites passed, including 1,268 individually recorded GoogleTest scenarios |
| Baseline scenario preservation | All 1,300 original observations covered; no scenario migration exceptions needed |
| Diagnostic mapping | All 126 old codes/names mapped; ownership and distinct meanings validated |
| Generator | All 320 cases in 10 domains matched exactly, including metadata |
| Query and concrete behavior/diagnostics | All preserved observations matched exactly |
| Frozen fuzz corpus | All 164 seeds replayed successfully through 14 sanitizer targets |
| Candidate Full CI | Pending publication and execution at the final #655 commit |
| Authoritative start-gate comment | Missing; #702 remains open |

## Reproduce the technical gates

Use Linux x64, Python 3.11+, GCC/G++, Clang 18 with sanitizer runtimes, CMake,
Ninja and GNU/LLVM `nm`. Keep the reference tag and commit available locally.
Run from the repository root; each evidence output must be a new directory.

```sh
python -B scripts/test_module_contracts.py
python -B scripts/test_module_isolation.py
python -B scripts/test_behavioral_baseline.py
python -B scripts/test_modular_baseline.py
python -B scripts/check_module_isolation.py --shared OFF
python -B scripts/check_module_isolation.py --shared ON
python -B scripts/verify_modular_baseline.py \
  --build build/modular-baseline --output build/modular-evidence
python -B scripts/modular_performance.py --output build/modular-performance
```

Configure a separate sanitizer build for the frozen fuzz corpus:

```sh
cmake -S . -B build/modular-fuzz -G Ninja \
  -DCMAKE_C_COMPILER=clang-18 -DCMAKE_BUILD_TYPE=Debug \
  -DOPENTLV_BUILD_FUZZING=ON -DOPENTLV_BUILD_CXX=OFF \
  -DOPENTLV_BUILD_TESTS=OFF -DOPENTLV_BUILD_EXAMPLES=OFF
cmake --build build/modular-fuzz --parallel 2
python -B scripts/behavioral_baseline.py fuzz \
  --build build/modular-fuzz --output build/modular-fuzz-evidence
```

The existing CMake consumer matrix also covers Windows and the full
static/shared, C/C++ configuration matrix. The ELF symbol inspection below
does not replace those platform jobs.

## Architecture and installed consumers

The contract checker enforces the complete acyclic graph, generic-to-builtin
dependency prohibition, exact file ownership and private-header boundaries.
The final baseline gate requires every contracted module and integration to
be registered and all migration exceptions to be resolved.

Isolation uses a fresh build for each compiled target and checks that only
its declared dependency closure produces libraries. It validates direct and
transitive dependencies, duplicate/unresolved/foreign symbols, owner-internal
interfaces and exact shared exports. All seven generic and six builtin modules
and their integrations participate.

Installed consumers use a relocated prefix, load only their requested
components and reject an implicit `OpenTLV::tlv` aggregate. They compile every
owned public header independently. Pure C cases supply an unusable C++ compiler;
facades inherit C++11 requirements from their functional targets. A combined
Generator/Codec case exercises multiple component selection and both APIs.
The static and shared reports preserve the module registry and consumer list.

## Behavior, diagnostics and scenario coverage

`verify_modular_baseline.py` builds and runs the complete native/property suite,
then replays the immutable Generator, Query and concrete behavior observations.
Generator comparison includes algorithm version, configuration, ordered
candidates, seed, limits and every output byte. Existing Generator result,
workspace and allocation tests supply error-path coverage.

After modularization CTest groups GoogleTest scenarios into owned executables.
The verifier therefore collects per-scenario GoogleTest XML as well as CTest
JUnit. Increasing the total number of tests cannot hide a lost baseline
scenario. Explicit renames require a reason and named passing replacements in
[`scenario-migrations.json`](../../../tests/baseline/scenario-migrations.json).
For the old NFC parameter names that contain heap addresses, comparison ignores
the address dump but preserves the number of cases for each test.

Diagnostic mapping checks read the immutable old vocabulary, independently of
the current headers. Removing both a declaration and its mapping still fails;
so do changed numeric values, duplicate mappings, invalid ownership and merged
meanings. Reports list every mapped baseline code with its owner, new identity,
meaning and representation change. The probe compares actual Schema/Reader and
Codec/Schema cause content, offsets, paths, tags, expected/actual values and
availability flags. Query conformance and existing diagnostic tests cover the
remaining execution and integration scenarios. Adapters normalize public header
paths and the previously documented diagnostic representation changes, never
the expected outputs.

Fuzz replay reconstructs all original bytes from the sealed corpus and executes
14 current fuzz targets with ASan/UBSan, including DOL and Query pairs. It is a
regression replay; the existing Full CI fuzz campaign remains a separate check.

## Informational performance

The performance runner freshly builds both libraries using GCC, static Release,
`-O3 -DNDEBUG` and no LTO. It compiles the same v0.8.0 Reader, Writer, Query and
DER parsing workloads against both libraries and adds the same numeric Codec
round-trip workload on both sides. The candidate adapter changes include paths
and supplies the frozen harness's former feature macros as enabled; those
macros are local to the harness and are not restored in the library.

Seven alternating native runs retain raw timings and checksums. Workload
identity, input size, iteration count and checksums must agree. Timings are
informational: disjoint observed ranges are reported as changes, overlapping
ranges as noise. The report records both commits, dirty status, source/workload
hashes, compiler, configuration, CPU and platform. Significant regressions need
evaluation before epic closure; this task makes no optimization changes.

Local measurements used an Intel Core i7-1255U, GCC 13.3.0 and 5,000 iterations
per sample. Median time per workload iteration, in microseconds:

| Workload | v0.8.0 | Modular candidate | Median difference |
| --- | ---: | ---: | ---: |
| Reader / BER Format / Field | 27.563 | 27.991 | +1.55% |
| Writer | 34.981 | 33.902 | -3.08% |
| Query execution | 54.943 | 52.047 | -5.27% |
| Builtin DER Schema parsing | 19.609 | 17.692 | -9.78% |
| Numeric Codec round trip | 8.556 | 8.619 | +0.74% |

All five pairs had overlapping observed ranges and were classified as noise.
There is no clear regression in these samples and no optimization claim.
Raw per-run timings and semantic checksums are retained in `performance.json`.

## Evidence publication and remaining actions

PR/Main/Full consumer jobs run the isolation and behavioral gates. Full CI also
records the informational performance comparison. Artifacts preserve successful
reports and failure logs; the fuzz job preserves the original-corpus replay.
The existing Core quality gate requires these jobs to pass.

For final closure, attach the candidate's modular verification, fuzz,
performance and Full CI artifacts, reference the dated epic start-gate comment,
and resolve any significant performance observation. Local successful checks
alone cannot supply the missing approval record or hosted candidate CI.
