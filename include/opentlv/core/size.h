// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#ifndef OPENTLV_SIZE_H
#define OPENTLV_SIZE_H

#include "opentlv/core/attributes.h"
#include "opentlv/core/export.h"
#include "opentlv/core/result.h"
#include <stddef.h>
#include <stdint.h>

/** @file
 * @brief Logical TLV byte counts and checked arithmetic.
 * @ingroup core
 */

/** Unsigned 64-bit byte count, independent of the selected wire width. */
typedef uint64_t tlv_size_t;

/** Largest representable logical byte count. */
#define TLV_SIZE_MAX UINT64_MAX

#ifdef __cplusplus
extern "C" {
#endif

/** Convert a logical count to a native byte count.
 * @return TLV_OK on success, TLV_ERR_NULL_ARG for a null output pointer,
 * or TLV_ERR_NATIVE_SIZE when `count > SIZE_MAX`. On failure, the output
 * is unchanged. The caller must check the size of the actual buffer separately.
 */
TLV_NODISCARD OPENTLV_CORE_API tlv_result_t tlv_size_to_native(tlv_size_t count, size_t *out);

/** Add two logical byte counts.
 * @return TLV_OK on success, TLV_ERR_NULL_ARG for a null output pointer,
 * or TLV_ERR_OVERFLOW when the sum exceeds TLV_SIZE_MAX. On failure, the
 * output is unchanged.
 */
TLV_NODISCARD OPENTLV_CORE_API tlv_result_t tlv_size_add(tlv_size_t a, tlv_size_t b,
                                                        tlv_size_t *out);

#ifdef __cplusplus
}
#endif

#endif /* OPENTLV_SIZE_H */
