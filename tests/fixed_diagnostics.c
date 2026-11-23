#include <opentlv/tlv.h>

#include "test_check.h"
#include <stdint.h>
#include <string.h>

static const tlv_diagnostic_t dirty = {
    .result = TLV_ERR_TRUNCATED, .field = TLV_FIELD_VALUE, .offset = 91, .required = 82, .available = 73
};

static void check_detail(tlv_diagnostic_t d, tlv_result_t result, tlv_field_t field,
                         size_t offset, uintmax_t required, uintmax_t available)
{
    CHECK(d.result == result && d.field == field && d.offset == offset);
    CHECK(d.required == required && d.available == available);
}

static void check_result_only(tlv_diagnostic_t d, tlv_result_t result)
{
    check_detail(d, result, TLV_FIELD_NONE, 0, 0, 0);
}

static void check_view(tlv_element_t got, tlv_element_t expected)
{
    CHECK(got.tag.data == expected.tag.data && got.tag.size == expected.tag.size);
    CHECK(got.value.data == expected.value.data && got.value.size == expected.value.size);
}

static void test_result_categories(void)
{
    const struct {
        tlv_result_t result;
        tlv_result_category_t category;
    } cases[] = {
        {TLV_OK, TLV_CATEGORY_SUCCESS}, {TLV_END, TLV_CATEGORY_END},
        {TLV_ERR_NULL_ARG, TLV_CATEGORY_INVALID_ARGUMENT}, {TLV_ERR_INVALID_ARG, TLV_CATEGORY_INVALID_ARGUMENT},
        {TLV_ERR_INVALID_TAG_SIZE, TLV_CATEGORY_MALFORMED_DATA}, {TLV_ERR_INVALID_LENGTH, TLV_CATEGORY_MALFORMED_DATA},
        {TLV_ERR_TRUNCATED, TLV_CATEGORY_TRUNCATED}, {TLV_ERR_BUFFER_TOO_SHORT, TLV_CATEGORY_CAPACITY},
        {TLV_ERR_OVERFLOW, TLV_CATEGORY_RANGE}, {TLV_ERR_NATIVE_SIZE, TLV_CATEGORY_RANGE}
    };
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; ++i) {
        CHECK(tlv_result_category(cases[i].result) == cases[i].category);
        const char *message = tlv_result_string(cases[i].result);
        CHECK(message != NULL && *message != '\0' && strcmp(message, "unknown result") != 0);
    }
    CHECK(tlv_result_category((tlv_result_t)2) == TLV_CATEGORY_UNKNOWN);
    CHECK(strcmp(tlv_result_string((tlv_result_t)2), "unknown result") == 0);
    CHECK(tlv_result_category((tlv_result_t)99) == TLV_CATEGORY_UNKNOWN);
    CHECK(strcmp(tlv_result_string((tlv_result_t)99), "unknown result") == 0);
}

