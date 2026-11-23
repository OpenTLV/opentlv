#include <opentlv/tlv.h>

#include "test_check.h"
#include <stdint.h>

static const uint8_t marker[] = {0xaa, 0xbb, 0xcc};
static const tlv_element_t sentinel = {{marker, 1}, {marker + 1, 2}};

static void check_unchanged(tlv_element_t element)
{
    CHECK(element.tag.data == sentinel.tag.data && element.tag.size == sentinel.tag.size);
    CHECK(element.value.data == sentinel.value.data && element.value.size == sentinel.value.size);
}

static void expect_read_failure(const uint8_t *data, size_t size, const tlv_fixed_format_t *format,
                                tlv_result_t expected)
{
    tlv_element_t element = sentinel;
    size_t consumed = 99;
    CHECK(tlv_read(data, size, format, &element, &consumed) == expected);
    check_unchanged(element);
    CHECK(consumed == 99);
}

static void expect_stopped(tlv_reader_t *reader, tlv_result_t expected)
{
    const size_t pos = reader->pos;
    tlv_element_t element = sentinel;
    for (int attempt = 0; attempt < 2; ++attempt) {
        CHECK(tlv_reader_next(reader, &element) == expected);
        check_unchanged(element);
        CHECK(reader->pos == pos);
    }
}

static void test_length_widths(void)
{
    const tlv_byte_order_t orders[] = {TLV_BYTE_ORDER_BIG_ENDIAN, TLV_BYTE_ORDER_LITTLE_ENDIAN};
    for (size_t i = 0; i < 2; ++i) {
        for (size_t width = 1; width <= 8; ++width) {
            const tlv_fixed_format_t format = {{2}, {width, orders[i]}};
            const size_t payload = 2 + width;
            const size_t record_size = payload + 2;
            uint8_t wire[13] = {0xfe, 0x00};
            wire[i == 0 ? payload - 1 : 2] = 2;
            wire[payload] = 0x12;
            wire[payload + 1] = 0x34;
            wire[record_size] = 0xaa;

            tlv_element_t element = sentinel;
            size_t consumed = 0;
            CHECK(tlv_read(wire, record_size + 1, &format, &element, &consumed) == TLV_OK);
            CHECK(consumed == record_size);
            CHECK(element.tag.data == wire && element.tag.size == 2);
            CHECK(element.value.data == wire + payload && element.value.size == 2);
            for (size_t cut = 0; cut < record_size; ++cut) {
                tlv_reader_t reader;
                const tlv_result_t error = cut == 0 ? TLV_END : TLV_ERR_TRUNCATED;
                expect_read_failure(wire, cut, &format, error);
                CHECK(tlv_reader_init(&reader, wire, cut, &format) == TLV_OK);
                expect_stopped(&reader, error);
            }
        }
    }
}

static void test_stream(void)
{
    /* The first value contains a complete TLV record as opaque payload. */
    const uint8_t wire[] = {0xa1, 3, 0x78, 1, 0xee, 0xa2, 0, 0xa3, 2, 0xde, 0xad};
    tlv_fixed_format_t format = {{1}, {1, TLV_BYTE_ORDER_BIG_ENDIAN}};
    tlv_reader_t reader;
    tlv_element_t element;
    CHECK(tlv_reader_init(&reader, wire, sizeof wire, &format) == TLV_OK);
    CHECK(reader.data == wire && reader.size == sizeof wire && reader.pos == 0);
    format.identifier.size = 2;
    format.length.size = 8;
    CHECK(tlv_reader_next(&reader, &element) == TLV_OK && reader.pos == 5);
    CHECK(element.tag.data == wire && element.value.data == wire + 2 && element.value.size == 3);
    const tlv_element_t first = element;
    CHECK(tlv_reader_next(&reader, &element) == TLV_OK && reader.pos == 7);
    CHECK(element.tag.data == wire + 5 && element.value.size == 0 && element.value.data == wire + 7);
    CHECK(tlv_reader_next(&reader, &element) == TLV_OK && reader.pos == sizeof wire);
    CHECK(element.tag.data == wire + 7 && element.value.data == wire + 9 && element.value.size == 2);
    expect_stopped(&reader, TLV_END);

    CHECK(tlv_reader_init(&reader, wire, sizeof wire - 1, &reader.format) == TLV_OK && reader.pos == 0);
    CHECK(tlv_reader_next(&reader, &element) == TLV_OK && reader.pos == 5);
    tlv_reader_t copy = reader;
    CHECK(tlv_reader_next(&reader, &element) == TLV_OK && reader.pos == 7);
    CHECK(copy.pos == 5);
    CHECK(tlv_reader_next(&copy, &element) == TLV_OK && copy.pos == 7);
    CHECK(element.tag.data == wire + 5 && element.value.data == wire + 7 && element.value.size == 0);
    expect_stopped(&reader, TLV_ERR_TRUNCATED);
    CHECK(tlv_reader_init(&reader, NULL, 0, &reader.format) == TLV_OK);
    expect_stopped(&reader, TLV_END);
    CHECK(first.tag.data == wire && first.value.data == wire + 2 && first.value.data[2] == 0xee);

    /* For a terminal empty value the borrowed pointer is exactly one past the buffer. */
    size_t consumed;
    CHECK(tlv_read(wire + 5, 2, &reader.format, &element, &consumed) == TLV_OK && consumed == 2);
    CHECK(element.value.data == wire + 7 && element.value.size == 0);
}

