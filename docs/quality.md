# Fixed TLV contributor checks

The `fixed-tlv` and `first-party-analysis` jobs use the same contract and
analysis commands as contributors. Run the commands below from the root of
a full Git checkout. These tools are optional for consumers. The default
production build requires CMake 3.15 or newer and a C99 compiler and
configures offline.

## Start from a clean checkout

Use Bash on Ubuntu 24.04 x86-64 to reproduce the Linux jobs. Install Git
and clone the repository.

```bash
git clone https://github.com/OpenTLV/opentlv.git
cd opentlv
```

To test a specific commit, run `git switch --detach <candidate-sha>` with
the SHA under review. Check that `git rev-parse HEAD` prints that SHA and
`git status --short` prints nothing before the first run. For the GCC
contract suite, install CMake and CTest 3.21 or newer, Ninja, Python 3.8
or newer and the GCC 13 C and C++ compilers.

```bash
sudo apt-get update
sudo apt-get install -y cmake ninja-build python3 gcc-13 g++-13
python3 -B tests/run_quality.py contracts --work-dir build/quality
```

This builds Debug and Release with Ninja Multi-Config and runs each through
`tests/run_ctest.py --require-x64`. Inspect `junit.xml`, `ctest.log` and
`toolchain.txt` under `build/quality/ctest-reports/{Debug,Release}/`.
The `--work-dir` option selects the build and report location and defaults
to `build/quality`. Use a fresh directory when changing compiler or revision.
The [compiler matrix](testing.md#compiler-and-architecture-matrix) identifies
the separate Clang and MSVC configurations.

## Formatting and analysis

For a format check, install the pinned LLVM tools into a virtual environment
and activate it in the shell that runs the checks. Formatting requires Python
and clang-format 18.1.8.

```bash
sudo apt-get install -y python3-venv
python3 -m venv build/quality/venv
. build/quality/venv/bin/activate
python -m pip install --require-hashes --only-binary=:all: -r .github/requirements-source-quality.txt
python -B tests/run_quality.py format
```

For the complete `first-party-analysis` job, use the contract build tools
above and install Cppcheck and the Valgrind headers used by the benchmark
sources. The analysis command checks formatting, runs clang-tidy 18.1.8 and
Cppcheck 2.13.0, and verifies that deliberate C and C++ violations are caught.

```bash
sudo apt-get install -y --no-install-recommends cppcheck=2.13.0-2ubuntu3 valgrind=1:3.22.0-0ubuntu3
python -B tests/run_quality.py analysis --work-dir build/quality
```

The first contract configure fetches GoogleTest 1.17.0. Analysis also needs
Google Benchmark 1.9.4 for its headers. For offline CMake configuration,
set `OPENTLV_GOOGLETEST_URL` and, for analysis, `OPENTLV_GOOGLE_BENCHMARK_URL`
to absolute paths to the corresponding source archives before running the
commands. CMake checks their pinned SHA-256 hashes. On Windows x64, the
same requirements file supplies a clang-format wheel for the format check.
Use the [Windows test instructions](testing.md#windows-x86-and-x64) for
MSVC Win32 Release and x64 Debug.

## Performance checks

To compare a local candidate on the CI reference host, follow the
[two-revision performance procedure](performance-baseline.md#reproduce-a-comparison).
It runs `benchmarks/collect_performance.py` for the pinned baseline and the
candidate using GCC 13.3.0, Valgrind 3.22.0 and GNU size. Both measured
checkouts must be clean, and each build and report directory must be new and
outside the checkout it measures. To keep everything under one local build
directory, place both detached checkouts and their output directories there
as siblings. The separate [resource checks](testing.md#heap-allocation-and-binary-size)
use GDB to detect allocations and verify reproducible static and shared
footprints.

To replay hosted measurements using Git and Python, check out the measured
candidate and extract its `fixed-tlv-performance` artifact into
`build/quality/performance`. Run the comparator and compare the resulting
report with the submitted one. The CI `callgrind` job runs the same command
after collecting both sets of measurements. Both use the baseline SHA from
`benchmarks/performance-baseline.txt`. Local replay checks the candidate
against Git HEAD, and Actions passes its event SHA with `--candidate-revision`.

```bash
artifact=build/quality/performance
python3 -B tests/run_quality.py compare-performance --reports "$artifact" \
  --output build/quality/replay
cmp "$artifact/comparison.json" build/quality/replay/comparison.json
```

An increase in an instruction count or a measured file or section size fails
the comparison. Review timing changes alongside the raw measurements.

## Full CI and release evidence

1. Record the full candidate SHA with `git rev-parse HEAD`. In **Actions →
   Fixed TLV → Run workflow**, select the branch at that SHA. For the release
   handoff, select the reviewed `main` candidate. A scheduled Full run on
   the same SHA also qualifies.
2. Verify the run's event is `workflow_dispatch` or `schedule` and its
   commit equals the recorded SHA. A PR or push run skips the Clang header
   step and the `windows-package` job. In the Full run, verify that the
   Clang header check passed, along with both static and shared MSVC package
   checks. Verify `fixed-tlv-quality` and each worker in the
   [workflow matrix](governance.md#continuous-integration) passed, including
   `first-party-analysis`, `workflow-analyzers`, both Windows architectures
   and both resource configurations.
3. Save the run URL, run attempt, full candidate SHA and full baseline SHA
   from `benchmarks/performance-baseline.txt` in the release evidence for
   [issue #58](https://github.com/OpenTLV/opentlv/issues/58). Retain the
   following artifacts from that run before they expire and record their
   names and IDs. GitHub also provides a SHA-256 digest for checking each
   downloaded ZIP before extraction.

| Artifact | What to check |
| --- | --- |
| `ctest-reports` | GCC Debug and Release, Clang Release and Clang Debug ASan/UBSan suite results and toolchain reports. |
| `ctest-reports-x86`, `ctest-reports-x64` | MSVC Win32 Release and x64 Debug results, including the ABI and native-size cases. |
| `coverage-reports` | Both GCC/gcov reports agree and meet the [production coverage gates](coverage.md). |
| `fixed-tlv-memcheck` | Expected results for the negative controls and clean suite logs. |
| `fixed-tlv-resource-usage-static`, `fixed-tlv-resource-usage-shared` | Allocation checks, matching repeat footprint reports and build settings for each linkage. |
| `callgrind-instruction-counts` | Candidate instruction counts, raw profiles and build metadata. |
| `fixed-tlv-performance` | `comparison.json` records `pass` and the expected baseline and candidate SHAs. Retain the summary and both measurement directories. |

`fixed-tlv-fuzz-failures` is uploaded on a failing fuzz step when inputs are
available. If a required job failed, an artifact is missing or the revision
differs, record the affected job or artifact and run URL as a blocker for
#58. Collect [managed CodeQL evidence](governance.md#codeql-coverage) for
the same candidate separately.
