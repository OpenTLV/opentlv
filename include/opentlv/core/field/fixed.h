// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#ifndef OPENTLV_FIELD_FIXED_H
#define OPENTLV_FIELD_FIXED_H

#include "opentlv/core/attributes.h"
#include "opentlv/core/endian.h"
#include "opentlv/core/export.h"
#include "opentlv/core/size.h"

/** @file
 * @brief Fixed identifier widths and bounded conversion of wire lengths.
 * @ingroup core
 */

#ifdef __cplusplus
extern "C" {
#endif

/** @brief A fixed-width identifier of one or more uninterpreted bytes. */
typedef struct tlv_fixed_identifier {
    size_t size; /**< Number of tag bytes, in wire order. */
} tlv_fixed_identifier_t;

/** @brief A fixed-width unsigned wire length with an explicit byte order. */
typedef struct tlv_fixed_length {
    size_t size;                 /**< Field width in octets, from 1 through 8. */
    tlv_byte_order_t byte_order; /**< Big-endian or little-endian. */
} tlv_fixed_length_t;

/**
 * @brief Decode one length field into a 64-bit count.
 *
 * Read the first config->size bytes of data and set *consumed to that width.
 * data may be NULL only for zero available bytes. On TLV_ERR_TRUNCATED,
 * set *consumed to available and preserve *length. Argument errors preserve
 * both outputs. The length and consumed outputs must use separate storage.
 * This operation does not inspect the payload.
 * @return TLV_OK, TLV_ERR_NULL_ARG, TLV_ERR_INVALID_ARG (width or order),
 * or TLV_ERR_TRUNCATED.
 */
TLV_NODISCARD OPENTLV_CORE_API tlv_result_t tlv_fixed_length_read(
    const tlv_fixed_length_t *config, const uint8_t *data, size_t available,
    tlv_size_t *length, size_t *consumed);

/**
 * @brief Encode one length field, or measure its width with NULL and zero capacity.
 *
 * *written receives the field width on success. Reject a count outside the
 * configured field's range with TLV_ERR_INVALID_LENGTH, or a short destination
 * with TLV_ERR_BUFFER_TOO_SHORT. All failures preserve the destination and
 * *written. The destination and written output must use separate storage.
 * @return TLV_OK, TLV_ERR_NULL_ARG, TLV_ERR_INVALID_ARG (width or order),
 * TLV_ERR_INVALID_LENGTH, or TLV_ERR_BUFFER_TOO_SHORT.
 */
TLV_NODISCARD OPENTLV_CORE_API tlv_result_t tlv_fixed_length_write(
    const tlv_fixed_length_t *config, tlv_size_t length, uint8_t *data,
    size_t capacity, size_t *written);

#ifdef __cplusplus
}
#endif

#endif /* OPENTLV_FIELD_FIXED_H */
