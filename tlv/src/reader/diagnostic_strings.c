// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#include "tlv/reader/diagnostic.h"
#include "tlv/writer/writer.h"

const char* tlv_reader_operation_string(tlv_reader_operation_t value) {
    switch (value) {
        case TLV_READER_OP_TAG: return "tag";
        case TLV_READER_OP_LENGTH: return "length";
        case TLV_READER_OP_VALUE: return "value";
        case TLV_READER_OP_TRAILER: return "trailer";
        case TLV_READER_OP_HEADER: return "header";
    }
    return "unknown";
}

const char* tlv_writer_operation_string(tlv_writer_operation_t value) {
    switch (value) {
        case TLV_WRITER_OP_TAG: return "tag";
        case TLV_WRITER_OP_LENGTH: return "length";
        case TLV_WRITER_OP_VALUE: return "value";
        case TLV_WRITER_OP_HEADER: return "header";
        case TLV_WRITER_OP_TRAILER: return "trailer";
        case TLV_WRITER_OP_COPY: return "copy";
        case TLV_WRITER_OP_PRESERVE: return "preserve";
        case TLV_WRITER_OP_BEGIN: return "begin";
        case TLV_WRITER_OP_END: return "end";
    }
    return "unknown";
}

static const char* reader_detail_name(uint16_t type) {
    return type == 1 ? "reader" : "unknown";
}
static const char* reader_code_name(uint16_t family, int code) {
    return family == 1 ? tlv_reader_operation_string(code) : "unknown";
}
static const tlv_diagnostic_domain_t reader_domain = {2, "opentlv.io", reader_detail_name,
                                                      reader_code_name};
typedef char
    reader_detail_fits[(sizeof(tlv_reader_detail_t) <= TLV_DIAGNOSTIC_DETAIL_MAX) ? 1 : -1];
tlv_result_t tlv_reader_detail_attach(tlv_diagnostic_causes_t* causes,
                                      const tlv_reader_detail_t* detail) {
    return tlv_diagnostic_causes_append(causes, &reader_domain, 1, detail, sizeof *detail);
}
tlv_result_t tlv_reader_detail_copy(const tlv_diagnostic_cause_t* cause,
                                    tlv_reader_detail_t* detail) {
    return tlv_diagnostic_cause_copy(cause, &reader_domain, 1, detail, sizeof *detail);
}
