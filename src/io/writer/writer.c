// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#include "opentlv/io/writer/writer.h"
#include "../../core/internal/fixed.h"
#include "../internal/diagnostic.h"

#include <string.h>

/* Both entry points supply a valid count pointer and a bounded output slice. */
static tlv_result_t write_element(uint8_t *data, size_t capacity, const tlv_fixed_format_t *format,
                                  const tlv_element_t *element, size_t *written,
                                  tlv_diagnostic_t *diagnostic)
{
    size_t required;
    tlv_result_t result = tlv_fixed_element_size(format, element, &required);
    if (result != TLV_OK) {
        return report_result(diagnostic, result);
    }
#ifdef __clang_analyzer__
    /* Successful measurement in Core includes a nonempty tag and length field. */
    __builtin_assume(required > 0);
#endif
    if (required > capacity) {
        *written = required;
        if (diagnostic) {
            set_field(diagnostic, TLV_ERR_BUFFER_TOO_SHORT, TLV_FIELD_ELEMENT, 0, required,
                      capacity);
        }
        return TLV_ERR_BUFFER_TOO_SHORT;
    }

    const size_t tag_size = element->tag.size;
    /* Measurement proved the length fits its field, and the whole element fits here. */
    encode_fixed_length(format->length, element->value.size, data + tag_size);
    memcpy(data, element->tag.data, tag_size);
    if (element->value.size != 0) {
        memcpy(data + tag_size + format->length.size, element->value.data,
               (size_t)element->value.size);
    }
    *written = required;
    return report_result(diagnostic, TLV_OK);
}

tlv_result_t tlv_write(uint8_t *data, size_t capacity, const tlv_fixed_format_t *format,
                       const tlv_element_t *element, size_t *written)
{
    return tlv_write_with_diagnostic(data, capacity, format, element, written, NULL);
}

tlv_result_t tlv_write_with_diagnostic(uint8_t *data, size_t capacity,
                                       const tlv_fixed_format_t *format,
                                       const tlv_element_t *element, size_t *written,
                                       tlv_diagnostic_t *diagnostic)
{
    if (!written || (!data && capacity != 0)) {
        return report_result(diagnostic, TLV_ERR_NULL_ARG);
    }
    return write_element(data, capacity, format, element, written, diagnostic);
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
    return tlv_writer_write_with_diagnostic(writer, element, NULL);
}

tlv_result_t tlv_writer_write_with_diagnostic(tlv_writer_t *writer, const tlv_element_t *element,
                                              tlv_diagnostic_t *diagnostic)
{
    if (!writer || (!writer->data && writer->capacity != 0)) {
        return report_result(diagnostic, TLV_ERR_NULL_ARG);
    }
    if (writer->pos > writer->capacity) {
        return report_result(diagnostic, TLV_ERR_INVALID_ARG);
    }
    uint8_t *dest = writer->data;
    if (writer->pos != 0) {
        dest += writer->pos;
    }
    size_t written;
    tlv_result_t result = write_element(dest, writer->capacity - writer->pos, &writer->format,
                                        element, &written, diagnostic);
    if (result == TLV_OK) {
        writer->pos += written;
    } else if (diagnostic && result == TLV_ERR_BUFFER_TOO_SHORT) {
        diagnostic->offset = writer->pos;
    }
    return result;
}
