# OpenTLV

OpenTLV is a C99 library under development for processing type-length-value
(TLV) data. C99 and C++11 callers can include `<opentlv/tlv.h>` to
[read fixed TLV elements](docs/fixed-tlv.md#reading-elements),
[write them into caller-provided buffers](docs/fixed-tlv.md#writing-elements),
convert fixed-width lengths, and measure encoded sizes.
The [fixed TLV data model](docs/fixed-tlv.md) gives the wire layout, limits,
and boundary test command. [Result categories and diagnostics](docs/fixed-tlv.md#result-categories-and-diagnostics)
show how to locate an incomplete field or size a write retry.
The [architecture guide](docs/architecture.md)
describes the Core and IO modules, buffer ownership, API stability, and how
to check the headers.

The package exposes the C API directly to C++ callers. It supports fixed-width
tag and length fields with opaque payloads. Variable-width field encodings,
automatic nested decoding, schemas, queries, EMV processing, language-specific
bindings, and single-file amalgamation are outside the Fixed TLV Foundation.

## Build and install

Build the libraries with CMake 3.15 or newer and a C99 compiler. Replace
`/path/to/opentlv` with the absolute path where you want to install the package.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
cmake --install build --config Release --prefix /path/to/opentlv
```

The default build produces static libraries. For shared libraries, add
`-DBUILD_SHARED_LIBS=ON` to the configure command. On Windows, add the
installation's `bin` directory to `PATH` when running a shared-library consumer.

## Run the tests

Set `BUILD_TESTING=ON` to build the tests with CMake 3.18 or newer and
a C++17 compiler. The default library build uses only C and works offline.
The Core suite checks fixed-length reads and writes against expected wire bytes for
widths 1–8 in both byte orders. It also checks header and element sizes, borrowed
values, checked arithmetic, and the categories and messages for result codes.
The IO suite checks one-shot and cursor reads and writes. Its cases verify where
borrowed views point, which bytes a failed append preserves, and how diagnostics
report an incomplete input field or the capacity needed for a retry.

```sh
cmake -S . -B build-tests -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build-tests --config Debug
cd build-tests
ctest -C Debug -N
ctest -C Debug --output-on-failure
cd ..
```

The first CTest command lists the individual cases. The second runs all suites.
For IO alone, run `ctest -C Debug -L io --output-on-failure` from the build
directory. Use `-L core` for Core, adding `-R LittleEndian3$` to select its
three-byte little-endian cases.

Run `ctest -C Debug -L integration --output-on-failure` for Fixed TLV
integration tests. They check literal wire vectors in all 16 length layouts,
round-trip binary values with 1-, 2- and 9-byte tags, and read adjacent
elements with every possible prefix of the final element. Each layout uses
the same fixed seed (`0x26f1`) and payloads of at most 513 bytes.

To check Release, repeat the commands with `Release` in place of `Debug` and
use `build-tests-release` as the build directory.

When you first configure a test build, CMake downloads the
[GoogleTest 1.17.0 source archive] and verifies its SHA-256. To build offline, add
`-DOPENTLV_GOOGLETEST_URL=/absolute/path/to/v1.17.0.tar.gz` to the first
command. CMake checks the local archive against the same digest.
Both build modes install only the OpenTLV libraries, headers, license and CMake files.

[GoogleTest 1.17.0 source archive]: https://github.com/google/googletest/archive/refs/tags/v1.17.0.tar.gz

### Run the 32-bit x86 tests

The `windows-x86` CI job builds the libraries, GoogleTest, and the Core, IO,
and integration suites for Win32 and executes them on the Windows runner.
To reproduce it with the Visual Studio 2022 C++ build tools, the v143 14.44.35207 x86/x64
toolset, Python 3, and CTest 3.21 or newer, use a fresh build directory.

```powershell
cmake -S . -B build-x86 -G "Visual Studio 17 2022" -A Win32 -T "v143,version=14.44.35207" -DBUILD_TESTING=ON -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded
cmake --build build-x86 --config Release --parallel
python tests/run_ctest.py build-x86 Release reports/x86 --require-x86
```

The Release build links the MSVC runtime statically so the test executables
use the runtime supplied with the selected toolset. The x86-only Core case
asserts `sizeof(size_t) == 4` at compile time and
checks that converting `UINT64_MAX` fails with the output preserved. The
runner requires this case and the named boundary cases to pass in the JUnit
report. They cover `SIZE_MAX`, rejection of `SIZE_MAX + 1`, checked addition,
and truncation of a 64-bit wire length before narrowing. GitHub publishes
the JUnit report and CTest log in `ctest-reports-x86`.

## Run the examples

After installing OpenTLV, run these commands from the repository root with
a C99 and a C++11 compiler. Use the same installation prefix as above.

```sh
cmake -S examples -B build-examples -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/path/to/opentlv
cmake --build build-examples --config Release
cd build-examples
ctest -C Release -V
```

The C writer encodes a three-byte payload. The C++ reader decodes the
same seven wire bytes. CTest runs both programs and shows their output.

| Example | Output |
| --- | --- |
| [C99 writer](examples/write.c) | `12 34 00 03 AA BB CC` |
| [C++11 reader](examples/read.cpp) | `tag 12 34, value AA BB CC` |

## Use the installed package

In a C99 or C++11 CMake project, link your executable to `OpenTLV::io` for
the reader and writer. This target also links Core. For just the fixed TLV
model, size calculations and result codes, link `OpenTLV::core` directly.

```cmake
find_package(OpenTLV CONFIG REQUIRED)
add_executable(example main.c)
target_link_libraries(example PRIVATE OpenTLV::io)
```

Configure the consumer with `-DCMAKE_PREFIX_PATH=/path/to/opentlv` so CMake
can find the installation. Both targets supply the include path for
`<opentlv/tlv.h>`. See the [package checks](docs/architecture.md#verification)
for the commands that exercise C99 and C++11 callers after moving an installation.

## Contributing

See [Running the tests](docs/fixed-tlv.md#running-the-tests) for the unit suite,
sanitizer checks, and coverage report. The
[CI guide](docs/governance.md#continuous-integration) lists the pull request
checks and explains how to run Full CI.

For a bug, proposal, or maintenance task, start with the
[issue chooser](https://github.com/OpenTLV/opentlv/issues/new/choose).
Follow the shared [contribution guidelines] for branch names, commit messages,
pull requests, labels, and changelog entries. The [code of conduct] applies
to project discussions and reviews.

Ask usage questions in [OpenTLV Q&A] or consult the [support guide].
Report a suspected vulnerability privately through the [security policy].

The [repository governance record](docs/governance.md) describes how changes
reach `main` and which GitHub settings maintainers have verified.
Notable changes are listed in the [changelog](CHANGELOG.md).

## License

OpenTLV is distributed under the [MIT License](LICENSE).

[contribution guidelines]: https://github.com/OpenTLV/.github/blob/main/CONTRIBUTING.md
[code of conduct]: https://github.com/OpenTLV/.github/blob/main/CODE_OF_CONDUCT.md
[OpenTLV Q&A]: https://github.com/OpenTLV/opentlv/discussions/categories/q-a
[support guide]: https://github.com/OpenTLV/.github/blob/main/SUPPORT.md
[security policy]: https://github.com/OpenTLV/opentlv/security/policy
