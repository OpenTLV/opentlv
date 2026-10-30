// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel
#ifndef OPENTLV_TEST_QUERY_DIAGNOSTIC_HELPERS_H
#define OPENTLV_TEST_QUERY_DIAGNOSTIC_HELPERS_H
#include "tlv/query/program.h"
#include "tlv/codec/diagnostic.h"
#include "tlv/schema/detail.h"
#include <string.h>
static inline tlv_codec_detail_t query_test_codec(tlv_query_diagnostic_t diagnostic) {
    tlv_codec_detail_t detail;
    memset(&detail, 0, sizeof detail);
    (void)tlv_codec_detail_import(&diagnostic.detail.causes, &detail);
    return detail;
}
static inline tlv_reader_detail_t query_test_codec_reader(tlv_query_diagnostic_t diagnostic) {
    tlv_reader_detail_t detail;
    memset(&detail, 0, sizeof detail);
    if (diagnostic.detail.causes.count > 1)
        (void)tlv_reader_detail_copy(&diagnostic.detail.causes.frames[1], &detail);
    return detail;
}
static inline tlv_schema_detail_t query_test_schema(tlv_query_diagnostic_t diagnostic) {
    tlv_schema_detail_t detail;
    memset(&detail, 0, sizeof detail);
    if (diagnostic.detail.causes.count)
        (void)tlv_schema_detail_copy(&diagnostic.detail.causes.frames[0], &detail);
    return detail;
}
static inline tlv_result_t query_test_export(tlv_diagnostic_envelope_t* output,
                                             tlv_codec_diagnostic_t* codec, tlv_result_t result) {
    tlv_codec_diagnostic_export(output, codec);
    return result;
}
#endif
