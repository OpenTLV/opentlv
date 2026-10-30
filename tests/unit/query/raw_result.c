// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel
#include "../../query_diagnostic_helpers.h"
#include "raw_result.h"

tlv_result_t tlv_test_raw_query(const void* context, const tlv_tree_event_t* event,
                                const uint8_t* data, size_t size, void* scratch,
                                size_t scratch_size, tlv_query_result_t* result,
                                tlv_diagnostic_envelope_t* query_envelope) {
    tlv_codec_diagnostic_t  query_codec_storage;
    tlv_codec_diagnostic_t* diagnostic = query_envelope ? &query_codec_storage : NULL;
    tlv_codec_diagnostic_init(diagnostic, TLV_CODEC_OP_DECODE);

    (void)data;
    (void)size;
    (void)scratch;
    (void)scratch_size;
    (void)event;
    (void)result;
    return query_test_export(query_envelope, diagnostic, (tlv_result_t) * (const int32_t*)context);
}
