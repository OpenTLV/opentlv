// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

/* A C99 consumer that compiles opentlv.c beside itself and includes only
 * opentlv.h: no OpenTLV library, include tree or compile definition. */
#include <stdio.h>
#include <string.h>

#include "opentlv.h"

int main(void) {
    static const uint8_t     ber[] = {0x04, 0x03, 'a', 'b', 'c'};
    const tlv_fixed_format_t config = {.identifier = {1}, .length = {1, TLV_BYTE_ORDER_BIG_ENDIAN}};
    const uint8_t            value[] = {'o', 'k'};
    tlv_format_t             fixed;
    tlv_element_t            element;
    uint8_t                  buffer[8];
    size_t                   written = 0;
    size_t                   consumed = 0;

    if (strcmp(tlv_version_string(), OPENTLV_VERSION_STRING) != 0) return 1;
    if (tlv_config_query_frontend() != OPENTLV_QUERY_FRONTEND ||
        tlv_config_query_set_operations() != OPENTLV_QUERY_SET_OPERATIONS)
        return 2;
    if (tlv_read(ber, sizeof ber, &tlv_format_ber, &element, &consumed) != TLV_OK ||
        consumed != sizeof ber || element.value.size != 3 || element.value.data != ber + 2)
        return 3;
    if (tlv_fixed_format_init(&fixed, &config) != TLV_OK ||
        tlv_write(buffer, sizeof buffer, &fixed, TLV_TAG(0x01), value, sizeof value, &written) !=
            TLV_OK ||
        written != 4 || tlv_read(buffer, written, &fixed, &element, &consumed) != TLV_OK ||
        memcmp(element.value.data, value, sizeof value) != 0)
        return 4;
    if (strcmp(tlv_result_string(TLV_ERR_BUFFER_TOO_SHORT), tlv_result_string(TLV_OK)) == 0)
        return 5;
    /* A second builtin shares the same generic Reader and Codec implementation. */
    {
        static const uint8_t lldp[] = {6, 2, 0, 120};
        if (tlv_read(lldp, sizeof lldp, &tlv_format_lldp, &element, &consumed) != TLV_OK ||
            consumed != sizeof lldp || element.value.size != 2 || element.value.data[1] != 120)
            return 6;
    }
    printf("OpenTLV %s C consumer passed\n", tlv_version_string());
    return 0;
}
