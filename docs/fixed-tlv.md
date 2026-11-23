# Fixed TLV data model

A fixed TLV element consists of a tag, an unsigned length, and exactly that
many value bytes, in that order. The caller selects the field widths and byte
order in `tlv_fixed_format_t`. Every element using that format has the same
header size. The wire carries no width or byte-order indicator.

| Field | Encoding |
| --- | --- |
| Tag | Any positive number of octets, fixed by `identifier.size`. All byte patterns are valid. Leading zeros and byte order are part of the identifier. |
| Length | Exactly `length.size` octets (1–8), big-endian or little-endian as specified by `length.byte_order`. A field of width *w* represents counts from 0 through `2^(8 × w) − 1`. |
| Value | Contiguous, uninterpreted octets. The length counts only these bytes. An empty value has a zero length field and occupies no payload bytes. |

For example, with two tag bytes and a two-byte length, a three-byte value
has the following representations. Only the length field changes with byte
order.

| Byte order | Tag | Length | Value |
| --- | --- | --- | --- |
| Big-endian | `12 34` | `00 03` | `AA BB CC` |
| Little-endian | `12 34` | `03 00` | `AA BB CC` |

## Borrowed bytes and sizes

Core primitives, the reader, and the writer allocate no memory. A format
configuration only needs to remain valid during the call. A value view
borrows its backing bytes for as long as the caller uses the view.

- `tlv_tag_t` and `tlv_length_t` describe raw bytes in wire order. Each
  carries a `const uint8_t *data` and a native `size_t size`. For a raw
  length field, `size` counts the bytes occupied by the field itself.
- `tlv_value_t` carries a read-only pointer and a logical payload size of
  type `tlv_size_t` (`uint64_t`). `tlv_element_t` pairs the tag with this
  value. Copying an element or a view copies its descriptor. The caller
  keeps the backing bytes alive and unchanged during every use.
- `tlv_value_init` accepts a null data pointer for zero bytes and preserves
  the supplied pointer, including for an empty view. `tlv_value_validate`
  checks pointer and native-size requirements without reading the payload.
  The caller supplies at least the declared number of readable bytes.

## Checking boundaries

Use `tlv_fixed_length_read` and `tlv_fixed_length_write` to convert a single
length field. They use the configured byte order regardless of host order or
buffer alignment. Extra bytes after the field are untouched. For a sizing
query, pass a null destination and zero capacity to `tlv_fixed_length_write`.

An eight-byte length of all `FF` decodes to `UINT64_MAX` on both 32-bit and
64-bit platforms. `tlv_size_to_native` checks whether that count fits a
native buffer size. `tlv_size_add` checks addition of two logical counts.
`tlv_fixed_header_size` validates the format and returns the sum of its tag
and length widths, rejecting a sum above `SIZE_MAX`.
`tlv_fixed_element_size` checks the tag and value descriptors, wire length
range, and the complete header-plus-payload size against `SIZE_MAX`. It
measures without accessing the backing bytes.

For the length-field primitives, a truncated read reports the available
prefix size in `*consumed` and preserves the decoded count. All other
failures preserve every output, including destination bytes.

## Reading elements

Use `tlv_read` to decode the first element of a bounded buffer. On success
it returns the borrowed tag and value and sets `*consumed` to the complete
element size. To iterate through adjacent elements, initialize a
`tlv_reader_t` and call `tlv_reader_next` until it returns `TLV_END`.
Both APIs are available through `<opentlv/io/reader/reader.h>` or the
umbrella header.

With one-byte tag and length fields, `10 01 AB 20 00 30 02 CD` contains
two complete elements followed by an incomplete third element.

| Call to `tlv_reader_next` | Result | Next byte offset (`reader.pos`) |
| --- | --- | --- |
| First | `TLV_OK` (tag `10`, value `AB`) | 3 |
| Second | `TLV_OK` (tag `20`, empty value) | 5 |
| Third | `TLV_ERR_TRUNCATED` (tag `30` declares two bytes, only `CD` is available) | 5 |

A buffer ending at offset 5 instead returns `TLV_END` on the third call.
Empty input, including a null pointer with zero size, also returns `TLV_END`
for a valid format. Any nonempty incomplete tag, length field or value
returns `TLV_ERR_TRUNCATED`. The reader compares the 64-bit payload length
with the remaining input before converting it to a native size. Even a
length of `UINT64_MAX` in a short buffer is reported as truncation on both
32-bit and 64-bit hosts.

On end or error, the element, cursor, and one-shot consumed count keep
their previous values. Repeating the call gives the same result. Successful
initialization copies the format settings and resets the cursor to zero.
To restart a reader with its current settings, pass `&reader.format` to
`tlv_reader_init`. Copy an initialized reader to iterate independently
over the same input from the copied position.

Store the reader and the output element in separate, nonoverlapping objects
outside the input buffer. Even partial overlap with a reader member such
as `reader.format` violates the contract for `tlv_reader_next`. The tag and
value pointers in the output refer to the original input. Keep those bytes
alive and unchanged while using the reader or any view. An empty value
points just past its header. A payload that resembles another TLV is
returned as raw bytes.

## Writing elements

Use `tlv_write(data, capacity, format, element, written)` to encode one
element. Include `<opentlv/io/writer/writer.h>` or the umbrella header.
The caller supplies writable storage and a separate `size_t` for the byte
count. On success, `*written` is the complete element size. The writer
copies the tag bytes in wire order, encodes the payload length in the
configured width and byte order, then copies the payload.

