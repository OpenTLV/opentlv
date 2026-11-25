# Testing OpenTLV

Run the commands from the repository root. The production libraries require a
C99 compiler and CMake 3.15 or newer. Setting `BUILD_TESTING=ON` requires a
C++17 compiler and adds GoogleTest 1.17.0 to the build. Consumers use
C99 or C++17.
The test build requires CMake 3.18 or newer. For the commands below, use
CMake and CTest 3.21 or newer, Ninja, and Python 3.8 or newer.

## Build and run

Start with a fresh build directory. For Ninja with MSVC, run these commands
from a Visual Studio developer shell with the target compiler on `PATH`.

```sh
cmake -S . -B build-tests -G "Ninja Multi-Config" -DBUILD_TESTING=ON
cmake --build build-tests --config Debug
ctest --test-dir build-tests -C Debug --output-on-failure
```

On the first configure, CMake downloads the pinned GoogleTest source archive
and verifies its SHA-256. To configure a fresh test build **offline**, obtain
the same [GoogleTest archive] in advance and pass its absolute path to CMake.

```sh
cmake -S . -B build-tests -G "Ninja Multi-Config" -DBUILD_TESTING=ON "-DOPENTLV_GOOGLETEST_URL=/absolute/path/to/v1.17.0.tar.gz"
```

Both sources use the same checksum from [tests/CMakeLists.txt]. Follow either
configure command with the same build and CTest commands. The default
production build configures offline using only C.

## Benchmark workload catalog

