# OpenTLV

OpenTLV is a C99 library under development for processing type-length-value
(TLV) data. Include `<opentlv/tlv.h>` in C99 or `<opentlv/tlv.hpp>` in C++17 to
[read fixed TLV elements](docs/fixed-tlv.md#reading-elements),
[write them into caller-provided buffers](docs/fixed-tlv.md#writing-elements),
convert fixed-width lengths, and measure encoded sizes.
The [fixed TLV data model](docs/fixed-tlv.md) gives the wire layout and size
limits. [Result categories and diagnostics](docs/fixed-tlv.md#result-categories-and-diagnostics)
show how to locate an incomplete field or size a write retry.
The [architecture guide](docs/architecture.md)
describes the Core and IO modules, buffer ownership, API stability, and how
to check the headers.

The C++17 umbrella exposes typed views and Fixed TLV configuration in
`opentlv::core` and a reader and writer in `opentlv::io`. The
[C++ API guide](docs/cpp-api.md) covers buffer lifetimes, reading and
writing streams, and using diagnostics to locate a partial field or size a write.

OpenTLV supports fixed-width tag and length fields with opaque payloads.
Variable-width field encodings, automatic nested decoding, schemas, queries,
EMV processing, bindings for other languages, and single-file amalgamation
are outside the Fixed TLV Foundation.

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

Build the Core, IO and integration suites with a C99 compiler, a C++17
compiler and CMake 3.18 or newer. The first test configure downloads GoogleTest.

```sh
cmake -S . -B build-debug -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug --config Debug
cd build-debug
ctest -C Debug --output-on-failure
cd ..
```

The [testing guide](docs/testing.md) covers offline setup, CTest filters and
reports, sanitizers, coverage, 32-bit execution and fuzz reproduction.
The [benchmark workload catalog](docs/testing.md#benchmark-workload-catalog)
provides equivalent C99 and C++17 inputs. The [Google Benchmark runner](docs/testing.md#runtime-and-throughput)
measures Core and IO latency and throughput and writes JSON reports.

## Run the examples

After installing OpenTLV, run these commands from the repository root with
a C99 and a C++17 compiler. Use the same installation prefix as above.

```sh
cmake -S examples -B build-examples -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/path/to/opentlv
cmake --build build-examples --config Release
cd build-examples
ctest -C Release -V
```

Both writers encode the tag `12 34` and a three-byte payload. The C++ reader
decodes the same seven wire bytes. CTest runs all three programs and shows
their output.

| Example | Output |
| --- | --- |
| [C99 writer](examples/write.c) | `12 34 00 03 AA BB CC` |
| [C++17 writer](examples/write.cpp) | `12 34 00 03 AA BB CC` |
| [C++17 reader](examples/read.cpp) | `tag 12 34, value AA BB CC` |

## Use the installed package

Link `OpenTLV::io` to use the reader and writer. It also links Core.
For just the fixed TLV model, size calculations and result codes, link
`OpenTLV::core` directly.

```cmake
find_package(OpenTLV CONFIG REQUIRED)
add_executable(example main.cpp)
target_link_libraries(example PRIVATE OpenTLV::io)
```

Configure the consumer with `-DCMAKE_PREFIX_PATH=/path/to/opentlv`.
Both targets supply the public include directory and request C++17 for C++
compilation. Include a module header such as `<opentlv/io/writer/writer.hpp>`
or use the umbrella `<opentlv/tlv.hpp>`. A C99 project can link the same
targets and include `<opentlv/tlv.h>` with only the C language enabled.

## Use the build-tree package

Build OpenTLV, then configure the consumer with
`-DOpenTLV_DIR=/absolute/path/to/build`. The same `find_package` and target
names work before installation. Keep the OpenTLV source and build directories
in place. On Windows, put the directory containing the built DLLs on `PATH`
when running a shared-library consumer.

## Contributing

The [CI guide](docs/governance.md#continuous-integration) lists the pull request
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
