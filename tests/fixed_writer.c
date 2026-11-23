#include <opentlv/tlv.h>

#include "test_check.h"
#include <stdint.h>
#include <string.h>

static void test_wire_vectors(void)
{
    const uint8_t tag[] = {0xf1, 0x00};
    uint8_t payload[258];
    for (size_t i = 0; i < sizeof payload; ++i) {
        payload[i] = (uint8_t)i;
    }
    const tlv_byte_order_t orders[] = {TLV_BYTE_ORDER_BIG_ENDIAN, TLV_BYTE_ORDER_LITTLE_ENDIAN};
    for (size_t i = 0; i < 2; ++i) {
        for (size_t width = 1; width <= 8; ++width) {
            const tlv_fixed_format_t format = {{sizeof tag}, {width, orders[i]}};
            const size_t count = width == 1 ? 2 : sizeof payload;
            const tlv_element_t element = {{tag, sizeof tag}, {payload, count}};
            const size_t required = sizeof tag + width + count;
            uint8_t expected[1 + sizeof tag + 8 + sizeof payload + 1];
            uint8_t output[sizeof expected];
            memset(expected, 0xcc, sizeof expected);
            memset(output, 0xcc, sizeof output);
            memcpy(expected + 1, tag, sizeof tag);
            uint8_t *length = expected + 1 + sizeof tag;
            memset(length, 0, width);
            /* 258 is 0x0102; the one-octet vector uses just 0x02. */
            length[i == 0 ? width - 1 : 0] = 2;
            if (width > 1) {
                length[i == 0 ? width - 2 : 1] = 1;
            }
            memcpy(length + width, payload, count);
            size_t written = 0;
            CHECK(tlv_write(output + 1, sizeof output - 2, &format, &element, &written) == TLV_OK);
            CHECK(written == required && memcmp(output, expected, sizeof output) == 0);

            tlv_element_t decoded;
            size_t consumed = 0;
            CHECK(tlv_read(output + 1, written, &format, &decoded, &consumed) == TLV_OK);
            CHECK(consumed == written && decoded.tag.size == sizeof tag && decoded.value.size == count);
            CHECK(memcmp(decoded.tag.data, tag, sizeof tag) == 0);
            CHECK(memcmp(decoded.value.data, payload, count) == 0);
        }
    }
}

static void test_capacity_and_retry(void)
{
    const uint8_t tag[] = {0xa0};
    const uint8_t bytes[] = {0x10, 0x20, 0x30};
    const tlv_element_t record = {{tag, 1}, {bytes, sizeof bytes}};
    const tlv_element_t empty = {{tag, 1}, {NULL, 0}};
    tlv_fixed_format_t format = {{1}, {1, TLV_BYTE_ORDER_BIG_ENDIAN}};
    const uint8_t encoded[] = {0xa0, 3, 0x10, 0x20, 0x30};
    const uint8_t clean[8] = {7, 6, 5, 4, 3, 2, 1, 0};
    uint8_t output[sizeof clean];
    size_t written = 99;
    CHECK(tlv_write(NULL, 0, &format, &record, &written) == TLV_ERR_BUFFER_TOO_SHORT);
    CHECK(written == sizeof encoded);
    for (size_t cap = 0; cap <= sizeof encoded; ++cap) {
        memcpy(output, clean, sizeof output);
        written = 99;
        if (cap < sizeof encoded) {
            CHECK(tlv_write(output + 1, cap, &format, &record, &written) == TLV_ERR_BUFFER_TOO_SHORT);
            CHECK(memcmp(output, clean, sizeof output) == 0);
        } else {
            CHECK(tlv_write(output + 1, cap, &format, &record, &written) == TLV_OK);
            CHECK(memcmp(output + 1, encoded, cap) == 0 && output[0] == clean[0]);
            CHECK(memcmp(output + 1 + cap, clean + 1 + cap, sizeof output - 1 - cap) == 0);
        }
        CHECK(written == sizeof encoded);
    }

    /* Start with room for two empty elements and one spare byte. */
    tlv_writer_t writer;
    memcpy(output, clean, sizeof output);
    CHECK(tlv_writer_init(&writer, output + 1, 5, &format) == TLV_OK && writer.pos == 0);
    format.length.size = 2;
    CHECK(tlv_writer_write(&writer, &empty) == TLV_OK && writer.pos == 2);
    const uint8_t prefix[] = {7, 0xa0, 0, 4, 3, 2, 1, 0};
    for (int attempt = 0; attempt < 2; ++attempt) {
        CHECK(tlv_writer_write(&writer, &record) == TLV_ERR_BUFFER_TOO_SHORT && writer.pos == 2);
        CHECK(memcmp(output, prefix, sizeof output) == 0);
    }
    CHECK(tlv_writer_write(&writer, &empty) == TLV_OK && writer.pos == 4);
    const uint8_t final[] = {7, 0xa0, 0, 0xa0, 0, 2, 1, 0};
    CHECK(memcmp(output, final, sizeof output) == 0);
    CHECK(tlv_writer_init(&writer, NULL, 0, &format) == TLV_OK);
    CHECK(tlv_writer_write(&writer, &empty) == TLV_ERR_BUFFER_TOO_SHORT && writer.pos == 0);
}

