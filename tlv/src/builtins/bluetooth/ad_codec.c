// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#include "tlv/builtins/bluetooth/ad_codec.h"
#include "tlv/utf8.h"
#include <string.h>
#include "tlv/codec/values.h"

/* Validators return why the bytes are rejected, or NONE. When the offending
 * bytes are known they set [*begin, *end); otherwise *end stays below *begin. */
static tlv_codec_reason_t validate_flags(const uint8_t* data, size_t size, size_t* begin,
                                         size_t* end) {
    /* A trailing zero octet is not the minimal Flags encoding. */
    if (size && data[size - 1] == 0) {
        *begin = *end = size - 1;
        return TLV_CODEC_REASON_CANONICAL;
    }
    return TLV_CODEC_REASON_NONE;
}

/* Local names are complete UTF-8 byte sequences, even for the shortened AD
 * type: a shortened name must end at a character boundary. */
static tlv_codec_reason_t validate_name(const uint8_t* data, size_t size, size_t* begin,
                                        size_t* end) {
    if (tlv_utf8_validate(data, size) == TLV_OK) return TLV_CODEC_REASON_NONE;
    (void)tlv_utf8_find_invalid(data, size, begin, end);
    return TLV_CODEC_REASON_TEXT;
}

typedef tlv_codec_reason_t (*ad_validator_t)(const uint8_t*, size_t, size_t*, size_t*);

static tlv_result_t decode_span(const uint8_t* data, size_t size, void* value, size_t capacity,
                                ad_validator_t validate, tlv_codec_diagnostic_t* diagnostic) {
    size_t begin = 1, end = 0;
    tlv_codec_reason_t reason = validate(data, size, &begin, &end);
    if (reason != TLV_CODEC_REASON_NONE && begin > end)
        return tlv_codec_diagnostic_reason(diagnostic, TLV_ERR_INVALID_VALUE, reason);
    if (reason != TLV_CODEC_REASON_NONE)
        return tlv_codec_diagnostic_reason_at(diagnostic, TLV_ERR_INVALID_VALUE, reason,
                                              begin == end ? TLV_LOCATION_POINT : TLV_LOCATION_SPAN,
                                              begin, end);
    return tlv_codec_decode(&tlv_codec_bytes, data, size, value, capacity, diagnostic);
}

static tlv_result_t encode_span(const void* value, size_t size, uint8_t* data, size_t capacity,
                                size_t* written, ad_validator_t validate,
                                tlv_codec_diagnostic_t* diagnostic) {
    tlv_codec_diagnostic_init(diagnostic, data ? TLV_CODEC_OP_ENCODE : TLV_CODEC_OP_MEASURE);
    tlv_value_t input;
    size_t length = 0, begin = 0, end = 0;
    tlv_codec_reason_t reason;
    if (size != sizeof(input))
        return tlv_codec_diagnostic_reason(diagnostic, TLV_ERR_INVALID_VALUE,
                                           TLV_CODEC_REASON_OBJECT);
    memcpy(&input, value, sizeof(input));
    if (!input.data && input.size) return tlv_codec_diagnostic_result(diagnostic, TLV_ERR_NULL_ARG);
    if (input.size && tlv_size_to_native(input.size, &length) != TLV_OK)
        return tlv_codec_diagnostic_reason(diagnostic, TLV_ERR_INVALID_VALUE,
                                           TLV_CODEC_REASON_OBJECT);
    /* An application object has no Value coordinates. */
    reason = validate(input.data, length, &begin, &end);
    if (reason != TLV_CODEC_REASON_NONE)
        return tlv_codec_diagnostic_reason(diagnostic, TLV_ERR_INVALID_VALUE, reason);
    return tlv_codec_diagnostic_result(
        diagnostic,
        tlv_codec_encode(&tlv_codec_bytes, value, size, data, capacity, written, diagnostic));
}

static tlv_result_t decode_flags(const void* context, const uint8_t* data, size_t size, void* value,
                                 size_t capacity, tlv_codec_diagnostic_t* diagnostic) {
    tlv_codec_diagnostic_init(diagnostic, TLV_CODEC_OP_DECODE);
    (void)context;
    return tlv_codec_diagnostic_result(
        diagnostic, decode_span(data, size, value, capacity, validate_flags, diagnostic));
}

