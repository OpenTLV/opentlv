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

## Formatting and static analysis

The `first-party-analysis` CI job runs clang-format 18.1.8, clang-tidy
18.1.8 and Cppcheck 2.13.0. See the [contributor checks](quality.md#formatting-and-analysis)
for the pinned tools and the same command CI runs on Ubuntu 24.04 x86-64.

The runner checks source and header files beneath `include/`, `src/`,
`examples/`, `tests/` and `benchmarks/`, including ABI inventories,
package consumers, fuzz targets and diagnostic probes. It skips CMake
build trees, fetched dependencies, generated code and vendor directories.
Public C headers are analyzed in both C99 and C++17. The ABI inventory headers
are analyzed through `tests/abi_layout.cpp`, which supplies their macro and types.

Clang-tidy treats the checks selected in [the configuration](../.clang-tidy)
as errors. They cover memory and pointer use, object lifetimes, and suspicious
size and memory operations. The writer records one analyzer-only assumption
after `tlv_fixed_element_size` succeeds. Every valid Fixed TLV element has
at least one tag byte and one length byte, so its encoded size is positive
even for an empty payload. This lets clang-tidy check the sizing-only call
with a null destination and zero capacity across the translation-unit boundary.

Cppcheck uses its Unix 64-bit data model, GoogleTest macro definitions and
error, warning, performance and portability diagnostics. One `passedByValue`
finding is suppressed at the definition of `fixture::fixture(workload)` in
`benchmarks/fixtures.cpp`. This constructor takes a snapshot before measurement.
Its signature is part of the [fingerprinted measurement protocol](performance-baseline.md#read-or-replay-a-report).
The exception names the diagnostic, source path and line.

Pass `--check format`, `--check tidy` or `--check cppcheck` to
`tests/check_source_quality.py` to run one tool.
For example, `python tests/check_source_quality.py --check format src/core/size.c`
checks one file, and `clang-format -i src/core/size.c` applies the repository
style. The negative checks use a temporary directory and verify the expected
diagnostic from each tool.

## Compiler and architecture matrix

For v0.1.0, CI supports the following Fixed TLV Core and IO configurations.
Every row builds the libraries and runs the Core, IO, integration and ABI
layout tests on pull requests, pushes to `main`, and Full CI runs.

| Runner and target | C and C++ compilers | Configuration | Contract exercised |
| --- | --- | --- | --- |
| Ubuntu 24.04 x64 | GCC and G++ 13 | Debug and Release | Runs the reference Linux build with assertions enabled and with release optimization. |
| Ubuntu 24.04 x64 | Clang and Clang++ 18 | Debug with ASan/UBSan | Checks memory access and undefined behavior during the contract suite. |
| Ubuntu 24.04 x64 | Clang and Clang++ 18 | Release | Exercises Clang's optimized code without instrumentation. |
| Windows Server 2022 x86 | MSVC v143, toolset 14.44.35207 | Release | Checks Win32 record layout and rejects 64-bit counts above the four-byte native limit. |
| Windows Server 2022 x64 | MSVC v143, toolset 14.44.35207 | Debug | Checks the Windows x64 ABI, Debug runtime and successful native conversion through `UINT64_MAX`. |

GCC and Clang build OpenTLV in C99 and C++17 modes with extensions off
and `-pedantic-errors -Wall -Wextra -Werror`. MSVC checks the C sources in
`/std:c11`, its earliest selectable ISO C mode, and C++ in `/std:c++17`
with `/permissive-` and `/Zc:__cplusplus`. It treats level-four warnings
as errors (`/W4 /WX`). These warning settings apply to OpenTLV's own
validation targets. [MSVC language modes](https://learn.microsoft.com/en-us/cpp/build/reference/std-specify-language-standard-version?view=msvc-170)

Win32 supplies the four-byte `size_t` coverage and has a
[record-layout baseline](api-abi.md#visibility-and-verification). Linux
i686, other compiler versions and target/configuration pairs outside the
table are optional builds. Full CI also compiles the public headers with
Clang and runs Release static and shared package consumers with MSVC. See
the [workflow checks by trigger](governance.md#continuous-integration).

## Benchmark workload catalog

From a Git checkout, enable `OPENTLV_BUILD_BENCHMARKS` to build the fixture
checker and timing runner with Google Benchmark 1.9.4. This opt-in build requires Git, CMake
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
inputs and build settings. For instruction counts, see the
[Linux Callgrind and GDB probes](#fixed-tlv-instruction-counts).

## Runtime and throughput

For the cross-revision comparison used in CI, see the
[performance baseline and release evidence](performance-baseline.md).

Run the Release executable twice on the same idle machine. This example runs
all 3,584 language-qualified workloads with three repetitions per invocation.
The fixed iteration count makes a full catalog run practical. Keep both JSON
files with the build metadata. With a multi-config generator, take both
executables from the `Release` directory.

```sh
../opentlv-bench/opentlv_benchmark --benchmark_min_time=8192x --benchmark_repetitions=3 --benchmark_out_format=json --benchmark_out=../opentlv-bench/run1.json
../opentlv-bench/opentlv_benchmark --benchmark_min_time=8192x --benchmark_repetitions=3 --benchmark_out_format=json --benchmark_out=../opentlv-bench/run2.json
python3 benchmarks/check_results.py --fixtures ../opentlv-bench/opentlv_check_fixtures ../opentlv-bench/run1.json ../opentlv-bench/run2.json
```

Each iteration represents one public API operation. Each repetition starts
with 32 guard-filled destination buffers and prepared C99 or C++17 descriptors.
Successive batches reuse the same 32 buffers. After the first successful
write, later batches overwrite the same bytes with the same field or record.
This measures repeated use of caller-owned storage, which can benefit from
cache reuse. Reader and writer cursors are reset outside the timed interval
so each call starts at the same record.

Before timing, the runner checks one call against the golden fixture with a
fresh destination. After the last batch, it checks the final result, destination
bytes and guards for each of the 32 slots. Timings include the indirect call
through the adapter and the compiler barriers that keep the result and stores
observable. This overhead can be a significant part of a short field or sizing
operation. The JSON `run_name` contains the catalog ID followed by `/real_time`.

| JSON field | Meaning |
| --- | --- |
| `real_time` | Elapsed nanoseconds per operation in each iteration row. |
| `cpu_time` | CPU nanoseconds per operation. A short sample can be below the platform CPU timer's resolution. |
| `items_per_second` | Calls per elapsed second, including calls that return the expected error. |
| `bytes_per_operation` | Logical wire bytes successfully processed. Codecs count the field width. IO counts the complete encoded record, including tag and length. Sizing and failure paths count zero. |
| `bytes_per_second` | Logical wire bytes per elapsed second. A borrowed read counts the record it consumed, even though it only returns a view of the payload. |

For a closer comparison, filter to a matching C99/C++17 pair, allow the
framework to calibrate a longer run, and retain the individual repetitions.
The following command measures a complete unaligned write with diagnostics.

```sh
../opentlv-bench/opentlv_benchmark --benchmark_filter='^(c99|cpp17)/v1/write/le8/offset1/payload513/complete/diagnostic/' --benchmark_min_time=0.1s --benchmark_repetitions=7
```

Compare the wall-time medians, the spread of the repetitions, and the
variation between invocations. For repeated runs, the checker verifies the
mean, median, sample standard deviation and coefficient of variation for
timings and counters against the individual repetitions. In JSON, a `cv`
aggregate expresses the coefficient of variation as a ratio (0.02 means 2%).
CPU frequency, scheduling and background load can shift the measurements.
Use timing as observational evidence and compare matching workload IDs,
machines, toolchains and build settings.

## Heap allocation and binary size

`OPENTLV_BUILD_ALLOCATION_CHECKS` checks all 1,792 v1 inputs through both
public APIs. For each input, the probe prepares the caller's buffers, arms
the allocation check, initializes the cursor when the operation uses one,
and calls Core or IO. It disarms the check before comparing the results
with the golden fixture. A failure identifies the language and workload ID.
Separate checks exercise result classification and borrowed value-view
construction with valid and null inputs through both APIs.

The probe runs under Python-enabled GDB on native Linux or MinGW with GCC
or Clang. After the runtime libraries load, GDB sets breakpoints on
`malloc`, `calloc` and `realloc`, plus aligned allocation routines it can
resolve. An empty measurement checks the boundary. Deliberate calls to
the three C allocators and to scalar, array and over-aligned C++ allocation
functions verify that the observer detects an allocation. The result applies to
executed paths that reach the monitored allocators.

`OPENTLV_BUILD_FOOTPRINT` builds the C99 one-shot writer and the C++17
cursor writer from `examples/`. Both encode and print the same seven bytes.
Each executable links directly to `OpenTLV::io` and its Core dependency.
The report measures those executables and the two library files. For a
Windows shared build, the library measurements use the runtime DLLs.

From a Git checkout, run the following in Bash with CMake 3.21 or newer,
Ninja, Python 3, GCC, GDB and GNU size on the path. CI runs these checks
on Ubuntu 24.04 x86-64 with GCC and G++ 13.3.0. Each configuration gets
its own build directory. The second build uses the same paths and flags so
the two reports can be compared directly.

```bash
set -eu
export CC=gcc-13 CXX=g++-13
flags="-O2 -DNDEBUG -march=x86-64 -mtune=generic"
for shared in OFF ON; do
  build="$PWD/../opentlv-resources-$shared"
  cmake -S . -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_FLAGS_RELEASE="$flags" -DCMAKE_CXX_FLAGS_RELEASE="$flags" \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=OFF \
    -DBUILD_SHARED_LIBS="$shared" -DOPENTLV_BUILD_ALLOCATION_CHECKS=ON \
    -DOPENTLV_BUILD_FOOTPRINT=ON -DOPENTLV_SIZE_TOOL=/usr/bin/size
  cmake --build "$build" --target opentlv_allocation_probe opentlv_footprint
  ctest --test-dir "$build" -C Release --no-tests=error --output-on-failure --output-junit "$build/checks.xml"
  report="$build/benchmarks/footprint/Release/footprint.json"
  cp "$report" "$build/footprint-first.json"
  cmake --build "$build" --target opentlv_footprint --clean-first
  cmp "$build/footprint-first.json" "$report"
done
```

Each option can also be enabled independently. The footprint target needs
Python 3 and records `file_bytes` as the on-disk size. The `section_bytes`
map gives sizes by section name, such as `.text`, `.data` and `.bss`.
For archives, it sums like-named sections across members. File sizes
include headers and alignment, while a section such as `.bss` can occupy
memory without storing its contents in the file. A shared consumer's size
covers its executable alone. The runtime libraries have separate entries.
The two writers use different public call sequences, and each executable
also includes its toolchain's startup code.

The reporter reads System V section tables from GNU size or llvm-size, or
the hexadecimal section summary from MSVC dumpbin. Use `OPENTLV_SIZE_TOOL`
to select the tool for the target. If CMake finds none, `section_bytes`
is `null`. A selected tool must produce a valid report. Keep `footprint.json` with
`compile_commands.json` (Ninja) and the CTest report. Compare the same
artifact role, target, compiler, flags, linkage and section tool. CI publishes
these files in `fixed-tlv-resource-usage-static` and `fixed-tlv-resource-usage-shared`.

## Select a contract

| Source directory | CTest label | Behavior |
| --- | --- | --- |
| `tests/core/` | `core` | Fixed length fields of 1–8 bytes in both byte orders, size limits, borrowed values and result codes. |
| `tests/io/` | `io` | One-shot and cursor operations, bounds, diagnostic offsets, and preservation of outputs and committed bytes on error. |
| `tests/integration/` | `integration` | Literal wire vectors, binary round trips, every prefix of a final record, and writer capacities from zero remaining bytes through exact fit at a stream offset above 255. |

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

To reproduce a property failure, use the CTest name in the failure output.
For example, this runs the short-append property for one wire layout.

```sh
ctest --test-dir build-tests -C Debug -R "^WireLayouts/FixedRoundTrip[.]ReportsShortAppendsBeyondAOneBytePosition/LittleEndian2$" --output-on-failure
```

For `LittleEndian2`, 37 seven-byte records put the cursor at byte 259.
A short append must report offset 259 and preserve the committed bytes.
In a local regression check, casting the short-write diagnostic offset to
`uint8_t` made this case report 3. All 16 layouts detected the regression.
The failure trace reports the committed and remaining byte counts.

Rerunning the binary round-trip case for the reported layout regenerates
the same 1-, 2- and 9-byte tags and payloads of up to 513 bytes with seed
`0x26f1`. Its trace identifies the seed, tag width and payload size.

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
Core and IO cases and for cross-language round trips, truncated streams and
short appends beyond byte 255 in all 16 wire layouts. A missing or skipped
required case fails the run. For the targets in the matrix, `--require-x86` or `--require-x64`
also requires a passing ABI layout check and the named native-size boundary
cases for that architecture. The runner prints passing and discovered test
counts and writes `junit.xml`, `ctest.log` and `toolchain.txt` to the report
directory. The last file records both compiler IDs and versions and the
target pointer size in bytes. Use a separate directory for each configuration.

```sh
python3 tests/run_ctest.py build-tests Debug reports/Debug --require-x64
cmake --build build-tests --config Release
python3 tests/run_ctest.py build-tests Release reports/Release --require-x64
```

In the Linux `ctest-reports` artifact, inspect `Debug/` and `Release/` for GCC,
`sanitizers/` for Clang Debug and `clang-release/` for Clang Release. Windows
publishes `ctest-reports-x86` and `ctest-reports-x64`. For release evidence,
retain all three artifacts from the successful Full CI run for the candidate
SHA alongside its [performance evidence](performance-baseline.md#hand-off-release-evidence).

## Fixed TLV instruction counts

### Linux Callgrind

The opt-in `opentlv_callgrind` probe measures one adapter call for each of
28 catalog inputs through both the C99 and C++17 APIs. The workload ID is
the same ID used by the timing runner. Callgrind's `Ir` event counts executed
guest instructions. Each total includes the adapter dispatch, nested library
and runtime calls, and the client-request instructions at the collection
boundary. Fixture construction, cursor initialization and
output checks take place with collection disabled. [Callgrind manual](https://valgrind.org/docs/manual/cl-manual.html)

| Operations | Measured inputs for each API |
| --- | --- |
| Field read and write | Aligned three-byte big-endian and unaligned eight-byte little-endian fields, each with complete and one-byte-short buffers. |
| Header and element sizing | Header size with a two-byte big-endian length. Element size with an eight-byte little-endian length and a 513-byte payload. Each has a valid and an invalid case. |
| Read, next, write and append | Payload sizes of 0, 3, 513 and 3 bytes, respectively. Each operation runs on complete and one-byte-short buffers with a supplied or omitted diagnostic. |

The reference build uses Ubuntu 24.04 x86-64, GCC and G++ 13.3.0, and
Valgrind 3.22.0. Install the client headers and Callgrind from the same
Ubuntu package. From a Git checkout, run the following in Bash with Git,
CMake (3.19 or newer), Ninja and Python 3 on `PATH`. Use a fresh build and
a new reports directory for each comparison. [Ubuntu Valgrind package](https://packages.ubuntu.com/noble/valgrind)

```bash
set -eu
sudo apt-get install -y gcc-13 g++-13 valgrind=1:3.22.0-0ubuntu3
export CC=gcc-13 CXX=g++-13
test "$("$CC" -dumpfullversion)" = 13.3.0
test "$("$CXX" -dumpfullversion)" = 13.3.0
build=../opentlv-callgrind
flags="-O2 -g -DNDEBUG -march=x86-64 -mtune=generic"
cmake -S . -B "$build" -G Ninja -DOPENTLV_BUILD_CALLGRIND=ON \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DCMAKE_C_FLAGS_RELWITHDEBINFO="$flags" -DCMAKE_CXX_FLAGS_RELWITHDEBINFO="$flags" \
  -DBUILD_SHARED_LIBS=OFF -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=OFF
cmake --build "$build" --target opentlv_callgrind
python3 benchmarks/run_callgrind.py --probe "$build/opentlv_callgrind" --output "$build/reports"
```

The driver gets the expected IDs from `opentlv_callgrind --list` and starts
two independent Callgrind processes with collection initially disabled and
eager dynamic symbol binding (`LD_BIND_NOW=1`). The probe checks the measured
outputs against the golden fixtures, including expected errors, borrowed views, cursor
positions, diagnostics and destination guards. Each run produces a sorted
`counts.tsv` with one positive `Ir` total per ID. A missing workload, an
unexpected profile or a difference of even one instruction fails the check.
On a difference, inspect the named workload in `run-1/counts.tsv` and
`run-2/counts.tsv` and use `callgrind_annotate` on its raw profiles.

The CI artifact `callgrind-instruction-counts` contains the reports, raw
profiles, runtime information, build metadata and `compile_commands.json`.
When running locally, keep `reports/`, `benchmarks/RelWithDebInfo/metadata.json`
and `compile_commands.json` from the build directory together. Compare counts
for matching workload IDs, measurement boundaries, CPU, toolchain, build
flags and runtime libraries.

### GDB probes

The field probe checks golden bytes and counts instructions for aligned and
unaligned fields across all 16 layouts. It also measures complete records with
empty, three-byte and 255- or 513-byte payloads. The diagnostic probe measures
element sizing and one-shot and cursor operations on complete and short buffers.
The wrapper probe compares optimized C99 and C++17 callers on the same inputs
and includes the caller's descriptor conversions in the instruction count.
The GDB tables use single-stepped Windows x86-64 binaries and the entry
points specified for each probe. Compare those counts within the documented
GDB configuration. [Measurements and reproduction commands](../tests/diagnostics/README.md)

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
  python3 tests/run_ctest.py build-sanitized Debug reports/sanitizers --require-x64
```

## Memcheck

CI runs Valgrind Memcheck 3.22.0 against the GCC 13 Debug build on Ubuntu
24.04 x86-64. It runs the Core, IO and integration executables once each,
exercising the C99 implementations and C++17 wrappers with the same inputs
as the contract suite. The Clang build runs ASan/UBSan separately.

```sh
sudo apt-get update
sudo apt-get install -y --no-install-recommends gcc-13 g++-13 valgrind=1:3.22.0-0ubuntu3
build=$(mktemp -d)
CC=gcc-13 CXX=g++-13 cmake -S . -B "$build" -G "Ninja Multi-Config" -DBUILD_TESTING=ON
cmake --build "$build" --config Debug
CC=gcc-13 python3 tests/run_memcheck.py "$build/tests/Debug" "$build/memcheck-reports"
```

The runner first checks a clean one-byte allocation, an uninitialized byte
and a one-byte leak. Each deliberate error must produce exactly one error
with the expected diagnostic and exit code 97. During the contract suites,
Memcheck tracks uninitialized-value origins, reports invalid memory accesses and checks
for leaks. Definite, indirect and possible leaks fail the check. Reachable
blocks are reported for inspection. A failing test or Memcheck error fails
the `fixed-tlv` job and its aggregate `fixed-tlv-quality`.

The `fixed-tlv-memcheck` artifact contains the control logs, Memcheck logs
for the suites and their GoogleTest XML reports. On failure, inspect the
available logs for the first error and its origin.

Memcheck found an uninitialized read during integration test registration.
GoogleTest's default printer formatted the raw bytes of the `Layout` test
parameter, including padding. The fixture now prints the byte order and
length width, for example `BigEndian4`. OpenTLV supplies no Memcheck
suppressions. Local validation on Alpine 3.20.9 x86-64 with Memcheck
3.23.0 reported zero errors across 97 Core, 63 IO and 48 integration
cases after the printer fix. Both negative controls returned 97.
Record any new finding with the affected suite and revision.
Use the [security policy](https://github.com/OpenTLV/opentlv/security/policy)
for sensitive findings.

The [allocation probe](#heap-allocation-and-binary-size) detects allocator
calls during selected operations. [Callgrind](#fixed-tlv-instruction-counts)
counts instructions for the performance baseline.

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

## Windows x86 and x64

Use Visual Studio 2022 C++ Build Tools and the v143 14.44.35207 x86/x64
toolset to reproduce the two Windows contract builds in PowerShell.

```powershell
cmake -S . -B build-x86 -G "Visual Studio 17 2022" -A Win32 -T "v143,version=14.44.35207" -DBUILD_TESTING=ON -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded
cmake --build build-x86 --config Release --parallel
python tests/run_ctest.py build-x86 Release reports/x86 --require-x86
cmake -S . -B build-x64 -G "Visual Studio 17 2022" -A x64 -T "v143,version=14.44.35207" -DBUILD_TESTING=ON '-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded$<$<CONFIG:Debug>:Debug>'
cmake --build build-x64 --config Debug --parallel
python tests/run_ctest.py build-x64 Debug reports/x64 --require-x64
```

The test binaries link the static MSVC runtime for their configuration.
The x86 suite rejects 2³² and `UINT64_MAX` and checks that the
destination retains its value. The x64 case converts both counts exactly.
These tests exercise the conversion without allocating a large buffer.

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
