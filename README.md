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
sanitizer checks, and coverage report.

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
