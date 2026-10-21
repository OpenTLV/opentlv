![OpenTLV - Tag, Length, Value](docs/assets/opentlv.svg)

[![Build and tests](https://github.com/OpenTLV/opentlv/actions/workflows/build-gcc.yml/badge.svg?branch=main)](https://github.com/OpenTLV/opentlv/actions/workflows/build-gcc.yml)
[![Documentation](https://github.com/OpenTLV/opentlv-docs/actions/workflows/docs.yml/badge.svg?branch=main)](https://opentlv.github.io/opentlv-docs/)

**Read, write and inspect binary Tag-Length-Value data.**

OpenTLV is a reusable TLV toolkit for embedded software, protocol handlers and
binary inspection tools. Choose the wire format, then read elements, write
messages or edit an owned document. Optional Query, Schema and Codec operations
help you select elements, validate structure and interpret values.

Use C99, the idiomatic C++11+ API, or the official Rust, [Python](https://github.com/OpenTLV/opentlv-python), [Lua](https://github.com/OpenTLV/opentlv-lua) and Go
bindings (experimental). The standalone [`otlv` CLI](https://github.com/OpenTLV/opentlv-cli)
and WebAssembly tools use the same C execution engine.

## Why OpenTLV?

OpenTLV began with a simple question: which parser should I use for TLV data?
The answer I received was: none. That experience motivated a reusable,
format-independent foundation for working with TLV data.

## What you can do

- Inspect values without copying their bytes using borrowed Reader results.
- Traverse nested messages without building an owned tree.
- Write messages into caller-owned buffers, or use language conveniences.
- Own, edit and re-encode data with Document.
- Compose only the processing features and format components you need.

C Reader, Writer and tree traversal use explicit buffers and workspaces without
allocating. Document owns storage and allocates; C++ and binding conveniences
may also allocate. Raw parsing does not validate an entire protocol.

```text
bytes --Reader + Format--> Element --Writer + Format--> bytes
                              |
                       own and edit: Document
```

Schema checks structure and Codec interprets Value bytes; both are optional.
See the [basic mental model](docs/concepts/learning-model.md).

## Quick start

With [`otlv` installed separately](https://github.com/OpenTLV/opentlv-cli#standalone-build), inspect a tag `01` containing the UTF-8 bytes of `Hi`:

```sh
otlv dump --format fixed --hex "01 02 48 69"
```

```text
offset=0 tag=01 length=2 value=4869
```

Fixed defaults to one tag byte and one big-endian length byte. Select the format
that matches your input; OpenTLV does not guess it.

For a first library program, follow [Getting started](docs/getting-started/README.md)
with runnable C, C++, Rust, Python, Lua and Go examples, normal error handling
and the lifetime rules needed to use them safely.

## Build and integrate

```sh
git clone https://github.com/OpenTLV/opentlv.git
cd opentlv
cmake -S . -B build -DOPENTLV_BUILD_TESTS=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

Requires CMake 3.16+ and a C99 compiler; the default C++ facade needs C++11+.
The C engine and C++ facade have no third-party dependencies. Core does not
build or install `otlv`; build it from [opentlv-cli](https://github.com/OpenTLV/opentlv-cli).
Build and install Lua separately from [opentlv-lua](https://github.com/OpenTLV/opentlv-lua).
See [integration and installation](docs/getting-started/README.md) for CMake
consumer targets, runtime libraries and binding-specific setup.

## Documentation

Read the [documentation portal](https://opentlv.github.io/opentlv-docs/) for
versioned guides and generated C/C++ API references. Technical Markdown remains
in this repository; the links below also work when browsing the sources.

- [Start here](docs/index.md): choose a language or tool and follow the learning path.
- [Read, write or edit](docs/guides/processing.md): choose an API for your task.
- [Formats and supported capabilities](docs/formats/support.md): built-ins,
  limitations and extension points. Application-defined formats are supported;
  support for every TLV format is not implied.
- [API reference](docs/reference/README.md): exact behavior and contracts.
- [Contributing](CONTRIBUTING.md): build, test and develop OpenTLV.

## Project status

The native execution APIs and tools are implemented; their precise scope is in
[the capability inventory](docs/formats/support.md) and
[binding status](docs/concepts/bindings.md). The [roadmap](ROADMAP.md) defines
four architectural phases: Execution
Foundation, Runtime Model & OTLV, Protocol Inference, and Compilation. The latter
three are planned capabilities built on the same execution contracts; phase
numbers are independent of SemVer major versions.
See [releases](https://github.com/OpenTLV/opentlv/releases) and
[the changelog](CHANGELOG.md) for version history.

OpenTLV is distributed under the [MIT license](LICENSE).

<!-- Preserve established repository heading links. -->
<!-- markdownlint-disable MD033 -->
<a id="who-is-it-for"></a>
<a id="format-and-standard-support"></a>
<a id="candidate-formats-not-implemented"></a>
<a id="long-term-direction"></a>
<a id="releases-and-contributing"></a>
<!-- markdownlint-enable MD033 -->

Previous detailed sections are now reached through [format support](docs/formats/support.md),
[format candidates](docs/formats/format-catalogue.md), [the roadmap](ROADMAP.md)
and [contributing](CONTRIBUTING.md).
