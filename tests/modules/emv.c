// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#include "opentlv/builtin/emv/format.h"
#include "opentlv/builtin/emv/dol.h"
#include <string.h>

#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) return __LINE__;                                                                 \
    } while (0)

/* This executable links only the owning family and its declared dependencies.
 * The same source runs from a relocated installed package in the isolation gate. */
int main(void) {
    const uint8_t       wire[] = {0x5a, 0x01, 0x12};
    const uint8_t       truncated[] = {0x5a};
    const tlv_format_t* formats[] = {&tlv_format_emv};
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
    {
        const uint8_t dol[] = {0x9f, 0x02, 0x06, 0x5a, 0x08};
        size_t        written = 0;
        CHECK(tlv_dol_write(dol, sizeof dol, NULL, 0, NULL, NULL, NULL, &written, NULL) == TLV_OK);
        CHECK(written == 14);
    }
    return 0;
}
