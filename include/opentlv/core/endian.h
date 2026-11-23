// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#ifndef OPENTLV_ENDIAN_H
#define OPENTLV_ENDIAN_H

/** @file
 * @brief Wire byte order for integer fields.
 * @ingroup core
 */

/** @brief Byte order of an unsigned wire integer. A format must select an order. */
typedef enum tlv_byte_order {
    TLV_BYTE_ORDER_UNKNOWN = 0,
    TLV_BYTE_ORDER_BIG_ENDIAN,
    TLV_BYTE_ORDER_LITTLE_ENDIAN
} tlv_byte_order_t;

#endif /* OPENTLV_ENDIAN_H */
