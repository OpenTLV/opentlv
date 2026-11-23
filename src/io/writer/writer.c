// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#include "opentlv/io/writer/writer.h"

#include <string.h>

tlv_result_t tlv_write(uint8_t *data, size_t capacity, const tlv_fixed_format_t *format,
                       const tlv_element_t *element, size_t *written)
{
    if (!written || (!data && capacity != 0)) {
        return TLV_ERR_NULL_ARG;
    }
    size_t required;
    tlv_result_t result = tlv_fixed_element_size(format, element, &required);
    if (result != TLV_OK) {
        return result;
    }
    if (required > capacity) {
        *written = required;
        return TLV_ERR_BUFFER_TOO_SHORT;
    }

    const size_t tag_size = element->tag.size;
    size_t length_size;
    result = tlv_fixed_length_write(&format->length, element->value.size, data + tag_size,
                                    capacity - tag_size, &length_size);
    if (result != TLV_OK) {
        return result;
    }
    memcpy(data, element->tag.data, tag_size);
    if (element->value.size != 0) {
        memcpy(data + tag_size + length_size, element->value.data, (size_t)element->value.size);
    }
    *written = required;
    return TLV_OK;
}

tlv_result_t tlv_writer_init(tlv_writer_t *writer, uint8_t *data, size_t capacity,
                             const tlv_fixed_format_t *format)
{
    if (!writer || (!data && capacity != 0)) {
        return TLV_ERR_NULL_ARG;
    }
    size_t header_size;
    tlv_result_t result = tlv_fixed_header_size(format, &header_size);
    if (result == TLV_OK) {
        const tlv_writer_t initial = {data, capacity, 0, *format};
        *writer = initial;
    }
    return result;
}

tlv_result_t tlv_writer_write(tlv_writer_t *writer, const tlv_element_t *element)
{
    if (!writer || (!writer->data && writer->capacity != 0)) {
        return TLV_ERR_NULL_ARG;
    }
    if (writer->pos > writer->capacity) {
        return TLV_ERR_INVALID_ARG;
    }
    uint8_t *dest = writer->data;
    if (writer->pos != 0) {
        dest += writer->pos;
    }
    size_t written;
    tlv_result_t result = tlv_write(dest, writer->capacity - writer->pos, &writer->format,
                                    element, &written);
    if (result == TLV_OK) {
        writer->pos += written;
    }
    return result;
}
