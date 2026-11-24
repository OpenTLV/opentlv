# C++17 API conventions and module contracts

C++17 is the supported language mode for the C++ API. Its umbrella,
`<opentlv/tlv.hpp>`, currently exposes the public C declarations, including
`tlv_read` and `tlv_write`. The following conventions govern typed C++
interfaces as they are added.

C++17 callers can also use the C interface directly through
`<opentlv/tlv.h>` or individual `.h` headers. Newer C++ modes are
outside the required test matrix.

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
