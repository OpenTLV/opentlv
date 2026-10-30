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

typedef char codec_detail_fits[(sizeof(tlv_codec_detail_t) <= TLV_DIAGNOSTIC_DETAIL_MAX) ? 1 : -1];
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
    (void)tlv_codec_detail_attach(&output->causes, &input->codec);
    size_t count = input->causes.count;
    if (count > TLV_DIAGNOSTIC_CAUSES_MAX) count = TLV_DIAGNOSTIC_CAUSES_MAX;
    for (size_t i = 0; i < count; ++i) {
        if (output->causes.count < TLV_DIAGNOSTIC_CAUSES_MAX)
            output->causes.frames[output->causes.count++] = input->causes.frames[i];
        else
            ++output->causes.omitted;
    }
    if (input->causes.omitted > SIZE_MAX - output->causes.omitted)
        output->causes.omitted = SIZE_MAX;
    else
        output->causes.omitted += input->causes.omitted;
}

tlv_result_t tlv_codec_detail_attach(tlv_diagnostic_causes_t* causes,
                                     const tlv_codec_detail_t* detail) {
    return tlv_diagnostic_causes_append(causes, &codec_domain, 1, detail, sizeof *detail);
}

tlv_result_t tlv_codec_detail_copy(const tlv_diagnostic_cause_t* cause,
                                   tlv_codec_detail_t* detail) {
    return tlv_diagnostic_cause_copy(cause, &codec_domain, 1, detail, sizeof *detail);
}

tlv_result_t tlv_codec_detail_import(const tlv_diagnostic_causes_t* causes,
                                     tlv_codec_detail_t* output) {
    if (!causes || !output) return TLV_ERR_NULL_ARG;
    if (!causes->count || causes->count > TLV_DIAGNOSTIC_CAUSES_MAX) return TLV_ERR_INVALID_ARG;
    return tlv_codec_detail_copy(&causes->frames[0], output);
}
