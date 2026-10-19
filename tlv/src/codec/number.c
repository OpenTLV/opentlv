// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#include "tlv/codec/number.h"
#include "tlv/endian.h"
#include <string.h>

static int valid_config(const tlv_number_codec_config_t* config) {
    if (config->encoding == TLV_NUMBER_BCD)
        return config->width <= 9 && config->digits >= 1 && config->digits <= 18;
    return (config->encoding == TLV_NUMBER_BINARY_BE || config->encoding == TLV_NUMBER_BINARY_LE) &&
           config->width <= 8 && config->digits == 0;
}

static uint64_t decimal_limit(unsigned digits) {
    uint64_t limit = 1;
    while (digits--) limit *= 10;
    return limit - 1;
}

tlv_result_t tlv_number_decode(const void* context, const uint8_t* data, size_t size, void* value,
                               size_t capacity, tlv_codec_diagnostic_t* diagnostic) {
    tlv_codec_diagnostic_init(diagnostic, TLV_CODEC_OP_DECODE);
    const tlv_number_codec_config_t* config = (const tlv_number_codec_config_t*)context;
    uint64_t number = 0;
    if (!config || !value || (!data && size))
        return tlv_codec_diagnostic_result(diagnostic, TLV_ERR_NULL_ARG);
    if (!valid_config(config)) return tlv_codec_diagnostic_result(diagnostic, TLV_ERR_INVALID_ARG);
    if (!size || size > (config->encoding == TLV_NUMBER_BCD ? 9U : 8U) ||
        (config->width && size != config->width))
        return tlv_codec_diagnostic_reason(diagnostic, TLV_ERR_INVALID_VALUE,
                                           TLV_CODEC_REASON_LENGTH);
    if (config->encoding == TLV_NUMBER_BCD) {
        for (size_t i = 0; i < size; ++i) {
            unsigned high = data[i] >> 4, low = data[i] & 15;
            if (high > 9 || low > 9)
                return tlv_codec_diagnostic_reason_at(diagnostic, TLV_ERR_INVALID_VALUE,
                                                      TLV_CODEC_REASON_DIGIT, TLV_LOCATION_POINT, i,
                                                      i);
            number = number * 100 + (uint64_t)high * 10 + low;
        }
        if (number > decimal_limit(config->digits))
            return tlv_codec_diagnostic_reason(diagnostic, TLV_ERR_INVALID_VALUE,
                                               TLV_CODEC_REASON_RANGE);
    } else {
        tlv_byte_order_t order = config->encoding == TLV_NUMBER_BINARY_BE
                                     ? TLV_BYTE_ORDER_BIG_ENDIAN
                                     : TLV_BYTE_ORDER_LITTLE_ENDIAN;
        if (tlv_read_uint(data, size, order, &number) != TLV_OK)
            return tlv_codec_diagnostic_reason(diagnostic, TLV_ERR_INVALID_VALUE,
                                               TLV_CODEC_REASON_RANGE);
    }
    if (capacity < sizeof(number))
        return tlv_codec_diagnostic_result(diagnostic, TLV_ERR_BUFFER_TOO_SHORT);
    memcpy(value, &number, sizeof(number));
    return tlv_codec_diagnostic_result(diagnostic, TLV_OK);
}

tlv_result_t tlv_number_encode(const void* context, const void* value, size_t size, uint8_t* data,
                               size_t capacity, size_t* written,
                               tlv_codec_diagnostic_t* diagnostic) {
    tlv_codec_diagnostic_init(diagnostic, data ? TLV_CODEC_OP_ENCODE : TLV_CODEC_OP_MEASURE);
    const tlv_number_codec_config_t* config = (const tlv_number_codec_config_t*)context;
    uint64_t number, remaining;
    uint8_t bytes[9] = {0};
    size_t width = 1;
    unsigned radix;
    if (!written) return tlv_codec_diagnostic_result(diagnostic, TLV_ERR_NULL_ARG);
    *written = 0;
    if (!config || !value || (!data && capacity))
        return tlv_codec_diagnostic_result(diagnostic, TLV_ERR_NULL_ARG);
    if (!valid_config(config)) return tlv_codec_diagnostic_result(diagnostic, TLV_ERR_INVALID_ARG);
    if (size != sizeof(number))
        return tlv_codec_diagnostic_reason(diagnostic, TLV_ERR_INVALID_VALUE,
                                           TLV_CODEC_REASON_OBJECT);
    memcpy(&number, value, sizeof(number));
    if (config->encoding == TLV_NUMBER_BCD && number > decimal_limit(config->digits))
        return tlv_codec_diagnostic_reason(diagnostic, TLV_ERR_INVALID_VALUE,
                                           TLV_CODEC_REASON_RANGE);
    radix = config->encoding == TLV_NUMBER_BCD ? 100 : 256;
    remaining = number;
    while (remaining >= radix) {
        remaining /= radix;
        ++width;
    }
    if (config->width) {
        if (width > config->width)
            return tlv_codec_diagnostic_reason(diagnostic, TLV_ERR_INVALID_VALUE,
                                               TLV_CODEC_REASON_RANGE);
        width = config->width;
    }
    if (data && capacity < width)
        return tlv_codec_diagnostic_result(diagnostic, TLV_ERR_BUFFER_TOO_SHORT);
    if (config->encoding == TLV_NUMBER_BCD) {
        for (size_t i = width; i > 0; --i) {
            unsigned pair = (unsigned)(number % 100);
            bytes[i - 1] = (uint8_t)(((pair / 10) << 4) | (pair % 10));
            number /= 100;
        }
    } else {
        tlv_byte_order_t order = config->encoding == TLV_NUMBER_BINARY_BE
                                     ? TLV_BYTE_ORDER_BIG_ENDIAN
                                     : TLV_BYTE_ORDER_LITTLE_ENDIAN;
        if (tlv_write_uint(bytes, width, order, number) != TLV_OK)
            return tlv_codec_diagnostic_reason(diagnostic, TLV_ERR_INVALID_VALUE,
                                               TLV_CODEC_REASON_RANGE);
    }
    if (data) memcpy(data, bytes, width);
    *written = width;
    return tlv_codec_diagnostic_result(diagnostic, TLV_OK);
}
