// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#ifndef OPENTLV_CORE_INTERNAL_FIXED_H
#define OPENTLV_CORE_INTERNAL_FIXED_H

#include "opentlv/core/field/fixed.h"

/* Map an octet's significance (zero for least significant) to its wire offset. */
static inline size_t byte_offset(tlv_fixed_length_t field, size_t octet)
{
    return field.byte_order == TLV_BYTE_ORDER_LITTLE_ENDIAN ? octet : field.size - 1 - octet;
}

/* The field is valid and data contains at least field.size readable bytes. */
static inline tlv_size_t decode_fixed_length(tlv_fixed_length_t field, const uint8_t *data)
{
    tlv_size_t length = 0;
    for (size_t i = 0; i < field.size; ++i) {
        length |= (tlv_size_t)data[byte_offset(field, i)] << (8 * i);
    }
    return length;
}

/* The field is valid, length fits, and data has at least field.size writable bytes. */
static inline void encode_fixed_length(tlv_fixed_length_t field, tlv_size_t length, uint8_t *data)
{
    for (size_t i = 0; i < field.size; ++i) {
        data[byte_offset(field, i)] = (uint8_t)(length >> (8 * i));
    }
}

#endif /* OPENTLV_CORE_INTERNAL_FIXED_H */
