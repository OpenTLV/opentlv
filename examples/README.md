# OpenTLV Library examples

Examples are grouped by the public module they demonstrate. C and C++ versions
live together and use `.c` and `.cpp` extensions. Protocol-specific programs and
the unchanged NFC Type 2 binary fixtures live under `builtins/`.

Build and run from the repository root:

```sh
cmake -S . -B build -DOPENTLV_BUILD_EXAMPLES=ON
cmake --build build --config Release
ctest --test-dir build -C Release -L examples --output-on-failure
```

Set `OPENTLV_BUILD_CXX=OFF` to build only C examples. Text Query examples are
skipped when `OPENTLV_QUERY_FRONTEND=OFF`; configuration reports each skip.

Targets use `example-<lang>-<module>-<name>` (`c` or `cpp`), and CTest names use
`example.<lang>.<module>.<name>` with labels `integration;examples`. For example,
`io/quick_start.c` builds as `example-c-io-quick-start` and
`builtins/nfc/type2/type2.c` builds as `example-c-nfc-type2`.

Each directory declares its sources and public `LINK` targets using the thin
`opentlv_example()` helper in `cmake/Example.cmake`. `SOURCES` accepts multiple
files; optional `ARGS` and `PASS_REGULAR_EXPRESSION` preserve runtime checks.
The [complete source inventory](../docs/guides/examples.md#source-inventory)
lists the runnable programs.

## Standalone installed-package build

The same directory is a standalone CMake 3.16 project. It can be copied outside
the checkout, including `cmake/` and the binary fixtures, and built against an
installed OpenTLV package. For example, from the repository root:

```sh
cmake -S . -B build/core -DOPENTLV_BUILD_TESTS=OFF -DOPENTLV_BUILD_EXAMPLES=OFF
cmake --build build/core --config Release
cmake --install build/core --config Release --prefix "$PWD/build/install"
cmake -S examples -B build/examples-installed -DCMAKE_PREFIX_PATH="$PWD/build/install"
cmake --build build/examples-installed --config Release
cmake -E chdir build/examples-installed ctest -C Release -L examples --output-on-failure
```

Pass `-DOPENTLV_BUILD_CXX=OFF` to the examples configuration to enable only C.
The installed Query feature header controls whether the text Query examples
are built; a package built with `OPENTLV_QUERY_FRONTEND=OFF` skips them explicitly.
The umbrella quick start is also skipped explicitly if the optional `tlv`
component is unavailable. All other declared components and targets are required;
broken exports fail configuration rather than silently removing examples.

For a single C program, copy `io/quick_start.c` next to this `CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.16)
project(HelloTLV LANGUAGES C)
find_package(OpenTLV CONFIG REQUIRED COMPONENTS Core IO)
add_executable(hello_tlv quick_start.c)
target_link_libraries(hello_tlv PRIVATE OpenTLV::Core OpenTLV::IO)
```

Configure with `-DCMAKE_PREFIX_PATH=/path/to/opentlv-install`. Public targets
supply include paths, language requirements and transitive dependencies.
CTest sets the DLL search path automatically for Windows shared builds. When
running executables manually on Windows, add the package's `bin` directory
(or the in-tree Core DLL directory) to `PATH`.

The NFC Type 2 program accepts a file path as its first argument (up to 1024
bytes); without arguments it uses its original embedded sample. CTest supplies
the copied `sample.bin` by absolute path, independent of the working directory.
The binary fixtures are unchanged.

`scripts/check_cmake_integration.py` builds and runs a copied examples tree
against a relocated installation, both with and without the optional aggregate.
It checks exact CTest names against `examples.jsonl`, filtered by the consumer
language and installed Query frontend, before running all available examples.
CI exercises CMake 3.16 on Linux; Full/release
also covers Windows shared/static, C-only consumers and a frontend-disabled
package. PR validation keeps the existing Linux-only consumer selection.

## Public dependencies

Include the public headers for the APIs the example uses and declare their
owning targets in `LINK`. Each integration header requires its own target,
such as `OpenTLV::DocumentCodec`. Dependencies used only inside those headers
come from the exported targets and should not be repeated in `LINK`.
Examples use the public C99/C++11 requirements without per-example overrides.

The C++ quick start intentionally demonstrates `<opentlv/tlv.hpp>` and links
`OpenTLV::tlv`, with `UMBRELLA` marking that choice. The existing
`io/advanced_control.cpp` interop demonstration retains public `native.hpp`
with `NATIVE_INTEROP`: there is no module-specific replacement for it yet.
Its composite dependencies are derived from that header's public includes.

The helper writes `examples.jsonl` in the examples build directory, including
declarations skipped by C++ or Query feature switches. After configuring, check
direct includes and links against the existing module ownership contract:

```sh
python scripts/check_module_contracts.py --examples build/examples/examples.jsonl
```

The check rejects private headers, missing direct links, unused links and
unregistered sources. Static Analysis CI also builds and runs the examples
with C99 and C++11 explicitly selected.

## C examples

- **Start here:** [`quick_start.c`](io/quick_start.c) -- the smallest possible round
  trip: write one element, read it back.

### Use cases

The same document is parsed, built, addressed and checked here as in
[C++ examples](#c-examples-1), [`opentlv-rust/opentlv/examples`](https://github.com/OpenTLV/opentlv-rust/tree/main/opentlv/examples/)
and [standalone JS/WASM examples](https://github.com/OpenTLV/opentlv-js/tree/main/examples/), so the same
operation can be compared across languages.

- [`parse.c`](io/parse.c) -- read a nested BER-TLV document.
- [`write.c`](io/write.c) -- build that same document from its parts.
- [`query.c`](query/query.c) -- address one field directly by path.
- [`validate.c`](schema/validate.c) -- check the document's structure without decoding it.

### API tour

One topic per file: sequential I/O, explicit copies, schema validation while
traversing, codecs and endian conversion, and a fully custom format.

- [`sequential_io.c`](io/sequential_io.c) -- a sequential writer and reader,
  `tlv_writer_copy_element`/`tlv_writer_copy_encoded`.
- [`copies.c`](io/copies.c) -- the explicit `tlv_copy_value`/`tlv_copy_encoded`/`tlv_copy_element`
  helpers.
- [`incremental_reader.c`](io/incremental_reader.c) -- caller-owned sliding input,
  resumable reads and explicit EOF using a buffer smaller than the full input.
- [`tree_reader.c`](io/tree_reader.c) -- pull-based preorder traversal with
  caller-owned frames, incremental input and subtree skipping.
- [`schema_visitor.c`](schema/schema_visitor.c) -- schema validation inside a
  `tlv_reader_visit()` callback.
- [`codecs_and_endian.c`](codec/codecs_and_endian.c) -- an application value codec, and
  the standalone endian helpers.
- [`custom_format.c`](core/custom_format.c) -- defining your own reader/writer format.

### Formats

Generic, protocol-agnostic format mechanisms.

- [`core/fixed_format.c`](core/fixed_format.c) -- a runtime-configurable
  fixed-width format, `tlv_fixed_format_t`.

### Builtins

Examples specific to one wire format or standard.

- [`builtins/asn1/ber.c`](builtins/asn1/ber.c) -- BER: multi-byte tags and
  long-form lengths.
- [`builtins/asn1/cer.c`](builtins/asn1/cer.c) -- CER: indefinite-length framing
  and segmented strings.
- [`builtins/emv/tag_decoding.c`](builtins/emv/tag_decoding.c) -- decoding a full
  EMV record with the built-in dictionary.

## C++ examples

Start with [`quick_start.cpp`](io/quick_start.cpp): scoped encoding, iterable parsing
and typed string decoding for a Fixed-format `Hello, world!` round trip,
with explicit error handling and input lifetime. All examples compile and run
through CTest with the `examples` label when their components are enabled.
See the [C++ guide](../docs/guides/cxx-examples.md) for ownership,
workspace limits, API ceremony comparisons and equivalent CLI commands.

### Common operations

- [`basic_usage.cpp`](io/basic_usage.cpp): write two primitive elements and iterate
  over the sequence; uses the built-in BER Format.
- [`write.cpp`](io/write.cpp): build the same nested document with scoped writes.
- [`document.cpp`](document/document.cpp): parse an owning Document and traverse roots
  and children (requires Document and BER).
- [`query.cpp`](query/query.cpp): select a path from a streaming Tree Reader and,
  when enabled, an owning Document.
- [`typed_fields.cpp`](codec/typed_fields.cpp): write and decode an application field,
  including `document.get<Counter>()` when Document is enabled. Typed writing
  owns temporary codec storage. No optional protocol component is required.
- [`core/fixed_format.cpp`](core/fixed_format.cpp): configure generic
  fixed-width framing, then use typed Reader and Writer.

The nested BER examples use the same wire data as the
[C](#c-examples), [Rust](https://github.com/OpenTLV/opentlv-rust/tree/main/opentlv/examples/) and
[WASM](https://github.com/OpenTLV/opentlv-js/tree/main/examples/) examples. The CLI acceptance script is
[`opentlv-cli/tests/examples.cmake`](https://github.com/OpenTLV/opentlv-cli/blob/main/tests/examples.cmake).

### Advanced control

- [`parse.cpp`](io/parse.cpp): nested borrowed Visitor traversal with bounded frames.

- [`advanced_control.cpp`](io/advanced_control.cpp): explicit codec scratch,
  incremental Reader outcomes and native C interoperability.
- [`document_edit.cpp`](document/document_edit.cpp): edit nodes, check erased handles,
  and encode an owning Document.
- [`core/custom_format.cpp`](core/custom_format.cpp): implement the
  complete C++ Format contract, including source metadata and bounded encoding.
- [`core/fixed_format_runtime.cpp`](core/fixed_format_runtime.cpp): choose
  framing at runtime through the native C API.
- [`validate.cpp`](schema/validate.cpp): explicit native structure-schema validation.
- [`lldp.cpp`](builtins/lldp/lldp.cpp): protocol validation with the LLDP facade.

Examples use C++11-compatible lambdas. Format ordering and some advanced
contracts still use native enums; the guide identifies those remaining API
requirements rather than hiding them behind example-only helpers.
