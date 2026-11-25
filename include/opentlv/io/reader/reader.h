// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#ifndef OPENTLV_IO_READER_H
#define OPENTLV_IO_READER_H

#include "opentlv/core/diagnostic.h"
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
    const uint8_t *data;       /**< Start of the caller's input buffer. */
    size_t size;               /**< Number of readable input bytes. */
    size_t pos;                /**< Byte offset of the next element, or size at end. */
    tlv_fixed_format_t format; /**< Snapshot of the field widths and length byte order. */
} tlv_reader_t;

/**
 * @brief Read the first complete element and report its total encoded size.
 *
 * All pointers are required except that data may be null when size is zero.
 * The caller supplies size readable bytes. On TLV_OK, out_element borrows the
 * input and *consumed counts the tag, length and payload bytes. Trailing
 * bytes are available for subsequent reads. An empty value points just past
 * the header. Keep the backing bytes alive and unchanged while using a view.
 * @pre The storage for *out_element and *consumed must be disjoint from
 * each other and from the input bytes.
 *
 * @return TLV_END for empty input with a valid format, TLV_ERR_TRUNCATED if
 * a nonempty prefix lacks a complete header or payload, TLV_ERR_NULL_ARG for
 * a missing pointer, TLV_ERR_INVALID_ARG for invalid format settings, or
 * TLV_ERR_OVERFLOW if the configured header size cannot fit size_t.
 * Both outputs are unchanged on every result other than TLV_OK.
 */
TLV_NODISCARD OPENTLV_IO_API tlv_result_t tlv_read(const uint8_t *data, size_t size,
                                                   const tlv_fixed_format_t *format,
                                                   tlv_element_t *out_element, size_t *consumed);

/** As tlv_read(), with an optional diagnostic overwritten on every call.
 * On truncation it identifies the first incomplete field, its starting
 * offset within data, and its required and available byte counts.
 * Store the diagnostic separately from all other arguments and their buffers.
 */
TLV_NODISCARD OPENTLV_IO_API tlv_result_t tlv_read_with_diagnostic(const uint8_t *data, size_t size,
                                                                   const tlv_fixed_format_t *format,
                                                                   tlv_element_t *out_element,
                                                                   size_t *consumed,
                                                                   tlv_diagnostic_t *diagnostic);

/**
 * @brief Validate and copy the format, borrow the input, and start at offset zero.
 *
 * data may be null when size is zero. Initialization does not inspect any
 * input bytes. The format may go out of scope after this call. To reuse
 * an initialized reader's format, pass &reader->format as the format.
 * @pre The reader object must occupy storage disjoint from the input bytes.
 * Keep those bytes alive and unchanged while using the reader or any view.
 * @return TLV_OK, TLV_ERR_NULL_ARG, TLV_ERR_INVALID_ARG, or TLV_ERR_OVERFLOW.
 * On failure the previous reader is unchanged.
 */
TLV_NODISCARD OPENTLV_IO_API tlv_result_t tlv_reader_init(tlv_reader_t *reader, const uint8_t *data,
                                                          size_t size,
                                                          const tlv_fixed_format_t *format);

/**
 * @brief Publish the next borrowed element and advance past its payload.
 *
 * @pre The reader must be initialized. The storage for *out_element must
 * be disjoint from the entire reader object (including its format member)
 * and from the input bytes.
 *
 * The returned tag and value point into the input. The value is opaque.
 * Result codes and output lifetimes follow tlv_read(). An offset
 * beyond the supplied input size returns TLV_ERR_INVALID_ARG. On TLV_END
 * or error, both the cursor and the output retain their previous values.
 */
TLV_NODISCARD OPENTLV_IO_API tlv_result_t tlv_reader_next(tlv_reader_t *reader,
                                                          tlv_element_t *out_element);

/** As tlv_reader_next(), with the optional output of tlv_read_with_diagnostic().
 * Field offsets are measured from the beginning of the reader's input.
 * Store the diagnostic separately from the reader, input and output element.
 */
TLV_NODISCARD OPENTLV_IO_API tlv_result_t tlv_reader_next_with_diagnostic(
    tlv_reader_t *reader, tlv_element_t *out_element, tlv_diagnostic_t *diagnostic);

#ifdef __cplusplus
}
#endif

#endif /* OPENTLV_IO_READER_H */
