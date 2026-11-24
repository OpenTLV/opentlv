#include <opentlv/tlv.hpp>

#include <cstdlib>
#include <cstring>

int main()
{
    const uint8_t tag[] = {0xa5};
    tlv_element_t element{{tag, sizeof tag}, {nullptr, 0}};
    const tlv_fixed_format_t format{{1}, {2, TLV_BYTE_ORDER_BIG_ENDIAN}};
    size_t native;
    if (tlv_value_init(nullptr, 0, &element.value) != TLV_OK ||
        tlv_fixed_element_size(&format, &element, &native) != TLV_OK || native != 3 ||
        tlv_size_to_native(element.value.size, &native) != TLV_OK || native != 0)
        return EXIT_FAILURE;

    const uint8_t payload[] = {0xab};
    const uint8_t expected[] = {0xa5, 0x00, 0x01, 0xab};
    uint8_t wire[sizeof expected]{};
    tlv_writer_t writer{};
    if (tlv_value_init(payload, sizeof payload, &element.value) != TLV_OK ||
        tlv_writer_init(&writer, wire, sizeof wire, &format) != TLV_OK ||
        tlv_writer_write(&writer, &element) != TLV_OK ||
        writer.pos != sizeof expected || std::memcmp(wire, expected, sizeof expected) != 0)
        return EXIT_FAILURE;

    tlv_reader_t reader{};
    if (tlv_reader_init(&reader, wire, writer.pos, &format) != TLV_OK ||
        tlv_reader_next(&reader, &element) != TLV_OK ||
        element.tag.data != wire || element.value.size != 1 || element.value.data[0] != payload[0])
        return EXIT_FAILURE;

    tlv_diagnostic_t diagnostic{};
    if (tlv_reader_next_with_diagnostic(&reader, &element, &diagnostic) != TLV_END ||
        diagnostic.result != TLV_END || diagnostic.field != TLV_FIELD_NONE ||
        tlv_result_category(diagnostic.result) != TLV_CATEGORY_END ||
        std::strcmp(tlv_result_string(diagnostic.result), "end of input") != 0)
        return EXIT_FAILURE;
    return EXIT_SUCCESS;
}
