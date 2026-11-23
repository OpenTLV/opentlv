#include <opentlv/tlv.h>

#include <assert.h>
#include <stdint.h>
#include <string.h>

static void test_sizes(void)
{
    size_t native = 42;
    tlv_size_t sum = 42;

    assert(TLV_SIZE_MAX == UINT64_MAX);
    assert(tlv_size_to_native(0, &native) == TLV_OK && native == 0);
    assert(tlv_size_to_native(0, NULL) == TLV_ERR_NULL_ARG);
#if SIZE_MAX < UINT64_MAX
    assert(tlv_size_to_native(SIZE_MAX, &native) == TLV_OK && native == SIZE_MAX);
    assert(tlv_size_to_native((tlv_size_t)SIZE_MAX + 1, &native) == TLV_ERR_NATIVE_SIZE);
    assert(native == SIZE_MAX);
#else
    assert(tlv_size_to_native(TLV_SIZE_MAX, &native) == TLV_OK && native == TLV_SIZE_MAX);
#endif
    assert(tlv_size_add(0, 0, &sum) == TLV_OK && sum == 0);
    assert(tlv_size_add(TLV_SIZE_MAX - 1, 1, &sum) == TLV_OK && sum == TLV_SIZE_MAX);
    assert(tlv_size_add(sum, 1, &sum) == TLV_ERR_OVERFLOW && sum == TLV_SIZE_MAX);
    assert(tlv_size_add(0, 0, NULL) == TLV_ERR_NULL_ARG);
}

static void test_borrowed_value(void)
{
    const uint8_t bytes[] = {0x12, 0x34};
    tlv_value_t value = {NULL, 0};
    tlv_value_t copy;
    tlv_value_t missing = {NULL, 1};

    assert(tlv_value_init(bytes, sizeof bytes, &value) == TLV_OK);
    assert(value.data == bytes && value.size == sizeof bytes);
    assert(tlv_value_validate(&value) == TLV_OK);
    copy = value;
    assert(tlv_value_init(NULL, 1, &value) == TLV_ERR_NULL_ARG);
    assert(value.data == bytes && value.size == sizeof bytes);
    assert(tlv_value_init(NULL, 0, NULL) == TLV_ERR_NULL_ARG);
    assert(tlv_value_validate(NULL) == TLV_ERR_NULL_ARG);
    assert(tlv_value_validate(&missing) == TLV_ERR_NULL_ARG);
    assert(tlv_value_init(NULL, 0, &value) == TLV_OK);
    assert(value.data == NULL && value.size == 0 && tlv_value_validate(&value) == TLV_OK);
    assert(tlv_value_init(bytes + sizeof bytes, 0, &value) == TLV_OK);
    assert(value.data == bytes + sizeof bytes && tlv_value_validate(&value) == TLV_OK);
    assert(copy.data == bytes && copy.size == 2 && copy.data[1] == 0x34);
#if SIZE_MAX < UINT64_MAX
    missing.data = bytes;
    missing.size = (tlv_size_t)SIZE_MAX + 1;
    assert(tlv_value_validate(&missing) == TLV_ERR_NATIVE_SIZE);
    assert(tlv_value_init(bytes, missing.size, &value) == TLV_ERR_NATIVE_SIZE);
    assert(value.data == bytes + sizeof bytes && value.size == 0);
#endif
}

static void check_vector(const tlv_fixed_length_t *length, tlv_size_t number, const uint8_t *wire)
{
    uint8_t expected[10], output[10];
    const tlv_length_t raw = {wire, length->size};
    size_t count = 99;
    tlv_size_t decoded = ~number;

    memset(expected, 0xa5, sizeof expected);
    memset(output, 0xa5, sizeof output);
    memcpy(expected + 1, wire, length->size);
    assert(tlv_fixed_length_write(length, number, NULL, 0, &count) == TLV_OK);
    assert(count == length->size);
    count = 99;
    assert(tlv_fixed_length_write(length, number, output + 1, 8, &count) == TLV_OK);
    assert(count == length->size && memcmp(output, expected, sizeof output) == 0);
    assert(tlv_fixed_length_read(length, raw.data, raw.size, &decoded, &count) == TLV_OK);
    assert(decoded == number && count == raw.size);
    count = 99;
    decoded = ~number;
    assert(tlv_fixed_length_read(length, expected + 1, 9, &decoded, &count) == TLV_OK);
    assert(decoded == number && count == length->size);
}