static void test_append_from_committed_prefix(void)
{
    const uint8_t record[] = {0xd5, 0x00, 0x02, 0x01, 0xef};
    const tlv_element_t original = {{record, 1}, {record + 3, 2}};
    const tlv_fixed_format_t format = {{1}, {2, TLV_BYTE_ORDER_BIG_ENDIAN}};
    uint8_t output[2 * sizeof record];
    tlv_writer_t writer;
    tlv_element_t borrowed;
    size_t consumed;
    CHECK(tlv_writer_init(&writer, output, sizeof output, &format) == TLV_OK);
    CHECK(tlv_writer_write(&writer, &original) == TLV_OK && writer.pos == sizeof record);
    CHECK(tlv_read(output, writer.pos, &format, &borrowed, &consumed) == TLV_OK);
    CHECK(consumed == sizeof record && borrowed.tag.data == output && borrowed.value.data == output + 3);

    CHECK(tlv_writer_write(&writer, &borrowed) == TLV_OK && writer.pos == sizeof output);
    CHECK(tlv_writer_write(&writer, &borrowed) == TLV_ERR_BUFFER_TOO_SHORT && writer.pos == sizeof output);
    CHECK(memcmp(output, record, sizeof record) == 0);
    CHECK(memcmp(output + sizeof record, record, sizeof record) == 0);

    CHECK(tlv_writer_init(&writer, output, sizeof output, &writer.format) == TLV_OK);
    CHECK(writer.data == output && writer.capacity == sizeof output && writer.pos == 0);
    CHECK(writer.format.identifier.size == 1 && writer.format.length.size == 2);
    CHECK(writer.format.length.byte_order == TLV_BYTE_ORDER_BIG_ENDIAN);
    CHECK(memcmp(output, record, sizeof record) == 0);
    CHECK(memcmp(output + sizeof record, record, sizeof record) == 0);
}

static void test_invalid_inputs(void)
{
    const uint8_t tag = 0x42;
    const uint8_t original[] = {1, 2, 3};
    uint8_t output[sizeof original];
    tlv_fixed_format_t format = {{1}, {1, TLV_BYTE_ORDER_LITTLE_ENDIAN}};
    tlv_element_t element = {{&tag, 1}, {NULL, 0}};
    tlv_writer_t writer;
    size_t written = 99;
    memcpy(output, original, sizeof output);
    CHECK(tlv_writer_init(NULL, output, sizeof output, &format) == TLV_ERR_NULL_ARG);
    CHECK(tlv_writer_init(&writer, output, sizeof output, &format) == TLV_OK);
    CHECK(tlv_writer_init(&writer, NULL, 1, &format) == TLV_ERR_NULL_ARG);
    CHECK(tlv_writer_init(&writer, output, sizeof output, NULL) == TLV_ERR_NULL_ARG);
    CHECK(tlv_writer_write(NULL, &element) == TLV_ERR_NULL_ARG);
    CHECK(tlv_writer_write(&writer, NULL) == TLV_ERR_NULL_ARG);
    CHECK(tlv_write(NULL, 1, &format, &element, &written) == TLV_ERR_NULL_ARG);
    CHECK(tlv_write(output, sizeof output, NULL, &element, &written) == TLV_ERR_NULL_ARG);
    CHECK(tlv_write(output, sizeof output, &format, NULL, &written) == TLV_ERR_NULL_ARG);
    CHECK(tlv_write(output, sizeof output, &format, &element, NULL) == TLV_ERR_NULL_ARG);
    element.tag.size = 0;
    CHECK(tlv_write(output, sizeof output, &format, &element, &written) == TLV_ERR_INVALID_TAG_SIZE);
    CHECK(tlv_writer_write(&writer, &element) == TLV_ERR_INVALID_TAG_SIZE);
    element.tag.size = 1;
    element.value.size = 1;
    CHECK(tlv_write(output, sizeof output, &format, &element, &written) == TLV_ERR_NULL_ARG);
    element.value.data = &tag;
    element.value.size = 256;
    CHECK(tlv_write(NULL, 0, &format, &element, &written) == TLV_ERR_INVALID_LENGTH);
    format.length.size = 0;
    CHECK(tlv_write(output, sizeof output, &format, &element, &written) == TLV_ERR_INVALID_ARG);
    CHECK(tlv_writer_init(&writer, NULL, 0, &format) == TLV_ERR_INVALID_ARG);
    format.length.size = 8;
    format.identifier.size = SIZE_MAX;
    element.tag.size = SIZE_MAX;
    element.value.size = 0;
    CHECK(tlv_writer_init(&writer, NULL, 0, &format) == TLV_ERR_OVERFLOW);
    CHECK(tlv_write(NULL, 0, &format, &element, &written) == TLV_ERR_OVERFLOW);
    format.identifier.size = 1;
    element.tag.size = 1;
#if SIZE_MAX <= UINT64_MAX
    element.value.size = (tlv_size_t)SIZE_MAX;
    CHECK(tlv_write(NULL, 0, &format, &element, &written) == TLV_ERR_OVERFLOW);
#endif
#if SIZE_MAX < UINT64_MAX
    element.value.size = (tlv_size_t)SIZE_MAX + 1;
    CHECK(tlv_write(NULL, 0, &format, &element, &written) == TLV_ERR_NATIVE_SIZE);
#endif
    CHECK(written == 99 && memcmp(output, original, sizeof output) == 0);
    CHECK(writer.data == output && writer.capacity == sizeof output && writer.pos == 0);
    CHECK(writer.format.identifier.size == 1 && writer.format.length.size == 1);
    CHECK(writer.format.length.byte_order == TLV_BYTE_ORDER_LITTLE_ENDIAN);
    writer.pos = writer.capacity + 1;
    element.value.size = 0;
    CHECK(tlv_writer_write(&writer, &element) == TLV_ERR_INVALID_ARG);
    CHECK(writer.pos == writer.capacity + 1 && memcmp(output, original, sizeof output) == 0);
}

int main(void)
{
    test_wire_vectors();
    test_capacity_and_retry();
    test_append_from_committed_prefix();
    test_invalid_inputs();
    return 0;
}
