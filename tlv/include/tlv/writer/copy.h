// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#ifndef OPENTLV_WRITER_COPY_H
#define OPENTLV_WRITER_COPY_H

#include "tlv/copy.h"
#include "tlv/format.h"
#include "tlv/io/export.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Serializes an element's tag, length and value using a writer format.
 *
 * Encodes as tlv_write() does, regenerating the length from `value.size`,
 * so the result may differ from the bytes the element was read from. Use
 * tlv_source_preserve() to check unchanged content and retain original framing,
 * or tlv_copy_encoded() to copy an unvalidated byte range exactly.
 *
 * Requires the format's `measure` and `encode`
 * callbacks. The element's value bytes must not overlap the destination element.
 *
 * @param[in]  element     Element to serialize.
 * @param[in]  format   Writer format used to encode the element.
 * @param[out] data     Destination buffer. `NULL` with zero `capacity` queries
 *                      the required size.
 * @param[in]  capacity Destination capacity in bytes.
 * @param[out] written  Receives the encoded size.
 *
 * @return #TLV_OK on success.
 * @return #TLV_ERR_NULL_ARG for missing required pointers or callbacks.
 * @return #TLV_ERR_BUFFER_TOO_SHORT if `capacity` is insufficient.
 * @return Any callback error, propagated unchanged.
 *
 * @warning Callback errors may leave `data` modified. Size queries never
 *          write `data`.
 * @see tlv_write, tlv_copy_encoded
 */
OPENTLV_IO_API tlv_result_t tlv_copy_element(const tlv_element_t* element,
                                             const tlv_format_t* format, uint8_t* data,
                                             size_t capacity, size_t* written);

#ifdef __cplusplus
}
#endif
#endif
