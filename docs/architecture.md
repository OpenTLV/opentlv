# Library architecture and public C API

The [Fixed TLV Foundation](https://github.com/OpenTLV/opentlv/issues/3)
defines a C99 interface for C and C++17 callers. Include `<opentlv/tlv.h>`
to use the fixed TLV data model, reader, writer, and public compiler macros.
The [C++ API conventions](cpp-api.md) define the C++17 header layout and
the contracts for wrappers around Core and IO.

## Modules and dependencies

Core owns format-independent representations, wire-format rules and shared
result codes. IO depends on Core. Core can be used on its own. The fixed
format defines how to encode an element and determine its extent in a
buffer. The reader and writer use those rules and manage their own cursors.
Protocol-specific interpretation stays outside both modules.

Core defines the result codes, their categories and messages, and the
`tlv_diagnostic_t` record. Validators can report a result without IO.
The reader and writer supply offsets and byte counts when they encounter
incomplete input or insufficient output capacity. The record contains only
scalar values, so callers can retain it after releasing their buffer.
See [result categories and diagnostics](fixed-tlv.md#result-categories-and-diagnostics).

| Foundation work | Public header location | Tracking issue |
| --- | --- | --- |
| Tag, length, value, fixed encoding and size bounds | `opentlv/core/` | [#5](https://github.com/OpenTLV/opentlv/issues/5) |
| Bounded input and read cursor | `opentlv/io/reader/` | [#6](https://github.com/OpenTLV/opentlv/issues/6) |
| Bounded output and write cursor | `opentlv/io/writer/` | [#7](https://github.com/OpenTLV/opentlv/issues/7) |
| Shared results and diagnostic details | `opentlv/core/` | [#8](https://github.com/OpenTLV/opentlv/issues/8) |

## Public and private interfaces

C99 is the implementation baseline. C++17 callers can include the same C
headers. Each module header includes its prerequisites and has an
`OPENTLV_…_H` include guard. Headers declaring external functions wrap those
declarations in `extern "C"` under `#ifdef __cplusplus`. Compile the
implementation as C and link it using a compatible C ABI.

Use `tlv_` for public C functions, `tlv_…_t` for public typedefs and
`TLV_` for result constants and feature macros. The umbrella header exposes
`TLV_C_VERSION` and three portable attributes. Mark functions whose results
should be checked with `TLV_NODISCARD`, intentionally unused declarations
with `TLV_MAYBE_UNUSED`, and intentional fallthrough between switch cases
with `TLV_FALLTHROUGH;`. OpenTLV reserves all three attribute names.
Predefining any of them before including the header is a compilation error.

Expose `include/` as the consumer include path. Keep each module's public
headers under `include/opentlv/<module>/`, implementations in
`src/<module>/`, and shared private headers in `src/<module>/internal/`.
Public headers may include standard headers and public dependencies.
Use `static` for file-local helpers. For shared-library builds, only
public symbols receive the owning module's `OPENTLV_<MODULE>_API` export
annotation. Private headers and symbols are outside the public API.

## Ownership contract

Fixed TLV Core and IO use caller-provided storage in both C99 and C++17.
They allocate no heap memory. The caller owns the buffers and controls
when to reuse or release them. A format descriptor only needs to survive
the one-shot operation or cursor initialization that receives it.

- **Views.** Copying a tag, encoded length, value or element descriptor, including
  converting it between C and C++, copies its pointers and counts. The source
  descriptor can go out of scope. Keep the backing bytes alive, at the same
  address and unchanged through the last use of any view.
- **Readers.** A successful read returns a zero-copy view. The tag points to the
  start of the element and the value points just past its header, even for an
  empty value. Keep the input alive, unchanged and at the same address while
  using any reader or returned view. Each reader stores its format and position
  by value. A view stays valid after the reader advances or goes out of scope.
  Copied readers advance independently. Reinitializing one binds it to a new
  stream while its copies and earlier views still refer to the original input.
- **Writers.** A writer stores its format by value and borrows a fixed-capacity
  buffer. Keep the buffer at the same address and writable through the last
  write. Each successful write copies the tag and payload into that buffer.
  The source descriptors and bytes need only survive the call. A copied C
  writer has its own position and shares the destination, so writes from the
  same position can overwrite one another. The C++17 writer disables copying
  and moving.

### Aliasing and bounds

Reader and writer state must reside outside their respective buffers. For a
read, keep the output element and one-shot consumed count separate from each
other, the input bytes and the cursor. For a write, keep the source descriptors
and their bytes disjoint from the range being encoded. For an append, the
source element and its bytes must also be disjoint from the writer object.
The tag and payload may share source bytes. An append can also use bytes
from a committed prefix while writing into the suffix. Keep the written
count separate from the destination and the sources. An optional diagnostic
must occupy storage separate from the cursor, buffers, descriptors and counts.
For a length-field read, give the decoded length and consumed count separate
storage. For a length-field write, keep the written count outside the destination.
When reinitializing a C cursor, its own `format` member is a valid format input.

Value validation and element measurement inspect descriptors without reading
tag or payload bytes. A `tlv_size_t` payload count is 64 bits. Before using
it to access memory, check that it fits both `size_t` and the actual buffer.
The caller supplies the readable or writable extent advertised by each IO buffer.

### Outputs and cursor position

The table applies to both APIs, including IO calls with diagnostics.

| C99 / C++17 operation | On success | On end or error |
| --- | --- | --- |
| `tlv_value_init` / `make_value_view` | Store the supplied pointer and size, including the pointer supplied for an empty view. | Preserve the previous view. |
| Checked size conversion, addition, header and element measurement | Store the converted size, sum or encoded size. | Preserve the output count. |
| `tlv_fixed_length_read` / `read_length` | Store the decoded length and set `consumed` to the field width. | On truncation, set `consumed` to the available byte count and preserve the length. Other errors preserve both. |
| `tlv_fixed_length_write` / `write_length` | Set `written` to the field width. Encode exactly that many bytes when a destination is supplied. A null, zero-capacity destination queries the width. | Preserve every destination byte and `written`, including on short capacity. |
| Reader or writer initialization | Copy the format, bind the buffer and set the position to zero. Writer initialization preserves the buffer contents. | Preserve the previous cursor, including its buffer, format and position. |
| `tlv_read` / `read`, `tlv_reader_next` / `reader::next` | Return a borrowed element. The one-shot count or cursor advance covers exactly one complete element. | Preserve the element and the one-shot count or cursor position, including on truncation and end of input. |
| `tlv_write` / `write` | Encode one complete element, set `written` to its size and preserve trailing bytes. | Preserve every destination byte. Short capacity sets `written` to the full required size, including for a null, zero-capacity destination. Other errors preserve `written`. |
| `tlv_writer_write` / `writer::write` | Append one complete element at the current position and advance by its size. | Preserve the position and the entire destination, including the committed prefix and unused capacity. |

Each IO read or write refreshes a supplied diagnostic, even when other outputs
are preserved. It owns its scalar details and can outlive the buffer. For a
truncated read or short write, the offset is relative to the one-shot buffer
or the start of the cursor's buffer. Other results record the status with
`TLV_FIELD_NONE` and zero offset and counts.
See the [wire layout and diagnostic examples](fixed-tlv.md) and the
[contract test inventory](test-scenarios.md).

## Compatibility

The [v0.1.0 API and ABI baseline](api-abi.md) lists the Core and IO interfaces,
records the native layout assumptions and describes how to verify the exports.

Header paths, function signatures, public type layouts and documented
behavior form the public contract. Before 1.0, intentional breaking changes
must be identified in the pull request and changelog, with migration guidance
for callers. From 1.0 onward, preserve source compatibility and the public
C ABI within a major version on the same supported platform and ABI.
Removing an exported function or changing a public struct's layout requires
a new major version.

## Verification

The Core, IO and integration suites link the production library targets and
exercise their public C interfaces. The Core and IO suites also check the typed
C++17 views, Fixed TLV operations and reader against the C implementation.
Test builds use GoogleTest to discover individual cases through CTest. The
[testing guide](testing.md) explains how to select a contract, reproduce CI
runs and read their reports.

Run `python tests/check_public_api.py` from the repository root with Python
3 and a GCC- or Clang-compatible toolchain. Set `CC` and `CXX` to select
the C and C++ compiler executables. To use Clang's Microsoft-compatible
driver, run `python tests/check_public_api.py --clang-cl clang-cl`.
The check gives each header its own translation unit and includes it twice
to exercise its dependencies and include guard. It checks public `.h` files
and C API attribute diagnostics in C99 and C++17. It also exercises C11,
C17 and C23 when the compiler provides those modes. All public `.hpp`
headers must compile in C++17. Separate probes verify that including the
C++ umbrella in C++11 or C++14 reports the required standard.

Run the package checks with CMake, a native build tool, and C99
and C++17 compilers on the path.

```sh
python tests/check_cmake_package.py
python tests/check_cmake_package.py --shared
```

These Release checks build the production libraries with an unavailable
`CXX` compiler and FetchContent disconnected. Each C99 consumer build also
checks that CMake enabled only C. For release validation on Linux and Windows,
add `--check-symbols` to both commands to compare each installed library
with its [C ABI inventory](api-abi.md#visibility-and-verification).

The consumer projects build OpenTLV from the candidate checkout with
`add_subdirectory` and with `FetchContent`. Both enable the parent's tests
and check that OpenTLV's own test targets stay disabled. The package
consumers request the exact `0.1.0` version with `find_package` at three locations.
They first build and run against the build tree. After installation the check
removes the temporary library source and build trees, then builds and runs
fresh consumers at the original prefix. It moves the installation and repeats
the same checks at the new prefix. The checks verify that the Core and IO
target types match the selected static or shared build. Each project links
one executable to `OpenTLV::core` and another to `OpenTLV::io` and runs
both through CTest.
The C++17 IO consumer writes and reads an element using the module headers
and calls Core through IO's transitive dependency.

The checks disable GTest discovery in the consumer projects and look for test
artifacts in the install tree. They inspect installed CMake files for test
dependencies and paths to the source, build or original installation. They
also run the [documented examples](../README.md#run-the-examples) against
the relocated package and verify each program's output.

The epic tracks behavioral test and coverage infrastructure in
[#10](https://github.com/OpenTLV/opentlv/issues/10),
CI workflows in [#11](https://github.com/OpenTLV/opentlv/issues/11), and
the usage guide and runnable examples in
[#12](https://github.com/OpenTLV/opentlv/issues/12).
