# Fixed TLV contributor checks

The `fixed-tlv` and `first-party-analysis` jobs use the same contract and
analysis commands as contributors. Run the commands below from the root of
a full Git checkout. These tools are optional for consumers. The default
production build requires CMake 3.15 or newer and a C99 compiler and
configures offline.

To test the quality runner itself, run the following command from the repository
root. The tests use Python's standard library, mocked tools and temporary
performance reports. The `first-party-analysis` job runs them alongside the
source analysis tests.

```sh
python3 -B -m unittest discover -s tests -p "test_run_quality*.py"
```

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
`tests/run_ctest.py --require-x64`. Inspect `junit.xml`, `ctest.log`,
`toolchain.txt` and `passed-cases.txt` under `build/quality/ctest-reports/{Debug,Release}/`.
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

## Release documentation and examples

From a clean candidate checkout, run the README's [installation](../README.md#build-and-install)
and [example commands](../README.md#run-the-examples). For a shared build,
choose a fresh build directory and installation prefix and configure with
`-DBUILD_SHARED_LIBS=ON`. Use the same prefix for the consumer.
Run the release package check for each linkage.

```sh
python3 -B tests/check_cmake_package.py --check-symbols
python3 -B tests/check_cmake_package.py --shared --check-symbols
```

In each relocated installation, the README's CMake project must build with
only C enabled. Its writer and the three programs in `examples/` must print
the output in the README table. The check builds both C++17 API guide
programs and runs them through the `doc-core` and `doc-io` CTest cases
to verify the documented element size, bytes consumed and borrowed views.

The README recipes passed locally with static and shared installations on
Linux x64 (GCC 15.2) and on Windows x64 and Win32 (MSVC 19.52). The
Windows x64 run used CMake 3.15.7. Each profile ran the writer using the
installed package, the build tree, `add_subdirectory`, and `FetchContent`
with both a local checkout and a source archive.

The link audit uses the organization's Markdown checker. The commands
below use Python 3.12 and Bash from the candidate checkout.

```bash
git clone --depth 1 --branch v0.1.0 https://github.com/OpenTLV/.github.git build/quality/governance
git -C build/quality/governance checkout --detach db813248c730dd29b217ad90d6a85a562ed98cd6
python3.12 -m venv build/quality/doc-tools
build/quality/doc-tools/bin/python -m pip install PyYAML==6.0.3
build/quality/doc-tools/bin/python -B tests/check_doc_links.py build/quality/governance
```

The audit checks file names and heading fragments in the README, changelog,
guides and test documentation. It also resolves organization policy links
in the pinned governance checkout. Check the live policy links and other
external URLs separately and record any that require authentication.
Check the package version against the root
`CMakeLists.txt` and the [supported platform table](testing.md#compiler-and-architecture-matrix)
against the jobs in the [Fixed TLV workflow](../.github/workflows/fixed-tlv.yml).

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
   Clang header check passed. In `fixed-tlv` (Linux x64), verify that the
   [source archive check](release-artifacts.md#verify-the-archive) passed.
   In its static and shared package results and both `windows-package` legs
   (MSVC Win32 and x64), check the C99 and C++17
   results for `add_subdirectory` and `FetchContent`, then the versioned
   `find_package` results for the build tree, original install prefix and
   relocated prefix. Check the Core and IO symbol counts and the three
   example outputs, the README consumer and both C++17 guide cases. Include
   the [documentation link audit](#release-documentation-and-examples) in
   the candidate evidence. Compare the repeat contract results for each ABI profile
   using the [v0.1.0 release baseline](api-abi.md#compare-a-release-candidate).
   Verify `fixed-tlv-quality` and each worker in the
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
| `fixed-tlv-source` | Extract the source tarball from the Actions ZIP. Its filename must contain the candidate's CMake version and its embedded Git commit must match the Full CI SHA. See the [release artifact guide](release-artifacts.md#build-the-archive). |
| `ctest-reports` | GCC Debug and Release, Clang Release and Clang Debug ASan/UBSan results. In Full CI, `Release/passed-cases.txt` and `Release-repeat/passed-cases.txt` agree. |
| `ctest-reports-x86`, `ctest-reports-x64` | MSVC Win32 Release and x64 Debug results, including the ABI and native-size cases. In each Full CI artifact, `passed-cases.txt` and `repeat/passed-cases.txt` agree. |
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
