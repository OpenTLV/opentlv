// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#ifndef OPENTLV_RESULT_H
#define OPENTLV_RESULT_H

#include "opentlv/core/attributes.h"
#include "opentlv/core/export.h"

/** @file
 * @brief Results shared by Core and IO operations.
 * @ingroup core
 */

/** Outcome of a checked operation. Readers return TLV_END on normal exhaustion. */
typedef enum tlv_result {
    TLV_OK = 0,                    /**< The operation succeeded. */
    TLV_END = 1,                   /**< The input ended at an element boundary. */
    TLV_ERR_TRUNCATED = 3,         /**< Input ended inside a field or element. */
    TLV_ERR_INVALID_TAG_SIZE = 5,  /**< The tag width differs from the format's width. */
    TLV_ERR_INVALID_LENGTH = 6,    /**< The length falls outside the format's range. */
    TLV_ERR_NULL_ARG = 10,         /**< A required pointer was null. */
    TLV_ERR_INVALID_ARG = 11,      /**< An argument or format setting is invalid. */
    TLV_ERR_BUFFER_TOO_SHORT = 14, /**< The destination has insufficient capacity. */
    TLV_ERR_OVERFLOW = 16,         /**< A size calculation exceeded its result type's range. */
    TLV_ERR_NATIVE_SIZE = 17       /**< A logical size cannot be represented by size_t. */
} tlv_result_t;

/** Groups related operation outcomes for callers that share recovery logic. */
typedef enum tlv_category {
    TLV_CATEGORY_SUCCESS = 0,          /**< TLV_OK. */
    TLV_CATEGORY_END = 1,              /**< TLV_END at an element boundary. */
    TLV_CATEGORY_INVALID_ARGUMENT = 2, /**< TLV_ERR_NULL_ARG or TLV_ERR_INVALID_ARG. */
    TLV_CATEGORY_MALFORMED_DATA = 3,   /**< TLV_ERR_INVALID_TAG_SIZE or TLV_ERR_INVALID_LENGTH
                                           for an element that cannot be encoded in its format. */
    TLV_CATEGORY_TRUNCATED = 4,        /**< TLV_ERR_TRUNCATED for incomplete input. */
    TLV_CATEGORY_CAPACITY = 5,         /**< TLV_ERR_BUFFER_TOO_SHORT for an undersized output. */
    TLV_CATEGORY_RANGE = 6,            /**< TLV_ERR_OVERFLOW or TLV_ERR_NATIVE_SIZE. */
    TLV_CATEGORY_UNKNOWN = 7           /**< Unrecognized result code. */
} tlv_result_category_t;

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Classify a result code. Unrecognized codes return TLV_CATEGORY_UNKNOWN. */
TLV_NODISCARD OPENTLV_CORE_API tlv_result_category_t tlv_result_category(tlv_result_t result);

/**
 * @brief Describe a result code in English for logs and diagnostics.
 *
 * The returned non-null string is read-only and has static storage duration.
 * Unrecognized codes return "unknown result".
 */
TLV_NODISCARD OPENTLV_CORE_API const char *tlv_result_string(tlv_result_t result);

#ifdef __cplusplus
}
#endif

#endif /* OPENTLV_RESULT_H */