static tlv_result_t encode_flags(const void* context, const void* value, size_t size, uint8_t* data,
                                 size_t capacity, size_t* written,
                                 tlv_codec_diagnostic_t* diagnostic) {
    tlv_codec_diagnostic_init(diagnostic, data ? TLV_CODEC_OP_ENCODE : TLV_CODEC_OP_MEASURE);
    (void)context;
    return tlv_codec_diagnostic_result(
        diagnostic, encode_span(value, size, data, capacity, written, validate_flags, diagnostic));
}

bool tlv_bluetooth_ad_flags_test(const tlv_value_t* flags, uint8_t mask) {
    return flags && flags->size && flags->data && (flags->data[0] & mask) != 0;
}

static tlv_result_t decode_name(const void* context, const uint8_t* data, size_t size, void* value,
                                size_t capacity, tlv_codec_diagnostic_t* diagnostic) {
    tlv_codec_diagnostic_init(diagnostic, TLV_CODEC_OP_DECODE);
    (void)context;
    return tlv_codec_diagnostic_result(
        diagnostic, decode_span(data, size, value, capacity, validate_name, diagnostic));
}

static tlv_result_t encode_name(const void* context, const void* value, size_t size, uint8_t* data,
                                size_t capacity, size_t* written,
                                tlv_codec_diagnostic_t* diagnostic) {
    tlv_codec_diagnostic_init(diagnostic, data ? TLV_CODEC_OP_ENCODE : TLV_CODEC_OP_MEASURE);
    (void)context;
    return tlv_codec_diagnostic_result(
        diagnostic, encode_span(value, size, data, capacity, written, validate_name, diagnostic));
}

static tlv_result_t decode_tx_power(const void* context, const uint8_t* data, size_t size,
                                    void* value, size_t capacity,
                                    tlv_codec_diagnostic_t* diagnostic) {
    tlv_codec_diagnostic_init(diagnostic, TLV_CODEC_OP_DECODE);
    int8_t result;
    (void)context;
    if (size != 1)
        return tlv_codec_diagnostic_reason(diagnostic, TLV_ERR_INVALID_VALUE,
                                           TLV_CODEC_REASON_LENGTH);
    if (data[0] == 0x80)
        return tlv_codec_diagnostic_reason_at(diagnostic, TLV_ERR_INVALID_VALUE,
                                              TLV_CODEC_REASON_RANGE, TLV_LOCATION_POINT, 0, 0);
    if (capacity < sizeof(result))
        return tlv_codec_diagnostic_result(diagnostic, TLV_ERR_BUFFER_TOO_SHORT);
    /* Subtract in int before narrowing: no out-of-range unsigned-to-signed cast. */
    result = (int8_t)(data[0] < 0x80 ? (int)data[0] : (int)data[0] - 256);
    memcpy(value, &result, sizeof(result));
    return tlv_codec_diagnostic_result(diagnostic, TLV_OK);
}

static tlv_result_t encode_tx_power(const void* context, const void* value, size_t size,
                                    uint8_t* data, size_t capacity, size_t* written,
                                    tlv_codec_diagnostic_t* diagnostic) {
    tlv_codec_diagnostic_init(diagnostic, data ? TLV_CODEC_OP_ENCODE : TLV_CODEC_OP_MEASURE);
    int8_t input;
    (void)context;
    if (size != sizeof(input))
        return tlv_codec_diagnostic_reason(diagnostic, TLV_ERR_INVALID_VALUE,
                                           TLV_CODEC_REASON_OBJECT);
    memcpy(&input, value, sizeof(input));
    if (input == INT8_MIN)
        return tlv_codec_diagnostic_reason(diagnostic, TLV_ERR_INVALID_VALUE,
                                           TLV_CODEC_REASON_RANGE);
    if (data) {
        if (capacity < 1) return tlv_codec_diagnostic_result(diagnostic, TLV_ERR_BUFFER_TOO_SHORT);
        data[0] = (uint8_t)input;
    }
    *written = 1;
    return tlv_codec_diagnostic_result(diagnostic, TLV_OK);
}

const tlv_codec_t tlv_bluetooth_ad_codec_flags = {NULL, decode_flags, encode_flags};
const tlv_codec_t tlv_bluetooth_ad_codec_local_name = {NULL, decode_name, encode_name};
const tlv_codec_t tlv_bluetooth_ad_codec_tx_power = {NULL, decode_tx_power, encode_tx_power};
