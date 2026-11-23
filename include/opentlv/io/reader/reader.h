// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#ifndef OPENTLV_IO_READER_H
#define OPENTLV_IO_READER_H

#include "opentlv/core/formats/fixed.h"
#include "opentlv/io/export.h"

/** @file
 * @brief Read fixed TLV elements from a complete, bounded input buffer.
 * @ingroup io
 */

#ifdef __cplusplus
extern "C" {
#endif

/** A cursor over borrowed bytes. Initialize with tlv_reader_init() and treat
 * the fields as read-only. Copying an initialized reader creates an independent
 * cursor over the same bytes.
 */
typedef struct tlv_reader {
    const uint8_t *data;        /**< Start of the caller's input buffer. */
    size_t size;                /**< Number of readable input bytes. */
    size_t pos;                 /**< Byte offset of the next element, or size at end. */
    tlv_fixed_format_t format;  /**< Snapshot of the field widths and length byte order. */
} tlv_reader_t;

/**
 * @brief Read the first complete element and report its total encoded size.
 *
 * All pointers are required except that data may be null when size is zero.
 * The caller supplies size readable bytes. On TLV_OK, out_element borrows the
 * input and *consumed counts the tag, length and payload bytes. Trailing
 * bytes are available for subsequent reads. An empty value points just past
 * the header. Keep the backing bytes alive and unchanged while using a view.
 * Use separate storage for the input and the two outputs.
 *
 * @return TLV_END for empty input with a valid format, TLV_ERR_TRUNCATED if
 * a nonempty prefix lacks a complete header or payload, TLV_ERR_NULL_ARG for
 * a missing pointer, TLV_ERR_INVALID_ARG for invalid format settings, or
 * TLV_ERR_OVERFLOW if the configured header size cannot fit size_t.
 * Both outputs are unchanged on every result other than TLV_OK.
 */
TLV_NODISCARD OPENTLV_IO_API tlv_result_t tlv_read(
    const uint8_t *data, size_t size, const tlv_fixed_format_t *format,
    tlv_element_t *out_element, size_t *consumed);

/**
 * @brief Validate and copy the format, borrow the input, and start at offset zero.
 *
 * data may be null when size is zero. Initialization does not inspect any
 * input bytes. The format may go out of scope after this call. The input
 * must outlive the reader and every borrowed view still in use.
 * @return TLV_OK, TLV_ERR_NULL_ARG, TLV_ERR_INVALID_ARG, or TLV_ERR_OVERFLOW.
 * On failure the previous reader is unchanged.
 */
TLV_NODISCARD OPENTLV_IO_API tlv_result_t tlv_reader_init(
    tlv_reader_t *reader, const uint8_t *data, size_t size, const tlv_fixed_format_t *format);

/**
 * @brief Publish the next borrowed element and advance past its payload.
 *
 * Use an initialized reader and a separate output object. The value is
 * opaque. Result codes and output lifetimes follow tlv_read(). An offset
 * beyond the supplied input size returns TLV_ERR_INVALID_ARG. On TLV_END
 * or error, both the cursor and the output retain their previous values.
 */
TLV_NODISCARD OPENTLV_IO_API tlv_result_t tlv_reader_next(
    tlv_reader_t *reader, tlv_element_t *out_element);

#ifdef __cplusplus
}
#endif

#endif /* OPENTLV_IO_READER_H */
