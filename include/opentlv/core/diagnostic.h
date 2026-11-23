// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#ifndef OPENTLV_CORE_DIAGNOSTIC_H
#define OPENTLV_CORE_DIAGNOSTIC_H

#include "opentlv/core/result.h"
#include <stddef.h>
#include <stdint.h>

/** @file
 * @brief Byte counts and positions reported by bounded fixed TLV operations.
 * @ingroup core
 */

/** Part of an element for which byte counts are available. */
typedef enum tlv_field {
    TLV_FIELD_NONE = 0, /**< No field or byte counts apply. */
    TLV_FIELD_TAG,      /**< Incomplete input tag. */
    TLV_FIELD_LENGTH,   /**< Incomplete input length field. */
    TLV_FIELD_VALUE,    /**< Incomplete input payload. */
    TLV_FIELD_ELEMENT   /**< Insufficient space for the entire output element. */
} tlv_field_t;

/**
 * Optional caller-owned output, fully populated on every diagnostic call.
 * offset is relative to the start of the buffer supplied to the one-shot
 * operation or to cursor initialization. Counts refer to the indicated
 * field and can represent both a wire length and a native buffer size.
 * When field is TLV_FIELD_NONE, offset, required and available are zero.
 */
typedef struct tlv_diagnostic {
    tlv_result_t result; /**< Same result as the return value, including TLV_OK and TLV_END. */
    tlv_field_t field;   /**< Input field on truncation, whole element on short output. */
    size_t offset;       /**< First byte of the field, or the start of the proposed write. */
    uintmax_t required;  /**< Complete size of this field or output element in bytes. */
    uintmax_t available; /**< Bytes available from offset for this field or element. */
} tlv_diagnostic_t;

#endif /* OPENTLV_CORE_DIAGNOSTIC_H */
