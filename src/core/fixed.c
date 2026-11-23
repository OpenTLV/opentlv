// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#include "opentlv/core/formats/fixed.h"

static int valid_length_field(tlv_fixed_length_t field)
{
    return field.size >= 1 && field.size <= 8 &&
           (field.byte_order == TLV_BYTE_ORDER_BIG_ENDIAN ||
            field.byte_order == TLV_BYTE_ORDER_LITTLE_ENDIAN);
}

/* A full-width count always fits. Narrower fields use at most a 56-bit shift. */
static int length_fits(size_t width, tlv_size_t length)
{
    return width == 8 || length < (UINT64_C(1) << (8 * width));
}

/* Map an octet's significance (zero for least significant) to its wire offset. */
static size_t byte_offset(tlv_fixed_length_t field, size_t octet)
{
    return field.byte_order == TLV_BYTE_ORDER_LITTLE_ENDIAN ? octet : field.size - 1 - octet;
}

tlv_result_t tlv_fixed_length_read(const tlv_fixed_length_t *config, const uint8_t *data,
                                   size_t available, tlv_size_t *length, size_t *consumed)
{
    if (!config || !length || !consumed || (!data && available != 0)) {
        return TLV_ERR_NULL_ARG;
    }
    const tlv_fixed_length_t field = *config;
    if (!valid_length_field(field)) {
        return TLV_ERR_INVALID_ARG;
    }
    if (available < field.size) {
        *consumed = available;
        return TLV_ERR_TRUNCATED;
    }
    tlv_size_t result = 0;
    for (size_t i = 0; i < field.size; ++i) {
        result |= (tlv_size_t)data[byte_offset(field, i)] << (8 * i);
    }
    *length = result;
    *consumed = field.size;
    return TLV_OK;
}

tlv_result_t tlv_fixed_length_write(const tlv_fixed_length_t *config, tlv_size_t length,
                                    uint8_t *data, size_t capacity, size_t *written)
{
    if (!config || !written || (!data && capacity != 0)) {
        return TLV_ERR_NULL_ARG;
    }
    const tlv_fixed_length_t field = *config;
    if (!valid_length_field(field)) {
        return TLV_ERR_INVALID_ARG;
    }
    if (!length_fits(field.size, length)) {
        return TLV_ERR_INVALID_LENGTH;
    }
    if (data) {
        if (capacity < field.size) {
            return TLV_ERR_BUFFER_TOO_SHORT;
        }
        for (size_t i = 0; i < field.size; ++i) {
            data[byte_offset(field, i)] = (uint8_t)(length >> (8 * i));
        }
    }
    *written = field.size;
    return TLV_OK;
}

tlv_result_t tlv_fixed_element_size(const tlv_fixed_format_t *format,
                                    const tlv_element_t *element, size_t *size)
{
    if (!format || !element || !size) {
        return TLV_ERR_NULL_ARG;
    }
    if (format->identifier.size == 0 || !valid_length_field(format->length)) {
        return TLV_ERR_INVALID_ARG;
    }
    if (!element->tag.data && element->tag.size != 0) {
        return TLV_ERR_NULL_ARG;
    }
    if (element->tag.size != format->identifier.size) {
        return TLV_ERR_INVALID_TAG_SIZE;
    }
    tlv_result_t result = tlv_value_validate(&element->value);
    if (result != TLV_OK) {
        return result;
    }
    if (!length_fits(format->length.size, element->value.size)) {
        return TLV_ERR_INVALID_LENGTH;
    }
    if (format->identifier.size > SIZE_MAX - format->length.size) {
        return TLV_ERR_OVERFLOW;
    }
    size_t header_size = format->identifier.size + format->length.size;
    size_t payload_size = (size_t)element->value.size;
    if (payload_size > SIZE_MAX - header_size) {
        return TLV_ERR_OVERFLOW;
    }
    *size = header_size + payload_size;
    return TLV_OK;
}
