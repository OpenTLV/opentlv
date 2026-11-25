# C++17 API conventions and module contracts

C++17 is the supported language mode for the C++ API. Its umbrella,
`<opentlv/tlv.hpp>`, exposes the Core types, the Fixed TLV reader and writer in
`opentlv::io`, and the public C declarations.

C++17 callers can also use the C interface directly through
`<opentlv/tlv.h>` or individual `.h` headers. Newer C++ modes are
outside the required test matrix.

## Using Fixed TLV from C++17

A `fixed_format` specifies the tag width, length width and length byte order.
`header_size` validates those settings. `element_size` also checks that the
tag and payload fit the format and returns the number of bytes needed to
encode the element. Both operations preserve the output size on failure.

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

Use `io::write(format, output, element, written)` to encode into a mutable byte view. For successive elements, call `io::writer::init(format, output)` and then `writer.write(element)`. Its `position()` counts committed bytes. Both write operations accept a trailing diagnostic pointer.

To pass the same inputs to a C writer, obtain the native descriptors with
`auto c_format = core::to_c(format)` and `auto c_element = core::to_c(element)`.
Pass their addresses to the C operation. In the other direction, use
`core::from_c` on an element returned by a C reader. Each conversion copies
the descriptor and keeps the same backing byte addresses.

The tag and encoded length views carry native byte counts. A `value_view`
carries a 64-bit logical count. Use `make_value_view` to check that the
pointer and count form a valid descriptor, or `validate` to check an existing
view. The caller supplies the backing storage. Before accessing bytes, use
`size_to_native` to check that a logical count fits in `std::size_t` and
compare it with the available extent. Use `size_add` for checked addition
of two logical counts.

`read_length` decodes a `length_view` into a 64-bit `logical_size` and reports
the number of input bytes consumed. On truncation it reports the available
bytes and preserves the decoded length. `write_length` writes into a
`mutable_byte_view`. Pass an empty, null destination to query the width.
Both operations accept a `fixed_length` configuration and return a scoped
`result`. See the [fixed-field contracts](fixed-tlv.md) for wire limits.

## Reading elements

Include `<opentlv/io/reader/reader.hpp>` and link `OpenTLV::io`. For a
one-shot read, pass the format, the input byte view and two outputs as in
this example.

```cpp
namespace core = opentlv::core;
const std::uint8_t wire[] = {0xa5, 0x01, 0x10, 0xb6, 0x00};
const core::fixed_format format{{1}, {1, core::byte_order::big_endian}};
core::element_view element;
std::size_t consumed{};
const auto status = opentlv::io::read(format, {wire, sizeof wire}, element, consumed);
```

Here `status` is `core::result::ok`, the tag and value point to `wire[0]`
and `wire[2]`, and `consumed` is 3. With a valid format, empty input
returns `core::result::end`. A partial tag, length or value returns
`core::result::truncated`. Both outputs retain their values on end or error.

For a stream, construct an `opentlv::io::reader` and check that
`reader.init(format, input)` succeeds. Initialization copies the format,
borrows the input and sets the byte position to zero. To read a new buffer
with the saved format, call `reader.init(reader.format(), new_input)`.
A failed initialization preserves the previous cursor.

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

Make conversions between C and C++ descriptors explicit. Pass a native C
object to the C function, either as a wrapper member or as a local object
initialized from the corresponding fields. This keeps wrapper layout
independent of the C ABI. Preserve the C result code, diagnostic field,
offset and byte counts in conversions. Add C and C++ entry points for new
wire operations together so both use the same implementation and produce
the same bytes.

## Ownership and lifetimes

- Copying a **tag, encoded length, payload or element view** copies its descriptor.
  Keep the backing bytes alive, at the same address and unchanged through the
  last use of any view.
- A **reader** copies the format and borrows the input. Keep that input alive
  and unchanged while using any reader or view. Copies advance independently.
  Advancing or destroying a reader preserves the views it has returned.
- A **writer** copies the format and borrows a fixed-capacity output buffer.
  Keep the buffer alive and writable through the last write. Make the C++
  cursor non-copyable to prevent accidental writes from a duplicate position.
  An append borrows its source bytes for the call. Keep those bytes disjoint
  from the range being written.
- A **diagnostic** owns its scalar details. The caller can inspect it after
  releasing the input or output buffer.

Foundation operations use caller-provided storage and perform no dynamic
allocation. Destroying a view or cursor never releases its buffer. Supply
borrowed views from storage that survives their use. For example, growing
a vector can invalidate a reader and every element it returned. If a
future type acquires a resource, give it RAII ownership and document its
copy, move and destruction behavior.

## Results and exceptions

Declare checked C++ operations `[[nodiscard]]` and `noexcept`. Return
a typed result corresponding to `tlv_result_t`, including from fallible
initialization. End of input, truncation, invalid arguments,
range errors and short output each keep their C meanings. A caller can
request diagnostic details separately. No exception may cross the C ABI.

Preserve each C operation's output and cursor guarantees. For example,
a failed read or a read at end leaves the element and cursor intact. A
short one-shot write reports the full required capacity and preserves
the destination. A short cursor write also preserves the committed
position. A supplied diagnostic is refreshed on every call, including
success and end. For counts, offset origins and retry behavior, see the
[fixed TLV guide](fixed-tlv.md).

## Checking the contract

Run `python tests/check_public_api.py` from the repository root to compile
each public header on its own. The check compiles every `.h` header in
C99 and C++17, and every `.hpp` header in C++17. Rejection probes check
the C++ umbrella's diagnostic in C++11 and C++14.
The [package checks](architecture.md#verification) build and run C99 and C++17
consumers against relocated static and shared installations.

When adding a wrapper, exercise it alongside its C operation on the same
input and compare wire bytes, results, diagnostic details and output state
on success, end and failure as applicable.
