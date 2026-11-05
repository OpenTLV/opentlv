// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#include "opentlv/builtin/dhcp/dhcpv4.h"
#include "opentlv/core/formats/compose.h"

static const uint8_t dhcpv4_codes[] = {0, 255};
static const tlv_tag_t dhcpv4_tag_only[] = {{&dhcpv4_codes[0], 1}, {&dhcpv4_codes[1], 1}};
static const tlv_tagged_binary_composition_t options = {
    {{1}, {1, TLV_BYTE_ORDER_BIG_ENDIAN}, TLV_ELEMENT_ORDER_TLV, TLV_LENGTH_SCOPE_VALUE},
    dhcpv4_tag_only,
    2};

const tlv_format_t tlv_format_dhcpv4 = {&options, tlv_tagged_binary_decode,
                                        tlv_tagged_binary_measure, tlv_tagged_binary_encode, NULL};