static void test_input_fields(void)
{
    const tlv_fixed_format_t format = {{2}, {2, TLV_BYTE_ORDER_BIG_ENDIAN}};
    const uint8_t wire[] = {0xaa, 0xbb, 0, 0, 0xff, 0x00, 0, 3, 0x12, 0x34, 0x56};
    const size_t prefix = 4;
    const tlv_element_t first = {{wire, 2}, {wire + prefix, 0}};
    const struct {
        size_t cut;
        tlv_field_t field;
        size_t offset, required, available;
    } cases[] = {
        {1, TLV_FIELD_TAG, 0, 2, 1},
        {2, TLV_FIELD_LENGTH, 2, 2, 0}, {3, TLV_FIELD_LENGTH, 2, 2, 1},
        {4, TLV_FIELD_VALUE, 4, 3, 0}, {5, TLV_FIELD_VALUE, 4, 3, 1}, {6, TLV_FIELD_VALUE, 4, 3, 2}
    };
    tlv_diagnostic_t d = dirty;
    tlv_element_t element = first;
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; ++i) {
        size_t consumed = 99;
        CHECK(tlv_read_with_diagnostic(wire + prefix, cases[i].cut, &format, &element, &consumed, &d)
              == TLV_ERR_TRUNCATED);
        check_detail(d, TLV_ERR_TRUNCATED, cases[i].field, cases[i].offset, cases[i].required, cases[i].available);
        CHECK(consumed == 99);
        check_view(element, first);

        tlv_reader_t reader;
        CHECK(tlv_reader_init(&reader, wire, prefix + cases[i].cut, &format) == TLV_OK);
        CHECK(tlv_reader_next_with_diagnostic(&reader, &element, &d) == TLV_OK);
        check_result_only(d, TLV_OK);
        CHECK(tlv_reader_next_with_diagnostic(&reader, &element, &d) == TLV_ERR_TRUNCATED);
        check_detail(d, TLV_ERR_TRUNCATED, cases[i].field, prefix + cases[i].offset,
                     cases[i].required, cases[i].available);
        CHECK(tlv_reader_next_with_diagnostic(&reader, &element, NULL) == TLV_ERR_TRUNCATED);
        CHECK(reader.pos == prefix);
        check_view(element, first);
    }
    size_t consumed = 99;
    CHECK(tlv_read_with_diagnostic(NULL, 0, &format, &element, &consumed, &d) == TLV_END);
    check_result_only(d, TLV_END);
    CHECK(tlv_read_with_diagnostic(NULL, 1, &format, &element, &consumed, &d) == TLV_ERR_NULL_ARG);
    check_result_only(d, TLV_ERR_NULL_ARG);
    CHECK(consumed == 99);
    check_view(element, first);

    tlv_reader_t ended;
    CHECK(tlv_reader_init(&ended, wire, prefix, &format) == TLV_OK);
    CHECK(tlv_reader_next(&ended, &element) == TLV_OK);
    d = dirty;
    CHECK(tlv_reader_next_with_diagnostic(&ended, &element, &d) == TLV_END);
    check_result_only(d, TLV_END);
    CHECK(ended.pos == prefix);
    check_view(element, first);
    ended.pos = ended.size + 1;
    d = dirty;
    CHECK(tlv_reader_next_with_diagnostic(&ended, &element, &d) == TLV_ERR_INVALID_ARG);
    check_result_only(d, TLV_ERR_INVALID_ARG);
    CHECK(ended.pos == ended.size + 1);
    check_view(element, first);

    const tlv_fixed_format_t wide = {{1}, {8, TLV_BYTE_ORDER_BIG_ENDIAN}};
    const uint8_t max[] = {0xaa, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe};
    CHECK(tlv_read_with_diagnostic(max, sizeof max, &wide, &element, &consumed, &d) == TLV_ERR_TRUNCATED);
    check_detail(d, TLV_ERR_TRUNCATED, TLV_FIELD_VALUE, 9, TLV_SIZE_MAX, 1);
    CHECK(tlv_read_with_diagnostic(max, sizeof max, &wide, &element, &consumed, NULL) == TLV_ERR_TRUNCATED);
    CHECK(consumed == 99);
    check_view(element, first);
}

static void test_fixed_wire_domain(void)
{
    const tlv_fixed_format_t format = {{1}, {1, TLV_BYTE_ORDER_BIG_ENDIAN}};
    uint8_t wire[2 + 255];
    memset(wire, 0xff, sizeof wire);
    tlv_diagnostic_t d = dirty;
    for (size_t n = 0; n <= 255; ++n) {
        wire[0] = (uint8_t)n;
        wire[1] = (uint8_t)n;
        size_t consumed;
        tlv_element_t element;
        CHECK(tlv_read_with_diagnostic(wire, 2 + n, &format, &element, &consumed, &d) == TLV_OK);
        CHECK(consumed == 2 + n && element.tag.data == wire && element.value.size == n);
        check_result_only(d, TLV_OK);
    }
}

