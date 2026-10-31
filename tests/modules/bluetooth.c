// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#include "tlv/builtins/bluetooth/bluetooth_ltv.h"
#include <string.h>

#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) return __LINE__;                                                                 \
    } while (0)

/* This executable links only the owning family and its declared dependencies.
 * The same source runs from a relocated installed package in the isolation gate. */
int main(void) {
    const uint8_t       wire[] = {0x02, 0x01, 0x06};
    const uint8_t       truncated[] = {0x02};
    const tlv_format_t* formats[] = {&tlv_format_bluetooth_ltv};
    size_t              i;
    for (i = 0; i < sizeof formats / sizeof formats[0]; ++i) {
        tlv_decoded_t      decoded;
        tlv_format_error_t error;
        uint8_t            output[sizeof wire];
        size_t             written = 0;
        CHECK(tlv_format_decode(formats[i], wire, sizeof wire, &decoded, &error) == TLV_OK);
        CHECK(decoded.element.value.size == 1);
        CHECK(decoded.element.value.data[0] == wire[sizeof wire - 1]);
        CHECK(tlv_format_encode(formats[i], &decoded.element, output, sizeof output, &written,
                                &error) == TLV_OK);
        CHECK(written == sizeof wire && memcmp(output, wire, written) == 0);
        CHECK(tlv_format_decode(formats[i], truncated, sizeof truncated, &decoded, &error) ==
              TLV_ERR_TRUNCATED);
    }
    return 0;
}
