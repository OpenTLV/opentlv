// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#include "tlv/codec/diagnostic.h"
#include <string.h>

void tlv_codec_diagnostic_init(tlv_codec_diagnostic_t* diagnostic,
                               tlv_codec_operation_t operation) {
    if (!diagnostic) return;
    memset(diagnostic, 0, sizeof(*diagnostic));
    diagnostic->diagnostic.severity = TLV_DIAGNOSTIC_SEVERITY_INFO;
    diagnostic->codec.operation = operation;
}

typedef struct codec_leaf {
    tlv_codec_operation_t operation;
    int32_t reported;
    tlv_codec_violation_t violation;
    tlv_codec_reason_t reason;
    const char* representation;
} codec_leaf_t;
typedef char codec_leaf_fits[(sizeof(codec_leaf_t) <= TLV_DIAGNOSTIC_DETAIL_MAX) ? 1 : -1];
static const char* codec_detail_name(uint16_t type) {
    return type == 1 ? "codec" : "unknown";
}
static const char* codec_code_name(uint16_t family, int code) {
    switch (family) {
        case 1: return tlv_codec_operation_string(code);
        case 2: return tlv_codec_cause_string(code);
        case 3: return tlv_codec_violation_string(code);
        case 4: return tlv_codec_reason_string(code);
        default: return "unknown";
    }
}
static const tlv_diagnostic_domain_t codec_domain = {6, "opentlv.codec", codec_detail_name,
                                                     codec_code_name};
void tlv_codec_diagnostic_export(tlv_diagnostic_envelope_t* output,
                                 const tlv_codec_diagnostic_t* input) {
    if (!output) return;
    memset(output, 0, sizeof *output);
    if (!input) return;
    output->diagnostic = input->diagnostic;
    const codec_leaf_t leaf = {input->codec.operation, input->codec.reported,
                               input->codec.violation, input->codec.reason,
                               input->codec.representation};
    (void)tlv_diagnostic_causes_append(&output->causes, &codec_domain, 1, &leaf, sizeof leaf);
    if (input->codec.cause == TLV_CODEC_CAUSE_READER)
        (void)tlv_reader_detail_attach(&output->causes, &input->codec.detail.reader);
    else if (input->codec.cause == TLV_CODEC_CAUSE_SCHEMA)
        (void)tlv_schema_detail_attach(&output->causes, &input->codec.detail.schema);
}
tlv_result_t tlv_codec_detail_import(const tlv_diagnostic_causes_t* causes,
                                     tlv_codec_detail_t* output) {
    if (!causes || !output) return TLV_ERR_NULL_ARG;
    if (!causes->count || causes->count > TLV_DIAGNOSTIC_CAUSES_MAX) return TLV_ERR_INVALID_ARG;
    codec_leaf_t leaf;
    tlv_result_t rc =
        tlv_diagnostic_cause_copy(&causes->frames[0], &codec_domain, 1, &leaf, sizeof leaf);
    if (rc != TLV_OK) return rc;
    tlv_codec_detail_t detail = {0};
    detail.operation = leaf.operation;
    detail.reported = leaf.reported;
    detail.violation = leaf.violation;
    detail.reason = leaf.reason;
    detail.representation = leaf.representation;
    if (causes->count > 1) {
        if (tlv_reader_detail_copy(&causes->frames[1], &detail.detail.reader) == TLV_OK)
            detail.cause = TLV_CODEC_CAUSE_READER;
        else if (tlv_schema_detail_copy(&causes->frames[1], &detail.detail.schema) == TLV_OK)
            detail.cause = TLV_CODEC_CAUSE_SCHEMA;
    }
    *output = detail;
    return TLV_OK;
}