static void test_output_capacity(void)
{
    const tlv_fixed_format_t format = {{1}, {1, TLV_BYTE_ORDER_BIG_ENDIAN}};
    const uint8_t tag = 0xa1, payload[] = {0, 0xff};
    const tlv_element_t empty = {{&tag, 1}, {NULL, 0}};
    const tlv_element_t record = {{&tag, 1}, {payload, 2}};
    uint8_t output[5] = {1, 2, 3, 4, 5};
    const uint8_t prefix[] = {0xa1, 0, 3, 4, 5};
    tlv_writer_t writer;
    tlv_diagnostic_t d = dirty;
    size_t written = 99;
    CHECK(tlv_write_with_diagnostic(NULL, 0, &format, &record, &written, &d) == TLV_ERR_BUFFER_TOO_SHORT);
    check_detail(d, TLV_ERR_BUFFER_TOO_SHORT, TLV_FIELD_ELEMENT, 0, 4, 0);
    CHECK(written == 4);
    CHECK(tlv_writer_init(&writer, output, sizeof output, &format) == TLV_OK);
    CHECK(tlv_writer_write_with_diagnostic(&writer, &empty, &d) == TLV_OK);
    check_result_only(d, TLV_OK);
    CHECK(tlv_writer_write_with_diagnostic(&writer, &record, &d) == TLV_ERR_BUFFER_TOO_SHORT);
    check_detail(d, TLV_ERR_BUFFER_TOO_SHORT, TLV_FIELD_ELEMENT, 2, 4, 3);
    CHECK(tlv_writer_write_with_diagnostic(&writer, &record, NULL) == TLV_ERR_BUFFER_TOO_SHORT);
    CHECK(writer.pos == 2 && memcmp(output, prefix, sizeof output) == 0);
    CHECK(tlv_writer_write_with_diagnostic(&writer, NULL, &d) == TLV_ERR_NULL_ARG);
    check_result_only(d, TLV_ERR_NULL_ARG);
    CHECK(writer.pos == 2 && memcmp(output, prefix, sizeof output) == 0);
    writer.pos = writer.capacity + 1;
    d = dirty;
    CHECK(tlv_writer_write_with_diagnostic(&writer, &record, &d) == TLV_ERR_INVALID_ARG);
    check_result_only(d, TLV_ERR_INVALID_ARG);
    CHECK(writer.pos == writer.capacity + 1 && memcmp(output, prefix, sizeof output) == 0);
    written = 99;
    CHECK(tlv_write_with_diagnostic(output + 2, 3, &format, &record, &written, &d) == TLV_ERR_BUFFER_TOO_SHORT);
    check_detail(d, TLV_ERR_BUFFER_TOO_SHORT, TLV_FIELD_ELEMENT, 0, 4, 3);
    CHECK(written == 4);
    written = 99;
    CHECK(tlv_write_with_diagnostic(output + 2, 3, &format, &record, &written, NULL) == TLV_ERR_BUFFER_TOO_SHORT);
    CHECK(written == 4 && memcmp(output, prefix, sizeof output) == 0);
    CHECK(tlv_write_with_diagnostic(output, sizeof output, &format, &record, &written, &d) == TLV_OK);
    check_result_only(d, TLV_OK);
    CHECK(written == 4);
}

static void test_invalid_descriptors(void)
{
    const uint8_t tag = 0x42;
    uint8_t payload[256] = {0};
    tlv_fixed_format_t format = {{1}, {1, TLV_BYTE_ORDER_BIG_ENDIAN}};
    tlv_element_t element = {{&tag, 0}, {payload, 0}};
    tlv_diagnostic_t d = dirty;
    size_t written = 99;
    CHECK(tlv_write_with_diagnostic(NULL, 0, &format, &element, &written, &d) == TLV_ERR_INVALID_TAG_SIZE);
    check_result_only(d, TLV_ERR_INVALID_TAG_SIZE);
    element.tag.size = 1;
    element.value.size = sizeof payload;
    d = dirty;
    CHECK(tlv_write_with_diagnostic(NULL, 0, &format, &element, &written, &d) == TLV_ERR_INVALID_LENGTH);
    check_result_only(d, TLV_ERR_INVALID_LENGTH);
    format.length.size = 0;
    d = dirty;
    CHECK(tlv_write_with_diagnostic(NULL, 0, &format, &element, &written, &d) == TLV_ERR_INVALID_ARG);
    check_result_only(d, TLV_ERR_INVALID_ARG);
    format.length.size = 8;
#if SIZE_MAX <= UINT64_MAX
    /* The payload count fits size_t; including the nine header bytes overflows. */
    element.value.size = (tlv_size_t)SIZE_MAX;
    d = dirty;
    CHECK(tlv_write_with_diagnostic(NULL, 0, &format, &element, &written, &d) == TLV_ERR_OVERFLOW);
    check_result_only(d, TLV_ERR_OVERFLOW);
#endif
#if SIZE_MAX < UINT64_MAX
    element.value.size = TLV_SIZE_MAX;
    d = dirty;
    CHECK(tlv_write_with_diagnostic(NULL, 0, &format, &element, &written, &d) == TLV_ERR_NATIVE_SIZE);
    check_result_only(d, TLV_ERR_NATIVE_SIZE);
#endif
    CHECK(written == 99);
}

int main(void)
{
    test_result_categories();
    test_input_fields();
    test_fixed_wire_domain();
    test_output_capacity();
    test_invalid_descriptors();
    return 0;
}
