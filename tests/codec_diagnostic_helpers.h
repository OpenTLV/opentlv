// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel
#ifndef OPENTLV_TEST_CODEC_DIAGNOSTIC_HELPERS_H
#define OPENTLV_TEST_CODEC_DIAGNOSTIC_HELPERS_H
#include "tlv/codec/diagnostic.h"
#include "tlv/reader/diagnostic.h"
#include "tlv/schema/schema.h"
#include <string.h>
static inline tlv_codec_cause_t codec_test_cause(const tlv_codec_diagnostic_t* diagnostic) {
    if (!diagnostic->causes.count) return TLV_CODEC_CAUSE_NONE;
    tlv_reader_detail_t reader;
    if (tlv_reader_detail_copy(&diagnostic->causes.frames[0], &reader) == TLV_OK)
        return TLV_CODEC_CAUSE_READER;
    tlv_schema_detail_t schema;
    if (tlv_schema_detail_copy(&diagnostic->causes.frames[0], &schema) == TLV_OK)
        return TLV_CODEC_CAUSE_SCHEMA;
    return TLV_CODEC_CAUSE_NONE;
}
static inline tlv_reader_detail_t codec_test_reader(const tlv_codec_diagnostic_t* diagnostic) {
    tlv_reader_detail_t detail;
    memset(&detail, 0, sizeof detail);
    if (diagnostic->causes.count)
        (void)tlv_reader_detail_copy(&diagnostic->causes.frames[0], &detail);
    return detail;
}
static inline tlv_schema_detail_t codec_test_schema(const tlv_codec_diagnostic_t* diagnostic) {
    tlv_schema_detail_t detail;
    memset(&detail, 0, sizeof detail);
    if (diagnostic->causes.count)
        (void)tlv_schema_detail_copy(&diagnostic->causes.frames[0], &detail);
    return detail;
}
static inline tlv_codec_detail_t schema_test_codec(const tlv_schema_diagnostic_t* diagnostic) {
    tlv_codec_detail_t result;
    memset(&result, 0, sizeof result);
    if (diagnostic->cause == TLV_SCHEMA_CAUSE_CODEC)
        (void)tlv_codec_detail_import(&diagnostic->detail.causes, &result);
    return result;
}
#endif
