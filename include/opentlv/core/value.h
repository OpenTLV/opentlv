// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#ifndef OPENTLV_VALUE_H
#define OPENTLV_VALUE_H

#include "opentlv/core/size.h"

/** @file
 * @brief Borrowed value bytes and validation.
 * @ingroup core
 */

/** A read-only view of contiguous value bytes. Keep the backing storage alive
 * and unchanged until the last use of the view or any shallow copy.
 */
typedef struct {
    const uint8_t *data; /**< First value byte. May be null when size is zero. */
    tlv_size_t size;     /**< Logical byte count, excluding framing. */
} tlv_value_t;

#ifdef __cplusplus
extern "C" {
#endif

/** Initialize a borrowed view without reading the backing storage. Before
 * accessing its bytes, the caller must ensure that `size` bytes are readable.
 * An empty view may use a null or nonnull pointer.
 * @return TLV_OK on success, TLV_ERR_NULL_ARG for a null output pointer or
 * null data with positive size, or TLV_ERR_NATIVE_SIZE when size exceeds
 * SIZE_MAX. On failure, the output is unchanged.
 */
TLV_NODISCARD OPENTLV_CORE_API tlv_result_t tlv_value_init(const uint8_t *data, tlv_size_t size,
                                                          tlv_value_t *out);

/** Check the pointer and size without accessing the value bytes. The caller
 * remains responsible for the actual extent and lifetime of the storage.
 * @return TLV_OK for a well-formed view, TLV_ERR_NULL_ARG for a null view
 * pointer or null data with positive size, or TLV_ERR_NATIVE_SIZE when the
 * size exceeds SIZE_MAX.
 */
TLV_NODISCARD OPENTLV_CORE_API tlv_result_t tlv_value_validate(const tlv_value_t *value);

#ifdef __cplusplus
}
#endif

#endif /* OPENTLV_VALUE_H */
