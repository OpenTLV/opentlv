#include <opentlv/tlv.h>
#include "check.h"

#include <string.h>

enum { max_input = 1024 };

static void check_view(tlv_element_t actual, tlv_element_t expected)
{
    CHECK(actual.tag.data == expected.tag.data && actual.tag.size == expected.tag.size);
    CHECK(actual.value.data == expected.value.data && actual.value.size == expected.value.size);
}

/* Decode the literal wire length independently and compare it before narrowing. */
static tlv_result_t expected_record(const uint8_t *wire, size_t size, tlv_fixed_format_t format,
                                    tlv_element_t *view, size_t *count)
{
    const size_t tag = format.identifier.size;
    const size_t width = format.length.size;
    const size_t header = tag + width;
    if (!size) return TLV_END;
    if (size < header) return TLV_ERR_TRUNCATED;

    uint64_t length = 0;
    for (size_t i = 0; i < width; ++i) {
        const size_t offset = format.length.byte_order == TLV_BYTE_ORDER_BIG_ENDIAN ? i : width - 1 - i;
        length = (length << 8) | wire[tag + offset];
    }
    if (length > size - header) return TLV_ERR_TRUNCATED;
    *view = (tlv_element_t){{wire, tag}, {wire + header, length}};
    *count = header + (size_t)length;
    return TLV_OK;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 3 || size > max_input) return 0;
    const tlv_byte_order_t orders[] = {TLV_BYTE_ORDER_BIG_ENDIAN, TLV_BYTE_ORDER_LITTLE_ENDIAN,
                                       TLV_BYTE_ORDER_UNKNOWN};
    const tlv_fixed_format_t format = {{data[0]}, {data[1], orders[data[2] % 3]}};
    const uint8_t *wire = data + 3;
    const size_t available = size - 3;
    const tlv_element_t sentinel = {{data, 1}, {data + 1, 1}};
    const tlv_fixed_format_t initial = {{1}, {1, TLV_BYTE_ORDER_BIG_ENDIAN}};
    tlv_reader_t reader = {data, size, 1, initial};
    tlv_result_t initialized = tlv_reader_init(&reader, wire, available, &format);
    if (!data[0] || !data[1] || data[1] > 8 || format.length.byte_order == TLV_BYTE_ORDER_UNKNOWN) {
        CHECK(initialized == TLV_ERR_INVALID_ARG);
        CHECK(reader.data == data && reader.size == size && reader.pos == 1);
        CHECK(reader.format.identifier.size == 1 && reader.format.length.size == 1);
        CHECK(reader.format.length.byte_order == initial.length.byte_order);
        tlv_element_t view = sentinel;
        size_t consumed = SIZE_MAX;
        CHECK(tlv_read(wire, available, &format, &view, &consumed) == initialized);
        check_view(view, sentinel);
        CHECK(consumed == SIZE_MAX);
        return 0;
    }

    CHECK(initialized == TLV_OK && reader.pos == 0);
    uint8_t encoded[max_input];
    for (;;) {
        const size_t pos = reader.pos;
        CHECK(pos <= available);
        const uint8_t *start = wire + pos;
        const size_t remaining = available - pos;
        tlv_element_t expected = sentinel, direct = sentinel, next = sentinel;
        size_t count = SIZE_MAX, consumed = SIZE_MAX;
        const tlv_result_t result = expected_record(start, remaining, format, &expected, &count);
        CHECK(tlv_read(start, remaining, &format, &direct, &consumed) == result);
        CHECK(tlv_reader_next(&reader, &next) == result);
        CHECK(reader.data == wire && reader.size == available);
        CHECK(reader.format.identifier.size == format.identifier.size);
        CHECK(reader.format.length.size == format.length.size);
        CHECK(reader.format.length.byte_order == format.length.byte_order);
        check_view(direct, expected);
        check_view(next, expected);
        CHECK(consumed == count);
        if (result != TLV_OK) {
            CHECK(reader.pos == pos);
            break;
        }
        CHECK(count > 0 && count <= remaining && reader.pos == pos + count);
        size_t written = SIZE_MAX;
        CHECK(tlv_write(encoded, count, &format, &next, &written) == TLV_OK);
        CHECK(written == count && memcmp(encoded, start, count) == 0);
    }
    return 0;
}
