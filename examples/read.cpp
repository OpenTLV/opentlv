// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#include <opentlv/tlv.h>

#include <array>
#include <cstdio>
#include <cstdlib>

static void print_bytes(const uint8_t *data, tlv_size_t size)
{
    for (tlv_size_t i = 0; i < size; ++i) {
        std::printf(" %02X", static_cast<unsigned>(data[i]));
    }
}

int main()
{
    const std::array<uint8_t, 7> wire{{0x12, 0x34, 0x00, 0x03, 0xAA, 0xBB, 0xCC}};
    const tlv_fixed_format_t format{{2}, {2, TLV_BYTE_ORDER_BIG_ENDIAN}};
    tlv_reader_t reader;
    tlv_result_t result = tlv_reader_init(&reader, wire.data(), wire.size(), &format);
    if (result != TLV_OK) {
        std::fprintf(stderr, "tlv_reader_init: %s\n", tlv_result_string(result));
        return EXIT_FAILURE;
    }

    tlv_element_t element;
    while ((result = tlv_reader_next(&reader, &element)) == TLV_OK) {
        // Both views borrow bytes from wire.
        std::printf("tag");
        print_bytes(element.tag.data, element.tag.size);
        std::printf(", value");
        print_bytes(element.value.data, element.value.size);
        std::putchar('\n');
    }
    if (result != TLV_END) {
        std::fprintf(stderr, "tlv_reader_next: %s\n", tlv_result_string(result));
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