From a Git checkout, enable `OPENTLV_BUILD_BENCHMARKS` to build the fixture
checker and Google Benchmark 1.9.4. This opt-in build requires Git, CMake
3.19 or newer and a C++17 compiler. CMake verifies the source archive's
SHA-256. For an offline build, add
`-DOPENTLV_GOOGLE_BENCHMARK_URL=/absolute/path/to/v1.9.4.tar.gz` to the
configure command. Only the OpenTLV libraries, public headers, license and
CMake package are installed. [Pinned archive](https://github.com/google/benchmark/archive/refs/tags/v1.9.4.tar.gz)

```sh
cmake -S . -B ../opentlv-bench -G Ninja -DCMAKE_BUILD_TYPE=Release -DOPENTLV_BUILD_BENCHMARKS=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build ../opentlv-bench
ctest --test-dir ../opentlv-bench -C Release -L benchmark --output-on-failure
../opentlv-bench/opentlv_check_fixtures --list > ../opentlv-bench/workloads.txt
```

The checker exercises 1,792 workloads through each public API. It compares
results against independently constructed wire bytes, counts, borrowed views,
cursor positions, diagnostics and destination guards. Run it before measuring.
`--list` emits the sorted IDs after all checks pass. For example,
`cpp17/v1/write/le8/offset1/payload513/short/diagnostic` identifies a C++17
one-shot write with an eight-byte little-endian length, a 513-byte payload,
one byte too little capacity and a diagnostic output. The matching C99 ID
starts with `c99/`. Use a new catalog version when changing the inputs or
expected behavior of an existing ID.

| Operations | Inputs and expectations |
| --- | --- |
| Field read and write | Length widths 1–8 in both byte orders. The field starts at offset 0 or 1 from a 64-byte boundary. Short cases supply one byte less than the field width. |
| Header and element measurement | A two-byte tag and a valid format, or an invalid identifier width or mismatched element tag. Each failure preserves the output size. |
| One-shot and cursor reads and writes | The same 16 layouts and length-field alignments, with 0, 3 or 513 payload bytes (255 for a one-byte length). Complete and one-byte-short buffers run with a diagnostic or a null diagnostic pointer. |

The build writes `benchmarks/Release/metadata.json` beneath the build
directory. It records the Git revision and working-tree status, target,
pointer width, library linkage, C and C++ compiler versions and flags, coverage
instrumentation, interprocedural optimization, MSVC runtime selection where
applicable, and tool versions. Keep it with the workload IDs and
`compile_commands.json` for the exact compiler invocations. Use a clean
checkout for comparable results and compare matching workload versions,
inputs and build settings. This catalog reports fixture correctness. The
[GDB probes](../tests/diagnostics/README.md) record instruction counts.

## Select a contract

| Source directory | CTest label | Behavior |
| --- | --- | --- |
| `tests/core/` | `core` | Fixed length fields of 1–8 bytes in both byte orders, size limits, borrowed values and result codes. |
| `tests/io/` | `io` | One-shot and cursor operations, bounds, diagnostic offsets, and preservation of outputs and committed bytes on error. |
| `tests/integration/` | `integration` | Literal wire vectors, adjacent elements with every prefix of the final element, and binary round trips with 1-, 2- and 9-byte tags. Each length layout uses seed `0x26f1` and payloads of up to 513 bytes. |

CTest discovers individual GoogleTest cases after the build. List them with
`-N`, select a suite by label with `-L`, or filter case names with `-R`.

```sh
ctest --test-dir build-tests -C Debug -N
ctest --test-dir build-tests -C Debug -L "^io$" --output-on-failure
ctest --test-dir build-tests -C Debug -L "^core$" -R "LittleEndian3$" --output-on-failure
ctest --test-dir build-tests -C Debug -R "^CoreSize[.]ReportsOverflowAndPreservesOutput$" --output-on-failure
```

Use `-L "^integration$"` for end-to-end tests. The [behavioral scenario
inventory](test-scenarios.md) maps individual contracts to their test cases.

### Add a case

Put a new `TEST` or `TEST_P` alongside the cases for the same contract. If
it needs a new source file, add the file to the corresponding test executable
in [tests/CMakeLists.txt]. Rebuild and use `ctest -N` as above to find the
discovered case name. For a new wire layout, use independently specified
expected bytes so the encoder and decoder can be checked separately. For a
failure contract, assert the return code and the documented state of the
outputs, buffer and cursor. When adding a public function, map it to a case
in the [public API coverage table](coverage.md#public-api-behavior).

## Debug and Release reports

The CI runner executes the full suite and requires a passing case under each
of the three labels. It also checks the JUnit results for specific C/C++17
Core and IO cases and for both cross-language round-trip and truncated-stream
cases in all 16 wire layouts. A missing or skipped required case fails the
run. Debug, Release, sanitizer, coverage and x86 builds use the same runner.
It writes `junit.xml` and `ctest.log` in the directory given as the final
argument. Use a separate directory for each configuration.

```sh
python3 tests/run_ctest.py build-tests Debug reports/Debug
cmake --build build-tests --config Release
python3 tests/run_ctest.py build-tests Release reports/Release
```

CI uploads the reports from Debug, Release and sanitizer runs in the `ctest-reports`
artifact. On Windows, use `python` for the Python 3 interpreter.

## Fixed TLV instruction counts

The field probe checks golden bytes and counts instructions for aligned and
unaligned fields across all 16 layouts. It also measures complete records with
empty, three-byte and 255- or 513-byte payloads. The diagnostic probe measures
element sizing and one-shot and cursor operations on complete and short buffers.
The wrapper probe compares optimized C99 and C++17 callers on the same inputs
and includes the caller's descriptor conversions in the instruction count.
See the [measurements and reproduction commands](../tests/diagnostics/README.md).

## Sanitizers

On Ubuntu 24.04, use Clang 18 with the AddressSanitizer and
UndefinedBehaviorSanitizer runtimes. Configure a fresh build directory with
the flags on both C and C++ so the production code and tests are instrumented.

```sh
flags="-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer"
CC=clang-18 CXX=clang++-18 CFLAGS="$flags" CXXFLAGS="$flags" LDFLAGS="-fsanitize=address,undefined" \
  cmake -S . -B build-sanitized -G Ninja -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build-sanitized
ASAN_OPTIONS=halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  python3 tests/run_ctest.py build-sanitized Debug reports/sanitizers
```

## Coverage

CI runs GCC, G++ and gcov 13.3.0 on Ubuntu 24.04 x86-64. The coverage
driver configures a fresh instrumented build for each report. For an offline
run, export `OPENTLV_GOOGLETEST_URL` with the absolute archive path first.

```sh
export CC=gcc-13 CXX=g++-13 GCOV=gcov-13
python3 -m unittest discover -s tests -p test_check_coverage.py
python3 tests/check_coverage.py reports/coverage.txt
python3 tests/check_coverage.py reports/coverage-repeat.txt
diff -u reports/coverage.txt reports/coverage-repeat.txt
```

The two reports must agree byte for byte. CI publishes them as
`coverage-reports`. See the [production coverage gates](coverage.md) for the
measured sources, reviewed branches and mapping of public functions to tests.

## 32-bit x86

The `windows-x86` job runs on Windows Server 2022 with Visual Studio 2022
C++ Build Tools and the v143 14.44.35207 x86/x64 toolset. Run these commands
in PowerShell with a fresh build directory.

```powershell
cmake -S . -B build-x86 -G "Visual Studio 17 2022" -A Win32 -T "v143,version=14.44.35207" -DBUILD_TESTING=ON -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded
cmake --build build-x86 --config Release --parallel
python tests/run_ctest.py build-x86 Release reports/x86 --require-x86
```

The test binaries link the static MSVC runtime. The runner checks the JUnit
results for a passing four-byte `size_t` case and named boundary cases for
`SIZE_MAX`, `SIZE_MAX + 1`, checked addition, and truncation of a 64-bit
wire length. CI publishes the log and JUnit report as `ctest-reports-x86`.

## Fuzzing and compatibility

The standalone Clang/libFuzzer project under `tests/fuzz/` checks the reader
and writer. Its runner replays the checked-in seed corpus before bounded
mutation. A saved input can be replayed directly with its target, for example
`build-fuzz/fuzz_reader tests/fuzz/corpus/reader/partial-value`. See the
[fuzz build, run and replay commands](../tests/fuzz/README.md).

Public-header probes compile C headers in C99 and all public headers in
C++17. They also check that the C++ umbrella rejects C++11 and C++14.
Separate package checks exercise Core and IO from independent C99 and C++17
consumers under `tests/package/`. They run against the build tree and the
relocated installation for static and shared builds. See the [header and package
commands](architecture.md#verification).

[GoogleTest archive]: https://github.com/google/googletest/archive/refs/tags/v1.17.0.tar.gz
[tests/CMakeLists.txt]: ../tests/CMakeLists.txt