static void test_length_fields(void)
{
    const uint8_t big[] = {0x81, 0x72, 0x63, 0x54, 0x45, 0x36, 0x27, 0x18};
    const uint8_t little[] = {0x18, 0x27, 0x36, 0x45, 0x54, 0x63, 0x72, 0x81};
    const uint8_t zeros[8] = {0};
    uint8_t ones[8], output[8];
    const tlv_byte_order_t orders[] = {TLV_BYTE_ORDER_BIG_ENDIAN, TLV_BYTE_ORDER_LITTLE_ENDIAN};
    memset(ones, 0xff, sizeof ones);

    for (size_t i = 0; i < 2; ++i) {
        for (size_t width = 1; width <= 8; ++width) {
            const tlv_fixed_length_t length = {width, orders[i]};
            const tlv_size_t maximum = TLV_SIZE_MAX >> (8 * (8 - width));
            const tlv_size_t pattern = UINT64_C(0x8172635445362718) & maximum;
            size_t count = 99;
            check_vector(&length, pattern, i == 0 ? big + 8 - width : little);
            check_vector(&length, 0, zeros);
            check_vector(&length, maximum, ones);

            memset(output, 0xff, sizeof output);
            for (size_t available = 0; available < width; ++available) {
                tlv_size_t decoded = 42;
                count = 99;
                assert(tlv_fixed_length_read(&length, big, available, &decoded, &count) == TLV_ERR_TRUNCATED);
                assert(decoded == 42 && count == available);
                count = 99;
                assert(tlv_fixed_length_write(&length, 0, output, available, &count) == TLV_ERR_BUFFER_TOO_SHORT);
                assert(count == 99 && memcmp(output, ones, sizeof output) == 0);
            }
            if (width < 8) {
                assert(tlv_fixed_length_write(&length, maximum + 1, NULL, 0, &count) == TLV_ERR_INVALID_LENGTH);
                assert(tlv_fixed_length_write(&length, maximum + 1, output, 8, &count) == TLV_ERR_INVALID_LENGTH);
                assert(count == 99 && memcmp(output, ones, sizeof output) == 0);
            }
        }
    }
}

static void test_invalid_length_arguments(void)
{
    const tlv_fixed_length_t good = {1, TLV_BYTE_ORDER_BIG_ENDIAN};
    const tlv_fixed_length_t invalid[] = {
        {0, TLV_BYTE_ORDER_BIG_ENDIAN}, {9, TLV_BYTE_ORDER_LITTLE_ENDIAN}, {1, (tlv_byte_order_t)99}
    };
    uint8_t byte = 0x5a;
    size_t count = 99;
    tlv_size_t decoded = 42;
    for (size_t i = 0; i < sizeof invalid / sizeof invalid[0]; ++i) {
        assert(tlv_fixed_length_read(&invalid[i], &byte, 1, &decoded, &count) == TLV_ERR_INVALID_ARG);
        assert(tlv_fixed_length_write(&invalid[i], 0, &byte, 1, &count) == TLV_ERR_INVALID_ARG);
        assert(tlv_fixed_length_write(&invalid[i], 0, NULL, 0, &count) == TLV_ERR_INVALID_ARG);
    }
    assert(tlv_fixed_length_read(NULL, &byte, 1, &decoded, &count) == TLV_ERR_NULL_ARG);
    assert(tlv_fixed_length_read(&good, NULL, 1, &decoded, &count) == TLV_ERR_NULL_ARG);
    assert(tlv_fixed_length_read(&good, &byte, 1, NULL, &count) == TLV_ERR_NULL_ARG);
    assert(tlv_fixed_length_read(&good, &byte, 1, &decoded, NULL) == TLV_ERR_NULL_ARG);
    assert(tlv_fixed_length_write(NULL, 0, &byte, 1, &count) == TLV_ERR_NULL_ARG);
    assert(tlv_fixed_length_write(&good, 0, NULL, 1, &count) == TLV_ERR_NULL_ARG);
    assert(tlv_fixed_length_write(&good, 0, &byte, 1, NULL) == TLV_ERR_NULL_ARG);
    assert(decoded == 42 && count == 99 && byte == 0x5a);
    assert(tlv_fixed_length_read(&good, NULL, 0, &decoded, &count) == TLV_ERR_TRUNCATED);
    assert(decoded == 42 && count == 0);
}

