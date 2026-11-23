#include <opentlv/tlv.h>

#include <cstring>

#include "test_check.h"

int main()
{
    const uint8_t tag[] = {0xa5};
    tlv_element_t element{{tag, sizeof tag}, {nullptr, 0}};
    const tlv_fixed_format_t format{{1}, {2, TLV_BYTE_ORDER_BIG_ENDIAN}};
    size_t native = 0;
    CHECK(tlv_value_init(nullptr, 0, &element.value) == TLV_OK);
    CHECK(tlv_fixed_element_size(&format, &element, &native) == TLV_OK && native == 3);
    CHECK(tlv_size_to_native(element.value.size, &native) == TLV_OK && native == 0);

    const uint8_t payload[] = {0xab};
    const uint8_t expected[] = {0xa5, 0x00, 0x01, 0xab};
    uint8_t wire[sizeof expected]{};
    tlv_writer_t writer{};
    CHECK(tlv_value_init(payload, sizeof payload, &element.value) == TLV_OK);
    CHECK(tlv_writer_init(&writer, wire, sizeof wire, &format) == TLV_OK);
    CHECK(tlv_writer_write(&writer, &element) == TLV_OK);
    CHECK(writer.pos == sizeof expected && std::memcmp(wire, expected, sizeof expected) == 0);

    tlv_reader_t reader{};
    CHECK(tlv_reader_init(&reader, wire, writer.pos, &format) == TLV_OK);
    CHECK(tlv_reader_next(&reader, &element) == TLV_OK);
    CHECK(element.tag.data == wire && element.value.size == 1 && element.value.data[0] == payload[0]);
    CHECK(tlv_reader_next(&reader, &element) == TLV_END);
    return 0;
}
