// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#include "opentlv/io/reader/reader.h"

tlv_result_t tlv_read(const uint8_t *data, size_t size, const tlv_fixed_format_t *format,
                      tlv_element_t *out_element, size_t *consumed)
{
    if (!format || !out_element || !consumed || (!data && size != 0)) {
        return TLV_ERR_NULL_ARG;
    }
    size_t header_size;
    tlv_result_t result = tlv_fixed_header_size(format, &header_size);
    if (result != TLV_OK) {
        return result;
    }
    if (size == 0) {
        return TLV_END;
    }
    if (size < header_size) {
        return TLV_ERR_TRUNCATED;
    }

    const size_t tag_size = format->identifier.size;
    const uint8_t *length_field = data + tag_size;
    tlv_size_t value_size;
    size_t length_bytes;
    result = tlv_fixed_length_read(&format->length, length_field, size - tag_size,
                                   &value_size, &length_bytes);
    if (result != TLV_OK) {
        return result;
    }
    /* Compare before narrowing the wire count or adding it to an offset. */
    if (value_size > size - header_size) {
        return TLV_ERR_TRUNCATED;
    }
    const tlv_element_t element = {{data, tag_size}, {length_field + length_bytes, value_size}};
    *out_element = element;
    *consumed = header_size + (size_t)value_size;
    return TLV_OK;
}

tlv_result_t tlv_reader_init(tlv_reader_t *reader, const uint8_t *data, size_t size,
                             const tlv_fixed_format_t *format)
{
    if (!reader || !format || (!data && size != 0)) {
        return TLV_ERR_NULL_ARG;
    }
    size_t header_size;
    tlv_result_t result = tlv_fixed_header_size(format, &header_size);
    if (result == TLV_OK) {
        const tlv_reader_t initial = {data, size, 0, *format};
        *reader = initial;
    }
    return result;
}

tlv_result_t tlv_reader_next(tlv_reader_t *reader, tlv_element_t *out_element)
{
    if (!reader || !out_element || (!reader->data && reader->size != 0)) {
        return TLV_ERR_NULL_ARG;
    }
    if (reader->pos > reader->size) {
        return TLV_ERR_INVALID_ARG;
    }
    const uint8_t *start = reader->data;
    if (reader->pos != 0) {
        start += reader->pos;
    }
    size_t consumed;
    tlv_result_t result = tlv_read(start, reader->size - reader->pos, &reader->format,
                                   out_element, &consumed);
    if (result == TLV_OK) {
        reader->pos += consumed;
    }
    return result;
}