static void test_large_lengths(void)
{
    const tlv_fixed_format_t format = {{1}, {8, TLV_BYTE_ORDER_BIG_ENDIAN}};
    const uint8_t max[] = {0x42, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    /* A premature 32-bit conversion would turn 2^32 into an empty value. */
    const uint8_t wide[] = {0x42, 0, 0, 0, 1, 0, 0, 0, 0};
    tlv_reader_t reader;
    expect_read_failure(max, sizeof max, &format, TLV_ERR_TRUNCATED);
    expect_read_failure(wide, sizeof wide, &format, TLV_ERR_TRUNCATED);
    CHECK(tlv_reader_init(&reader, wide, sizeof wide, &format) == TLV_OK);
    expect_stopped(&reader, TLV_ERR_TRUNCATED);
}

static void test_invalid_arguments(void)
{
    const uint8_t wire[] = {0x42, 0};
    const tlv_fixed_format_t good = {{1}, {1, TLV_BYTE_ORDER_BIG_ENDIAN}};
    const tlv_fixed_format_t invalid[] = {
        {{0}, {1, TLV_BYTE_ORDER_BIG_ENDIAN}}, {{1}, {0, TLV_BYTE_ORDER_BIG_ENDIAN}},
        {{1}, {9, TLV_BYTE_ORDER_BIG_ENDIAN}}, {{1}, {1, (tlv_byte_order_t)99}},
        {{SIZE_MAX}, {1, TLV_BYTE_ORDER_BIG_ENDIAN}},
    };
    tlv_reader_t reader;
    tlv_element_t element = sentinel;
    size_t consumed = 99;
    expect_read_failure(NULL, 0, &good, TLV_END);
    expect_read_failure(NULL, 1, &good, TLV_ERR_NULL_ARG);
    expect_read_failure(NULL, 0, NULL, TLV_ERR_NULL_ARG);
    CHECK(tlv_read(wire, sizeof wire, &good, NULL, &consumed) == TLV_ERR_NULL_ARG && consumed == 99);
    CHECK(tlv_read(wire, sizeof wire, &good, &element, NULL) == TLV_ERR_NULL_ARG);
    check_unchanged(element);
    CHECK(tlv_reader_init(NULL, wire, sizeof wire, &good) == TLV_ERR_NULL_ARG);
    CHECK(tlv_reader_next(NULL, &element) == TLV_ERR_NULL_ARG);
    check_unchanged(element);

    CHECK(tlv_reader_init(&reader, wire, sizeof wire, &good) == TLV_OK);
    CHECK(tlv_reader_next(&reader, NULL) == TLV_ERR_NULL_ARG && reader.pos == 0);
    CHECK(tlv_reader_next(&reader, &element) == TLV_OK && reader.pos == sizeof wire);
    CHECK(tlv_reader_init(&reader, NULL, 1, &good) == TLV_ERR_NULL_ARG);
    CHECK(tlv_reader_init(&reader, NULL, 0, NULL) == TLV_ERR_NULL_ARG);
    for (size_t i = 0; i < sizeof invalid / sizeof invalid[0]; ++i) {
        const tlv_result_t error = invalid[i].identifier.size == SIZE_MAX ? TLV_ERR_OVERFLOW : TLV_ERR_INVALID_ARG;
        expect_read_failure(NULL, 0, &invalid[i], error);
        CHECK(tlv_reader_init(&reader, NULL, 0, &invalid[i]) == error);
    }
    CHECK(reader.data == wire && reader.size == sizeof wire && reader.pos == sizeof wire);
    CHECK(reader.format.identifier.size == 1 && reader.format.length.size == 1);
    CHECK(reader.format.length.byte_order == TLV_BYTE_ORDER_BIG_ENDIAN);
    expect_stopped(&reader, TLV_END);
    reader.pos = reader.size + 1;
    expect_stopped(&reader, TLV_ERR_INVALID_ARG);
}

int main(void)
{
    test_length_widths();
    test_stream();
    test_large_lengths();
    test_invalid_arguments();
    return 0;
}
