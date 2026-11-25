// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#ifndef OPENTLV_FORMATS_FIXED_H
#define OPENTLV_FORMATS_FIXED_H

#include "opentlv/core/element.h"
#include "opentlv/core/field/fixed.h"

/** @file
 * @brief Fixed TLV wire layout and checked element measurement.
 * @ingroup core
 */

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Tag bytes followed by a fixed-width payload length and the payload. */
typedef struct tlv_fixed_format {
    tlv_fixed_identifier_t identifier; /**< Width of the opaque tag. */
    tlv_fixed_length_t length;         /**< Width and byte order of the length. */
} tlv_fixed_format_t;

/**
 * @brief Validate the format and return its tag-plus-length width in octets.
 *
 * Both pointers are required. The tag width must be positive. The length
 * width must be 1 through 8 and its byte order must be big- or little-endian.
 * @return TLV_OK on success, TLV_ERR_NULL_ARG for a null pointer,
 * TLV_ERR_INVALID_ARG for an invalid format, or TLV_ERR_OVERFLOW if the
 * header exceeds SIZE_MAX. On failure *size is unchanged.
 */
TLV_NODISCARD OPENTLV_CORE_API tlv_result_t tlv_fixed_header_size(const tlv_fixed_format_t *format,
                                                                  size_t *size);

/**
 * @brief Validate an element for the format and measure its total wire size.
 *
 * The tag width must match the format. A zero-length value may use NULL.
 * The logical value size must fit both the length field and size_t. A total
 * above SIZE_MAX returns TLV_ERR_OVERFLOW. No tag or value bytes are read.
 * All pointers are required. On any error *size is unchanged.
 * @return TLV_OK, TLV_ERR_NULL_ARG, TLV_ERR_INVALID_ARG (format),
 * TLV_ERR_INVALID_TAG_SIZE, TLV_ERR_NATIVE_SIZE, TLV_ERR_INVALID_LENGTH,
 * or TLV_ERR_OVERFLOW.
 */
TLV_NODISCARD OPENTLV_CORE_API tlv_result_t tlv_fixed_element_size(const tlv_fixed_format_t *format,
                                                                   const tlv_element_t *element,
                                                                   size_t *size);

#ifdef __cplusplus
}
#endif

#endif /* OPENTLV_FORMATS_FIXED_H */
