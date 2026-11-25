# C++17 API conventions and module contracts

C++17 is the only officially supported C++ language mode. C++11 and C++14
are unsupported, and newer modes are outside the required test matrix.
Include `<opentlv/tlv.hpp>` for the types and functions in `opentlv::core`,
the Fixed TLV reader and writer in `opentlv::io`, and the public C
declarations. C++17 callers can also use the C99 implementation directly
through `<opentlv/tlv.h>` or individual `.h` headers.

The [reader](../examples/read.cpp) and [writer](../examples/write.cpp)
examples use the same seven-byte record. Follow the
[build instructions](../README.md#run-the-examples) to run them.

## Using Fixed TLV from C++17

A `fixed_format` owns two settings. Its `identifier` is a `fixed_identifier`
whose `size` is the number of opaque tag bytes (at least one). Its `length`
is a `fixed_length` whose `size` is 1 through 8 bytes and whose `order` is
`byte_order::big_endian` or `byte_order::little_endian`. Set both before use.
The default widths are zero and the default order is `byte_order::unknown`.

`header_size(format, size)` writes the sum of the field widths to a
`std::size_t`. `element_size(format, element, size)` measures the complete
encoding without reading its bytes. A nonempty tag or payload with a null
pointer returns `null_arg`. A mismatched tag width returns `invalid_tag_size`.
For a valid tag and nonnull payload, a count above `SIZE_MAX` returns
`native_size`. A representable count that exceeds the length field returns
`invalid_length`.
Both sizing operations return `invalid_arg` for invalid format settings and
`overflow` if the sum of the sizes exceeds `SIZE_MAX`. On any failure, they
preserve `size`.

```cpp
#include <opentlv/core/formats/fixed.hpp>

namespace core = opentlv::core;

bool fits_example()
{
    const std::uint8_t tag[] = {0xa5};
    const std::uint8_t payload[] = {0x10, 0x20, 0x30};
    const core::fixed_format format{{1}, {2, core::byte_order::big_endian}};
    const core::element_view element{{tag, sizeof tag}, {payload, sizeof payload}};
    std::size_t required{};
    return core::element_size(format, element, required) == core::result::ok
        && required == 6;
}
```

### Converting a length field

Use `read_length(field, input, length, consumed)` to decode the first
`field.size` bytes of a `length_view`. On success, `length` receives the
64-bit logical count and `consumed` the field width. Give the two outputs
separate storage. On `truncated`, `consumed` receives the available input
size and `length` keeps its value. Argument errors preserve both outputs.

`write_length(field, length, output, written)` encodes the count into a
`mutable_byte_view` and sets `written` to the field width. Extra destination
bytes retain their values. A null, zero-size destination (`{}`) queries the
width for that count. A nonnull destination shorter than the field returns
`buffer_too_short` for a valid field and count. A count too large for the
width returns `invalid_length`. All errors preserve both the bytes and
`written`, which must occupy separate storage. Both field operations return
`invalid_arg` for an invalid width or order and `null_arg` for a null buffer
with a nonzero extent. See the [Fixed TLV wire layout](fixed-tlv.md).

## Reading elements

Include `<opentlv/io/reader/reader.hpp>` and link `OpenTLV::io`. For a
one-shot read, pass the format, the input byte view and two outputs as in
this example.

```cpp
#include <opentlv/io/reader/reader.hpp>

namespace core = opentlv::core;
const std::uint8_t wire[] = {0xa5, 0x01, 0x10, 0xb6, 0x00};
const core::fixed_format format{{1}, {1, core::byte_order::big_endian}};
core::element_view element;
std::size_t consumed{};
const auto status = opentlv::io::read(format, {wire, sizeof wire}, element, consumed);
```

Here `status` is `core::result::ok`, the tag and value point to `wire[0]`
and `wire[2]`, and `consumed` is 3. An empty value points just past its
header. With a valid format, empty input returns `core::result::end`.
A partial tag, length or value returns
`core::result::truncated`. Both outputs retain their values on end or error.

For a stream, construct an `opentlv::io::reader` and check that
`reader.init(format, input)` succeeds. Initialization copies the format,
borrows the input and sets the byte position to zero. A failed initialization
preserves the previous cursor. `reader.format()` returns the saved format
by value. To reuse it, call `reader.init(reader.format(), new_input)`.

Loop while `reader.next(element)` returns `core::result::ok`. On each
success, `reader.position()` gives the offset immediately past the element.
At `core::result::end` or on error, the element and position keep their
previous values. Cursor copies advance independently. A returned element
remains usable after the reader advances or goes out of scope. Keep the input
bytes alive, unchanged and at the same address through the last use of any view.
See the [runnable reader example](../examples/read.cpp).

To locate a truncated field, pass a pointer to a `core::diagnostic` as the
last argument to `read` or `next`. Its `field` identifies the tag, length or
value, and `required` and `available` give the byte counts for that field.
The field's `offset` is relative to the supplied input for `read` and to the
start of the stream for `next`. Both operations refresh the diagnostic on
every call, including success and end. Give the element, consumed count and
optional diagnostic separate storage outside the input bytes and the reader.

## Writing elements

Include `<opentlv/io/writer/writer.hpp>` and link `OpenTLV::io`.
`opentlv::io::write(format, output, element, written)` encodes one element
into a `core::mutable_byte_view`. On `core::result::ok`, the first
`written` bytes contain the tag, encoded length and value. On
`buffer_too_short`, `written` gives the exact capacity required, including
when `output` is `{nullptr, 0}`. Other errors preserve the count. Every
error preserves the destination. To measure before writing, call
`core::element_size(format, element, size)`.

To append elements, construct an `opentlv::io::writer` and call
`writer.init(format, output)`. Success copies the format, borrows the
writable storage and sets the position to zero. Failed initialization
preserves the previous stream and position. `writer.format()` returns the
saved format by value. Each successful `writer.write(element)` advances
`writer.position()` by the encoded size. Before the first successful
`init`, an append returns `invalid_arg`. Any failed append preserves
the committed position and the entire buffer.

Both write operations accept a trailing `core::diagnostic*`. On short
output, `field::element` identifies the whole element. Its `required`
count gives the encoded size and `available` gives the remaining capacity.
The offset is zero for the one-shot write and the current byte position for
an append. For both operations, keep the source bytes and descriptors
disjoint from the range being written. Store the cursor, count and diagnostic
separately from each other, the input descriptors and the buffers.

## Headers, namespaces and linking

| Module | Namespace for C++ declarations | Public header location | Link target |
| --- | --- | --- | --- |
| Core | `opentlv::core` | `<opentlv/core/...>` | `OpenTLV::core` |
| IO | `opentlv::io` | `<opentlv/io/reader/...>` and `<opentlv/io/writer/...>` | `OpenTLV::io` |

Core owns byte views, element and format types, checked sizes, fixed-field
conversion, results and diagnostics. It can be used on its own. IO adds
bounded one-shot and cursor reads and writes. It uses Core's types and
links Core transitively. The reader and writer headers can each be included
independently. The umbrella includes both.

## Core types and sizes

The table lists the types in `opentlv::core` and the individual headers to
include beneath `opentlv/core/`. All byte pointers in views are read-only
except `mutable_byte_view::data`. The default byte views have a null pointer
and a zero count.

| Header | Public types | Contents |
| --- | --- | --- |
| `bytes.hpp` | `byte_view`, `mutable_byte_view` | `data` points to the start of an input or output buffer. `size` is its extent or capacity in bytes (`std::size_t`). |
| `tag.hpp` | `tag_view` | `data` and native `size` describe the tag bytes in wire order, including any leading zeros. |
| `length.hpp` | `length_view` | `data` and native `size` describe the encoded length bytes. |
| `value.hpp` | `value_view` | `data` points to the payload and `size` is its `logical_size` in bytes. |
| `element.hpp` | `element_view` | `tag` is a `tag_view` and `value` is a `value_view`. |
| `size.hpp` | `logical_size` | Unsigned 64-bit byte count (`tlv_size_t`). |
| `endian.hpp` | `byte_order` | Wire order of the length field. Tag and payload bytes retain their order. |
| `field/fixed.hpp` | `fixed_identifier`, `fixed_length` | Tag width and length-field settings. |
| `formats/fixed.hpp` | `fixed_format` | Tag and length configurations for a stream. |
| `result.hpp` | `result`, `category` | An operation's status and its classification. |
| `diagnostic.hpp` | `field`, `diagnostic` | The affected part of an element and byte-level IO details. |

Use `make_value_view(data, size, out)` to create a checked `value_view` or
`validate(value)` to check an existing one. Both return `null_arg` for a
null pointer with nonzero size and `native_size` if a nonnull view's count exceeds
`SIZE_MAX`. An empty view may have a null or nonnull pointer.
`make_value_view` preserves the supplied pointer on success and the previous
`out` on failure. Validation only inspects the descriptor. The caller
supplies at least `size` readable bytes when accessing a payload.

`size_to_native(count, out)` converts a `logical_size` to a `std::size_t`,
returning `native_size` if `count` exceeds `SIZE_MAX`. After success, check
the converted count against the available buffer extent before accessing bytes.
`size_add(a, b, out)` adds two logical counts, returning `overflow` if the
sum exceeds `UINT64_MAX`. Both operations preserve `out` on failure.

## Header conventions

Use `.hpp` for C++ headers and place them alongside the corresponding
`.h` interfaces, for example `<opentlv/core/formats/fixed.hpp>` for a
typed fixed format. Each header has its own `OPENTLV_…_HPP` guard, includes
the public declarations and standard headers it needs, and compiles as the
first include in a C++17 translation unit. Use C++17 nested namespace
syntax, for example `namespace opentlv::core { /* declarations */ }`,
and qualify names from other namespaces.

Use lower-case `snake_case` for C++ types, functions, members and scoped
enumerators. Names describe the domain within their namespace, for example
`byte_order`. Reserve the `tlv_` prefix and `_t` typedef suffix for C
declarations. Suffix borrowed byte descriptor names with `_view` so a
caller can recognize their lifetime at a call site.

## Types and C interoperability

Implement wrappers inline in public C++ headers. Include the owning
module's public C headers at global scope and call the C implementation
for validation, checked arithmetic, decoding and encoding. A C-only build
continues to require only a C compiler. Both languages link the same
exported functions with C linkage. See the [public C API](architecture.md).

Use distinct types for tag, encoded length and payload views, and for
read-only and writable buffers. Use scoped enums for finite choices such
as byte order, result and diagnostic field. Use references for required
objects and a pointer for an optional output such as a diagnostic.
Native buffer extents and offsets use `std::size_t`. Logical payload sizes
retain the full range of `tlv_size_t` (64 bits). Before indexing memory,
check both representability in `std::size_t` and the available buffer
extent.

`core::to_c(x)` returns the corresponding C type by value, and
`core::from_c(x)` returns the C++ type by value. Both are `constexpr`
and `noexcept`. Each pair of types below has both overloads.

| C++ type | C type |
| --- | --- |
| `tag_view` | `tlv_tag_t` |
| `length_view` | `tlv_length_t` |
| `value_view` | `tlv_value_t` |
| `element_view` | `tlv_element_t` |
| `byte_order` | `tlv_byte_order_t` |
| `fixed_identifier` | `tlv_fixed_identifier_t` |
| `fixed_length` | `tlv_fixed_length_t` |
| `fixed_format` | `tlv_fixed_format_t` |
| `result` | `tlv_result_t` |
| `category` | `tlv_result_category_t` |
| `field` | `tlv_field_t` |
| `diagnostic` | `tlv_diagnostic_t` |

For example, pass the addresses of `auto c_format = core::to_c(format)`
and `auto c_element = core::to_c(element)` to the C writer. Converted
views borrow the same backing bytes. Configuration and diagnostic conversions
copy each field. When converting byte order to C, any unsupported order
maps to `TLV_BYTE_ORDER_UNKNOWN`. When converting byte order from C, the
numeric value is preserved.

Use native C objects at the call boundary to keep wrapper layout
independent of the C ABI. Add C and C++ entry points for new wire operations
together so both use the same implementation and produce the same bytes.

## Ownership and lifetimes

- Copying a **tag, encoded length, payload or element view** copies its descriptor.
  Keep the backing bytes alive, at the same address and unchanged through the
  last use of any view.
- A **reader** copies the format and borrows the input. Keep that input alive
  and unchanged while using any reader or view. Copies advance independently.
  Advancing or destroying a reader preserves the views it has returned.
- A **writer** copies the format and borrows a fixed-capacity output buffer.
  Keep the buffer alive, writable and at the same address through the last
  write. The cursor supports neither copying nor moving. An append borrows
  its source bytes for the call.
- A **diagnostic** owns its scalar details. The caller can inspect it after
  releasing the input or output buffer.

Foundation operations use caller-provided storage and perform no dynamic
allocation. Destroying a view or cursor never releases its buffer. Supply
borrowed views from storage that survives their use. For example, growing
a vector can invalidate a reader and every element it returned. If a
future type acquires a resource, give it RAII ownership and document its
copy, move and destruction behavior.

## Results and exceptions

Checked C++ operations, including cursor initialization, are `[[nodiscard]]`
and `noexcept`. Their `core::result` return value preserves the C result code.
For example, a reader returns `core::result::end` at an element boundary and
`core::result::truncated` when input ends inside an element. No exception
may cross the C ABI.

Use `core::result_category(code)` to group results that share recovery logic.
For logging, `core::result_string(code)` returns a read-only C string valid
for the lifetime of the program. Both lookups accept codes representable by
`tlv_result_t`.

| `core::category` | `core::result` | Meaning |
| --- | --- | --- |
| `success` | `ok` | The operation completed. |
| `end` | `end` | The reader exhausted its input at an element boundary. |
| `invalid_argument` | `null_arg`, `invalid_arg` | A required pointer is null, or a format or cursor is invalid. |
| `malformed_data` | `invalid_tag_size`, `invalid_length` | The supplied tag width or payload length does not fit the format. |
| `truncated` | `truncated` | Input ends inside a tag, length or payload. |
| `capacity` | `buffer_too_short` | The destination is too small. |
| `range` | `native_size`, `overflow` | A logical count exceeds `SIZE_MAX`, or a sum exceeds its result type's maximum. |
| `unknown` | Any unassigned code | `result_string` returns `"unknown result"`. |

Pass an optional `core::diagnostic` pointer to a one-shot read or write,
`reader.next`, or `writer.write` for byte-level details. Its `result`
matches the return value on every call. On truncation, `field` is
`field::tag`, `field::length` or `field::value`. A short write sets
`field::element`. The `std::size_t offset` gives the start of that field
or element relative to the one-shot buffer or the original cursor buffer.
The `std::uintmax_t required` and `available` counts give its full byte
size and the bytes available at that offset. Every other result sets
`field::none` and zeros all three counts.

For example, if an append starts at offset 4 and needs five bytes but only
two remain, the diagnostic contains `core::result::buffer_too_short`,
`core::field::element`, `offset == 4`, `required == 5` and `available == 2`.

## Checking the contract

Run `python tests/check_public_api.py` from the repository root to compile
each public header on its own. The check compiles every `.h` header in
C99 and C++17, and every `.hpp` header in C++17. Rejection probes check
the C++ umbrella's diagnostic in C++11 and C++14.
The [package checks](architecture.md#verification) build and run C99 and C++17
consumers against the build tree and a relocated installation for both static
and shared libraries. The same checks build the [documented examples](../README.md#run-the-examples)
against the relocated installation and compare their output with the expected bytes.

When adding a wrapper, exercise it alongside its C operation on the same
input and compare wire bytes, results, diagnostic details and output state
on success, end and failure as applicable.
