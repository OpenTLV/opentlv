// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#ifndef OPENTLV_IO_WRITER_H
#define OPENTLV_IO_WRITER_H

#include "opentlv/core/diagnostic.h"
#include "opentlv/core/formats/fixed.h"
#include "opentlv/io/export.h"

/** @file
 * @brief Write complete fixed TLV elements into caller-owned storage.
 * @ingroup io
 */

#ifdef __cplusplus
extern "C" {
#endif

/** A cursor over a writable buffer. Initialize with tlv_writer_init() and
 * treat the fields as read-only. The encoded prefix occupies data[0..pos).
 */
typedef struct tlv_writer {
    uint8_t *data;             /**< Start of the caller's output buffer. */
    size_t capacity;           /**< Writable bytes in the buffer. */
    size_t pos;                /**< Bytes committed, or offset of the next element. */
    tlv_fixed_format_t format; /**< Snapshot of the field widths and length byte order. */
} tlv_writer_t;

/**
 * @brief Encode a tag, its fixed-width payload length, and its payload.
 *
 * format, element and written are required. data may be null only if
 * capacity is zero. A zero-length value may have a null data pointer.
 * @pre The element and format objects and their source bytes must be
 * disjoint from the bytes to be written. Store *written separately from
 * the destination, the input objects and their source bytes.
 *
 * @return TLV_OK on success, with *written set to the encoded size.
 * TLV_ERR_BUFFER_TOO_SHORT also sets *written to the exact required size,
 * including for a null destination with zero capacity. Other errors from
 * argument validation or tlv_fixed_element_size() preserve *written.
 * Every error leaves the destination bytes unchanged.
 */
TLV_NODISCARD OPENTLV_IO_API tlv_result_t tlv_write(uint8_t *data, size_t capacity,
                                                    const tlv_fixed_format_t *format,
                                                    const tlv_element_t *element, size_t *written);

/** As tlv_write(), with an optional diagnostic overwritten on every call.
 * On short output it reports TLV_FIELD_ELEMENT at offset zero, the complete
 * encoded size required, and the supplied capacity.
 * Store the diagnostic separately from all other arguments and their buffers.
 */
TLV_NODISCARD OPENTLV_IO_API tlv_result_t tlv_write_with_diagnostic(
    uint8_t *data, size_t capacity, const tlv_fixed_format_t *format, const tlv_element_t *element,
    size_t *written, tlv_diagnostic_t *diagnostic);

/**
 * @brief Borrow the output buffer, copy the format, and start at offset zero.
 *
 * data may be null only if capacity is zero. Keep the buffer writable
 * while using the writer. Initialization leaves its bytes untouched.
 * To reuse an initialized writer's format, pass &writer->format.
 * @pre Store the writer object outside the output buffer.
 * @return TLV_OK on success, TLV_ERR_NULL_ARG for a missing pointer, or
 * a format error from tlv_fixed_header_size(). Failure preserves *writer.
 */
TLV_NODISCARD OPENTLV_IO_API tlv_result_t tlv_writer_init(tlv_writer_t *writer, uint8_t *data,
                                                          size_t capacity,
                                                          const tlv_fixed_format_t *format);

/**
 * @brief Append one complete element and advance by its encoded size.
 *
 * @pre Use an initialized writer. Store the element and its source bytes
 * separately from the writer object and the range being encoded. An
 * earlier, disjoint part of the same buffer may supply the tag or value.
 * @return TLV_OK on success, TLV_ERR_INVALID_ARG for a position beyond
 * capacity, or an error from tlv_write(). Every error preserves the
 * cursor and the buffer. Use tlv_fixed_element_size() to size a retry.
 */
TLV_NODISCARD OPENTLV_IO_API tlv_result_t tlv_writer_write(tlv_writer_t *writer,
                                                           const tlv_element_t *element);

/** As tlv_writer_write(), with the optional output of tlv_write_with_diagnostic().
 * A short append reports the current cursor offset and remaining capacity.
 * Store the diagnostic separately from the writer, element and their buffers.
 */
TLV_NODISCARD OPENTLV_IO_API tlv_result_t tlv_writer_write_with_diagnostic(
    tlv_writer_t *writer, const tlv_element_t *element, tlv_diagnostic_t *diagnostic);

#ifdef __cplusplus
}
#endif

#endif /* OPENTLV_IO_WRITER_H */
