#include <opentlv/tlv.h>

int main()
{
    const uint8_t tag[] = {0xa5};
    tlv_element_t element{{tag, sizeof tag}, {nullptr, 0}};
    const tlv_fixed_format_t format{{1}, {2, TLV_BYTE_ORDER_BIG_ENDIAN}};
    size_t native = 0;
    return !(tlv_value_init(nullptr, 0, &element.value) == TLV_OK &&
             tlv_fixed_element_size(&format, &element, &native) == TLV_OK &&
             native == 3 &&
             tlv_size_to_native(element.value.size, &native) == TLV_OK &&
             native == 0);
}
