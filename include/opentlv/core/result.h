// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#ifndef OPENTLV_RESULT_H
#define OPENTLV_RESULT_H

/** @file
 * @brief Results shared by Core and IO operations.
 * @ingroup core
 */

/** Outcome of a checked operation. Readers return TLV_END on normal exhaustion. */
typedef enum tlv_result {
    TLV_OK = 0,                  /**< The operation succeeded. */
    TLV_END = 1,                 /**< The input ended at an element boundary. */
    TLV_ERR_TRUNCATED = 3,       /**< Input ended inside a field or element. */
    TLV_ERR_INVALID_TAG_SIZE = 5, /**< The tag width differs from the format's width. */
    TLV_ERR_INVALID_LENGTH = 6,  /**< The length falls outside the format's range. */
    TLV_ERR_NULL_ARG = 10,       /**< A required pointer was null. */
    TLV_ERR_INVALID_ARG = 11,    /**< An argument or format setting is invalid. */
    TLV_ERR_BUFFER_TOO_SHORT = 14, /**< The destination has insufficient capacity. */
    TLV_ERR_OVERFLOW = 16,       /**< A size calculation exceeded its result type's range. */
    TLV_ERR_NATIVE_SIZE = 17     /**< A logical size cannot be represented by size_t. */
} tlv_result_t;

#endif /* OPENTLV_RESULT_H */