static void test_header_size(void)
{
    tlv_fixed_format_t format = {{2}, {4, TLV_BYTE_ORDER_LITTLE_ENDIAN}};
    size_t size = 42;

    assert(tlv_fixed_header_size(NULL, &size) == TLV_ERR_NULL_ARG);
    assert(tlv_fixed_header_size(&format, NULL) == TLV_ERR_NULL_ARG);
    assert(size == 42);
    assert(tlv_fixed_header_size(&format, &size) == TLV_OK && size == 6);

    format.identifier.size = 0;
    assert(tlv_fixed_header_size(&format, &size) == TLV_ERR_INVALID_ARG && size == 6);
    format.identifier.size = 2;
    format.length.size = 0;
    assert(tlv_fixed_header_size(&format, &size) == TLV_ERR_INVALID_ARG && size == 6);
    format.length.size = 9;
    assert(tlv_fixed_header_size(&format, &size) == TLV_ERR_INVALID_ARG && size == 6);
    format.length.size = 8;
    format.length.byte_order = TLV_BYTE_ORDER_UNKNOWN;
    assert(tlv_fixed_header_size(&format, &size) == TLV_ERR_INVALID_ARG && size == 6);

    format.length.byte_order = TLV_BYTE_ORDER_BIG_ENDIAN;
    format.identifier.size = SIZE_MAX - 8;
    assert(tlv_fixed_header_size(&format, &size) == TLV_OK && size == SIZE_MAX);
    ++format.identifier.size;
    assert(tlv_fixed_header_size(&format, &size) == TLV_ERR_OVERFLOW && size == SIZE_MAX);
}

static void test_element_size(void)
{
    const uint8_t tag[9] = {0x81};
    const uint8_t value[] = {0x01, 0x02, 0x03};
    tlv_fixed_format_t format = {{sizeof tag}, {1, TLV_BYTE_ORDER_BIG_ENDIAN}};
    tlv_element_t element = {{tag, sizeof tag}, {value, sizeof value}};
    size_t total = 0;

    assert(tlv_fixed_element_size(&format, &element, &total) == TLV_OK && total == 13);
    format.length.byte_order = TLV_BYTE_ORDER_LITTLE_ENDIAN;
    element.value = (tlv_value_t){NULL, 0};
    assert(tlv_fixed_element_size(&format, &element, &total) == TLV_OK && total == 10);
    assert(tlv_fixed_element_size(NULL, &element, &total) == TLV_ERR_NULL_ARG);
    assert(tlv_fixed_element_size(&format, NULL, &total) == TLV_ERR_NULL_ARG);
    assert(tlv_fixed_element_size(&format, &element, NULL) == TLV_ERR_NULL_ARG);
    format.length.size = 9;
    assert(tlv_fixed_element_size(&format, &element, &total) == TLV_ERR_INVALID_ARG);
    format.length.size = 1;
    format.identifier.size = 0;
    assert(tlv_fixed_element_size(&format, &element, &total) == TLV_ERR_INVALID_ARG);
    format.identifier.size = 1;
    assert(tlv_fixed_element_size(&format, &element, &total) == TLV_ERR_INVALID_TAG_SIZE);
    element.tag = (tlv_tag_t){NULL, 1};
    assert(tlv_fixed_element_size(&format, &element, &total) == TLV_ERR_NULL_ARG);
    element.tag = (tlv_tag_t){tag, 1};
    element.value = (tlv_value_t){NULL, 1};
    assert(tlv_fixed_element_size(&format, &element, &total) == TLV_ERR_NULL_ARG);
    element.value = (tlv_value_t){value, 256};
    assert(tlv_fixed_element_size(&format, &element, &total) == TLV_ERR_INVALID_LENGTH);
    assert(total == 10);

    /* Measurement examines the descriptors without reading the claimed byte ranges. */
    format.length.size = 8;
#if SIZE_MAX <= UINT64_MAX
    element.value.size = (tlv_size_t)SIZE_MAX - 9;
    assert(tlv_fixed_element_size(&format, &element, &total) == TLV_OK && total == SIZE_MAX);
    ++element.value.size;
    assert(tlv_fixed_element_size(&format, &element, &total) == TLV_ERR_OVERFLOW && total == SIZE_MAX);
#endif
#if SIZE_MAX < UINT64_MAX
    element.value.size = (tlv_size_t)SIZE_MAX + 1;
    assert(tlv_fixed_element_size(&format, &element, &total) == TLV_ERR_NATIVE_SIZE && total == SIZE_MAX);
#endif
    element.value = (tlv_value_t){NULL, 0};
    element.tag.size = format.identifier.size = SIZE_MAX;
    total = 42;
    assert(tlv_fixed_element_size(&format, &element, &total) == TLV_ERR_OVERFLOW && total == 42);
}

int main(void)
{
    test_sizes();
    test_borrowed_value();
    test_length_fields();
    test_invalid_length_arguments();
    test_header_size();
    test_element_size();
    return 0;
}
