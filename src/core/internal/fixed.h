// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#ifndef OPENTLV_CORE_INTERNAL_FIXED_H
#define OPENTLV_CORE_INTERNAL_FIXED_H

#include "opentlv/core/field/fixed.h"

/* The field is valid and data contains at least field.size readable bytes. */
static inline tlv_size_t decode_fixed_length(tlv_fixed_length_t field, const uint8_t *data)
{
    tlv_size_t length;
    if (field.byte_order == TLV_BYTE_ORDER_LITTLE_ENDIAN) {
        length = data[field.size - 1];
        for (size_t i = field.size - 1; i > 0; --i) {
            length = (length << 8) | data[i - 1];
        }
    } else {
        length = data[0];
        for (size_t i = 1; i < field.size; ++i) {
            length = (length << 8) | data[i];
        }
    }
    return length;
}

/* The field is valid, length fits, and data has at least field.size writable bytes. */
static inline void encode_fixed_length(tlv_fixed_length_t field, tlv_size_t length, uint8_t *data)
{
    if (field.byte_order == TLV_BYTE_ORDER_LITTLE_ENDIAN) {
        data[0] = (uint8_t)length;
        for (size_t i = 1; i < field.size; ++i) {
            length >>= 8;
            data[i] = (uint8_t)length;
        }
    } else {
        data[field.size - 1] = (uint8_t)length;
        for (size_t i = field.size - 1; i > 0; --i) {
            length >>= 8;
            data[i - 1] = (uint8_t)length;
        }
    }
}

#endif /* OPENTLV_CORE_INTERNAL_FIXED_H */
