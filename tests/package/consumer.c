#include <opentlv/tlv.h>

#include <stdlib.h>
#include <string.h>

int main(void)
{
    const uint8_t tag = 0xa5;
    const uint8_t payload[] = {0xab, 0xcd};
    const tlv_fixed_format_t format = {{1}, {1, TLV_BYTE_ORDER_BIG_ENDIAN}};
    tlv_element_t element = {{&tag, 1}, {NULL, 0}};
    size_t header, total, count, native;
    tlv_size_t sum, decoded;
    uint8_t wire_length;

    if (tlv_value_init(payload, sizeof payload, &element.value) != TLV_OK ||
        tlv_value_validate(&element.value) != TLV_OK ||
        tlv_fixed_header_size(&format, &header) != TLV_OK ||
        tlv_fixed_element_size(&format, &element, &total) != TLV_OK ||
        tlv_size_add(header, element.value.size, &sum) != TLV_OK ||
        tlv_size_to_native(sum, &native) != TLV_OK || native != total || total != 4)
        return EXIT_FAILURE;

    if (tlv_fixed_length_write(&format.length, element.value.size, &wire_length, 1, &count) != TLV_OK ||
        count != 1 || wire_length != sizeof payload ||
        tlv_fixed_length_read(&format.length, &wire_length, 1, &decoded, &count) != TLV_OK ||
        decoded != sizeof payload || count != 1)
        return EXIT_FAILURE;

    /* Exercise unassigned enum values through the installed C ABI. */
    const tlv_fixed_length_t invalid = {1, (tlv_byte_order_t)99};
    if (tlv_fixed_length_read(&invalid, &wire_length, 1, &decoded, &count) != TLV_ERR_INVALID_ARG ||
        tlv_fixed_length_write(&invalid, 0, &wire_length, 1, &count) != TLV_ERR_INVALID_ARG ||
        tlv_result_category((tlv_result_t)99) != TLV_CATEGORY_UNKNOWN ||
        strcmp(tlv_result_string((tlv_result_t)99), "unknown result") != 0)
        return EXIT_FAILURE;
    return EXIT_SUCCESS;
}
