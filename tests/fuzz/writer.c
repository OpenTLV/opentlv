#include <opentlv/tlv.h>
#include "../test_check.h"

#include <string.h>

enum { max_input = 1024, max_output = max_input + 32 };

static void check_rejected_format(tlv_writer_t *writer, const tlv_element_t *element,
                                  unsigned selector, const uint8_t *expected)
{
    const tlv_writer_t before = *writer;
    uint8_t *record = before.data + before.pos;
    const size_t remaining = before.capacity - before.pos;
    tlv_fixed_format_t invalid = before.format;
    switch (selector) {
    case 0: invalid.identifier.size = 0; break;
    case 1: invalid.length.size = 0; break;
    case 2: invalid.length.size = 9; break;
    case 3: invalid.length.byte_order = TLV_BYTE_ORDER_UNKNOWN; break;
    }

    CHECK(tlv_writer_init(writer, record, remaining, &invalid) == TLV_ERR_INVALID_ARG);
    CHECK(writer->data == before.data && writer->capacity == before.capacity && writer->pos == before.pos);
    CHECK(writer->format.identifier.size == before.format.identifier.size);
    CHECK(writer->format.length.size == before.format.length.size);
    CHECK(writer->format.length.byte_order == before.format.length.byte_order);
    CHECK(memcmp(before.data, expected, max_output) == 0);

    size_t written = SIZE_MAX;
    CHECK(tlv_write(record, remaining, &invalid, element, &written) == TLV_ERR_INVALID_ARG);
    CHECK(written == SIZE_MAX && memcmp(before.data, expected, max_output) == 0);
}

/* The first record is empty. Its committed bytes must survive the next append. */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 4 || size > max_input) return 0;
    const size_t tag = 1 + data[0] % 8;
    const size_t width = 1 + data[1] % 8;
    if (size - 4 < tag) return 0;
    const tlv_byte_order_t order = (data[2] & 1) ? TLV_BYTE_ORDER_LITTLE_ENDIAN : TLV_BYTE_ORDER_BIG_ENDIAN;
    const tlv_fixed_format_t format = {{tag}, {width, order}};
    const size_t header = tag + width;
    const size_t length = size - 4 - tag;
    const size_t required = header + length;
    const tlv_element_t source = {{data + 4, tag}, {data + 4 + tag, length}};
    const tlv_element_t empty = {source.tag, {NULL, 0}};
    size_t capacity;
    if (data[3] == 0) capacity = required;
    else if (data[3] == 1) capacity = required - 1;
    else capacity = (size_t)(data[3] - 2) * (max_output - header) / (UINT8_MAX - 2);

    uint8_t stream[max_output], direct[max_output], expected[max_output];
    memset(stream, 0xa5, sizeof stream);
    memset(direct, 0xa5, sizeof direct);
    memset(expected, 0xa5, sizeof expected);
    CHECK(header + capacity <= sizeof stream);
    tlv_writer_t writer;
    CHECK(tlv_writer_init(&writer, stream, header + capacity, &format) == TLV_OK);
    CHECK(tlv_writer_write(&writer, &empty) == TLV_OK);
    memcpy(expected, source.tag.data, tag);
    memset(expected + tag, 0, width);
    CHECK(writer.data == stream && writer.capacity == header + capacity);
    CHECK(writer.pos == header && memcmp(stream, expected, sizeof stream) == 0);
    check_rejected_format(&writer, &source, (data[2] >> 1) & 3, expected);

    tlv_result_t result = TLV_OK;
    size_t expected_written = required;
    if (width == 1 && length > UINT8_MAX) {
        result = TLV_ERR_INVALID_LENGTH;
        expected_written = SIZE_MAX;
    } else if (capacity < required) {
        result = TLV_ERR_BUFFER_TOO_SHORT;
    } else {
        uint8_t *record = expected + header;
        memcpy(record, source.tag.data, tag);
        for (size_t i = 0; i < width; ++i) {
            size_t offset = order == TLV_BYTE_ORDER_LITTLE_ENDIAN ? i : width - 1 - i;
            record[tag + offset] = (uint8_t)((uint64_t)length >> (8 * i));
        }
        memcpy(record + header, source.value.data, length);
    }

    size_t written = SIZE_MAX;
    memcpy(direct, expected, header);
    uint8_t *record = direct + header;
    CHECK(tlv_write(record, capacity, &format, &source, &written) == result);
    CHECK(written == expected_written);
    CHECK(tlv_writer_write(&writer, &source) == result);
    CHECK(writer.data == stream && writer.capacity == header + capacity);
    CHECK(writer.format.identifier.size == tag && writer.format.length.size == width);
    CHECK(writer.format.length.byte_order == order);
    CHECK(writer.pos == header + (result == TLV_OK ? required : 0));
    CHECK(memcmp(stream, expected, sizeof stream) == 0);
    CHECK(memcmp(direct, expected, sizeof direct) == 0);

    if (result == TLV_OK) {
        tlv_element_t view;
        size_t consumed = SIZE_MAX;
        CHECK(tlv_read(record, written, &format, &view, &consumed) == TLV_OK);
        CHECK(consumed == written && view.tag.size == tag && view.value.size == length);
        CHECK(view.tag.data == record && view.value.data == record + header);
        CHECK(memcmp(view.tag.data, source.tag.data, tag) == 0);
        CHECK(memcmp(view.value.data, source.value.data, length) == 0);
    }
    return 0;
}