| Result | Destination | `*written` |
| --- | --- | --- |
| `TLV_OK` | Contains the element in the first `*written` bytes. | Number of bytes encoded. |
| `TLV_ERR_BUFFER_TOO_SHORT` | Preserves every byte. | Capacity required for the complete element. |
| Any other error | Preserves every byte. | Preserves the previous count. |

For example, a two-byte tag `12 34`, a two-byte little-endian length, and
one payload byte `AB` produce `12 34 01 00 AB`. With four bytes available,
`tlv_write` returns `TLV_ERR_BUFFER_TOO_SHORT` and sets `*written` to 5.
Passing `NULL` and zero capacity gives the same result for this element.
Use `tlv_fixed_element_size` to measure an element before choosing a buffer.

For successive writes, call `tlv_writer_init(writer, data, capacity, format)`.
Initialization copies the format and sets `writer->pos` to zero. A null
destination is valid only with zero capacity. Failed initialization preserves
the previous writer. Call `tlv_writer_write(writer, element)` to append at
`writer->pos`. A successful append advances the cursor by the encoded size.
Any failure preserves the cursor and the entire destination, including
previously encoded elements.

Keep the destination alive and writable while the writer uses it. Keep
the writer state and output count in separate storage. The tag, payload,
and input descriptors must remain readable during the call and must be
disjoint from the bytes being written. A completed prefix of the same
buffer can supply tag or payload bytes for an append into a disjoint suffix.

## Result categories and diagnostics

Pass the result of a Core validator or an IO operation to
`tlv_result_category(result)` to choose how to handle it.
`tlv_result_string(result)` returns a message in immutable static storage.
Callers may retain the pointer for the lifetime of the program. An
unrecognized result maps to `TLV_CATEGORY_UNKNOWN` and the message
`"unknown result"`.

| Category | Result | Meaning |
| --- | --- | --- |
| `TLV_CATEGORY_SUCCESS` | `TLV_OK` | The operation succeeded. |
| `TLV_CATEGORY_END` | `TLV_END` | The reader reached the end of its input. |
| `TLV_CATEGORY_INVALID_ARGUMENT` | `TLV_ERR_NULL_ARG`, `TLV_ERR_INVALID_ARG` | A required pointer is null, the format is invalid, or the cursor position exceeds its bound. |
| `TLV_CATEGORY_MALFORMED_DATA` | `TLV_ERR_INVALID_TAG_SIZE`, `TLV_ERR_INVALID_LENGTH` | The supplied tag width differs from the format, or the supplied count exceeds the selected length width. |
| `TLV_CATEGORY_TRUNCATED` | `TLV_ERR_TRUNCATED` | An input field or payload is incomplete. |
| `TLV_CATEGORY_CAPACITY` | `TLV_ERR_BUFFER_TOO_SHORT` | The destination cannot hold the requested encoding. |
| `TLV_CATEGORY_RANGE` | `TLV_ERR_NATIVE_SIZE`, `TLV_ERR_OVERFLOW` | A logical size exceeds `SIZE_MAX`, or a checked sum exceeds its result type's maximum. |

Every tag and length bit pattern is legal at its configured width, and
the payload is opaque. For example, a supplied length of 256 is malformed
for a one-byte length field. On input, a complete one-byte field of `FF`
declares 255 payload bytes. If fewer follow, the reader reports truncation.

For byte-level detail, pass a `tlv_diagnostic_t *` as the last argument to
`tlv_read_with_diagnostic`, `tlv_reader_next_with_diagnostic`,
`tlv_write_with_diagnostic`, or `tlv_writer_write_with_diagnostic`. Use a
separate object disjoint from the buffers, descriptors, cursor, and other
outputs. Pass `NULL` to omit the record. Existing calls keep their signatures
and numeric result codes. The diagnostic variants follow the same rules for
outputs and cursor movement.

Each call sets `result` in the supplied record. On truncation, `field`
identifies the incomplete part as `TLV_FIELD_TAG`, `TLV_FIELD_LENGTH`,
or `TLV_FIELD_VALUE`. On short output, `TLV_FIELD_ELEMENT` identifies the
entire element. The `size_t offset` gives the start of the affected field or
element in the buffer supplied to the one-shot operation or the original
cursor buffer. The `uintmax_t required` and `available` counts give the full size
needed and the bytes available from that position. All other results,
including success and end, set `field` to `TLV_FIELD_NONE` and the offset
and both counts to zero.

In the reader example above, the third element starts at offset 5.
`tlv_reader_next_with_diagnostic` reports `TLV_FIELD_VALUE` at offset 7
with 2 bytes required and 1 available. A one-shot read starting at the
third element reports offset 2 for the same field. For a writer at offset
4 in a six-byte destination, appending a five-byte element reports
`TLV_FIELD_ELEMENT` at offset 4 with 5 required and 2 available.

## Running the tests

Run `python tests/check_fixed_model.py` to check model and cursor bounds,
round-trip fixed TLV elements, and link a C++11 consumer against the C99
implementation.
Run `python tests/check_fixed_model.py --release` to run the same checks with
optimization enabled and `NDEBUG` defined.
Select GCC or Clang with the `CC` and `CXX` environment variables.
