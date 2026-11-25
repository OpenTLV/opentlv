// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#include <opentlv/tlv.h>

/* Keep an optimized C99 caller at the same measurement boundary as each
 * C++ caller. Both select a null diagnostic at compile time. */
tlv_result_category_t c_category(tlv_result_t code)
{
    return tlv_result_category(code);
}

tlv_result_t c_value(const uint8_t *data, tlv_size_t size, tlv_value_t *out)
{
    return tlv_value_init(data, size, out);
}

tlv_result_t c_measure(const tlv_fixed_format_t *format, const tlv_element_t *element, size_t *size)
{
    return tlv_fixed_element_size(format, element, size);
}

tlv_result_t c_read(const uint8_t *data, size_t size, const tlv_fixed_format_t *format,
                    tlv_element_t *out, size_t *count)
{
    return tlv_read_with_diagnostic(data, size, format, out, count, NULL);
}

tlv_result_t c_next(tlv_reader_t *cursor, tlv_element_t *out)
{
    return tlv_reader_next_with_diagnostic(cursor, out, NULL);
}

tlv_result_t c_write(uint8_t *data, size_t capacity, const tlv_fixed_format_t *format,
                     const tlv_element_t *source, size_t *count)
{
    return tlv_write_with_diagnostic(data, capacity, format, source, count, NULL);
}

tlv_result_t c_append(tlv_writer_t *cursor, const tlv_element_t *source)
{
    return tlv_writer_write_with_diagnostic(cursor, source, NULL);
}
