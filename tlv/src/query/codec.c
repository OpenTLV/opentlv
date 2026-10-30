// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel
#include "tlv/query/adapters.h"
#include "tlv/config.h"
#include "tlv/codec/values.h"
#include "tlv/codec/number.h"
#include "tlv/utf8.h"
#include <string.h>
#if OPENTLV_CODEC
static tlv_result_t codec_decode(const void* context, const tlv_tree_event_t* event,
                                 const uint8_t* data, size_t size, void* scratch, size_t capacity,
                                 tlv_query_result_t* result, tlv_codec_diagnostic_t* diagnostic) {
    tlv_codec_diagnostic_init(diagnostic, TLV_CODEC_OP_DECODE);
    (void)event;
    if (!context || !scratch || !result)
        return tlv_codec_diagnostic_result(diagnostic, TLV_ERR_NULL_ARG);
    const tlv_query_codec_adapter_t* adapter = context;
    if (adapter->representation < TLV_QUERY_CODEC_SIGNED ||
        adapter->representation > TLV_QUERY_CODEC_UTF8)
        return tlv_codec_diagnostic_result(diagnostic, TLV_ERR_INVALID_ARG);
    size_t needed =
        adapter->representation == TLV_QUERY_CODEC_UTF8 ? sizeof(tlv_value_t) : sizeof(uint64_t);
    if (capacity < needed) return tlv_codec_diagnostic_result(diagnostic, TLV_ERR_BUFFER_TOO_SHORT);
    tlv_result_t rc = tlv_codec_decode(adapter->codec, data, size, scratch, capacity, diagnostic);
    if (rc != TLV_OK) return tlv_codec_diagnostic_result(diagnostic, rc);
    tlv_query_result_t out = {0};
    out.kind = TLV_QUERY_RESULT_INTEGER;
    if (adapter->representation == TLV_QUERY_CODEC_SIGNED)
        memcpy(&out.integer, scratch, sizeof out.integer);
    else if (adapter->representation == TLV_QUERY_CODEC_UNSIGNED) {
        uint64_t n;
        memcpy(&n, scratch, sizeof n);
        if (n > INT64_MAX)
            return tlv_codec_diagnostic_reason(diagnostic, TLV_ERR_INVALID_VALUE,
                                               TLV_CODEC_REASON_RANGE);
        out.integer = (int64_t)n;
    } else {
        tlv_value_t span;
        memcpy(&span, scratch, sizeof span);
        if (span.size > SIZE_MAX || tlv_utf8_validate(span.data, (size_t)span.size) != TLV_OK) {
            /* Text that borrows the Value locates the malformed sequence in it. */
            uintptr_t first = (uintptr_t)data, text = (uintptr_t)span.data;
            size_t begin, end;
            if (diagnostic && span.size <= SIZE_MAX && text >= first && text - first <= size &&
                span.size <= size - (text - first) &&
                tlv_utf8_find_invalid(span.data, (size_t)span.size, &begin, &end))
                return tlv_codec_diagnostic_reason_at(diagnostic, TLV_ERR_INVALID_VALUE,
                                                      TLV_CODEC_REASON_TEXT, TLV_LOCATION_SPAN,
                                                      (text - first) + begin, (text - first) + end);
            return tlv_codec_diagnostic_reason(diagnostic, TLV_ERR_INVALID_VALUE,
                                               TLV_CODEC_REASON_TEXT);
        }
        out.kind = TLV_QUERY_RESULT_STRING;
        out.data = span.data;
        out.size = (size_t)span.size;
    }
    *result = out;
    return tlv_codec_diagnostic_result(diagnostic, TLV_OK);
}
tlv_result_t tlv_query_codec_decode(const void* context, const tlv_tree_event_t* event,
                                    const uint8_t* data, size_t size, void* scratch,
                                    size_t capacity, tlv_query_result_t* result,
                                    tlv_diagnostic_envelope_t* diagnostic) {
    tlv_codec_diagnostic_t codec;
    tlv_result_t rc = codec_decode(context, event, data, size, scratch, capacity, result,
                                   diagnostic ? &codec : NULL);
    tlv_codec_diagnostic_export(diagnostic, diagnostic ? &codec : NULL);
    return rc;
}
static const tlv_number_codec_config_t bcd_config = {TLV_NUMBER_BCD, 0, 18};
static const tlv_codec_t bcd_codec = {&bcd_config, tlv_number_decode, tlv_number_encode};
static const tlv_query_codec_adapter_t bcd_adapter = {&bcd_codec, TLV_QUERY_CODEC_UNSIGNED};
/* Imported DLL data addresses are not C static-initializer constants on Windows.
 * Form borrowed adapters at the call boundary without mutable global state. */
static tlv_result_t integer_decode(const void* context, const tlv_tree_event_t* event,
                                   const uint8_t* data, size_t size, void* scratch, size_t capacity,
                                   tlv_query_result_t* result,
                                   tlv_diagnostic_envelope_t* diagnostic) {
    (void)context;
    const tlv_query_codec_adapter_t adapter = {&tlv_codec_int64_minimal_be, TLV_QUERY_CODEC_SIGNED};
    return tlv_query_codec_decode(&adapter, event, data, size, scratch, capacity, result,
                                  diagnostic);
}
static tlv_result_t text_decode(const void* context, const tlv_tree_event_t* event,
                                const uint8_t* data, size_t size, void* scratch, size_t capacity,
                                tlv_query_result_t* result, tlv_diagnostic_envelope_t* diagnostic) {
    (void)context;
    const tlv_query_codec_adapter_t adapter = {&tlv_codec_bytes, TLV_QUERY_CODEC_UTF8};
    return tlv_query_codec_decode(&adapter, event, data, size, scratch, capacity, result,
                                  diagnostic);
}
static const tlv_query_hook_t builtin_hooks[] = {
    {1, TLV_QUERY_NUM, sizeof(int64_t), 8, NULL, integer_decode},
    {2, TLV_QUERY_BCD, sizeof(uint64_t), 8, &bcd_adapter, tlv_query_codec_decode},
    {3, TLV_QUERY_TEXT, sizeof(tlv_value_t), 8, NULL, text_decode}};
const tlv_query_hook_t* tlv_query_builtin_hooks(size_t* count) {
    if (count) *count = 3;
    return builtin_hooks;
}

#endif
