// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#include <opentlv/tlv.h>

#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    const uint8_t tag[] = {0x12, 0x34};
    const uint8_t value[] = {0xAA, 0xBB, 0xCC};
    const tlv_fixed_format_t format = {{sizeof tag}, {2, TLV_BYTE_ORDER_BIG_ENDIAN}};
    const tlv_element_t element = {{tag, sizeof tag}, {value, sizeof value}};
    uint8_t wire[16];
    size_t written;

    tlv_result_t result = tlv_write(wire, sizeof wire, &format, &element, &written);
    if (result != TLV_OK) {
        fprintf(stderr, "tlv_write: %s\n", tlv_result_string(result));
        return EXIT_FAILURE;
    }

    for (size_t i = 0; i < written; ++i) {
        printf("%02X%c", (unsigned)wire[i], i + 1 == written ? '\n' : ' ');
    }
    return EXIT_SUCCESS;
}
