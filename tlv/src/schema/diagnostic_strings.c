// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#include "tlv/schema/schema.h"

const char* tlv_schema_cause_string(tlv_schema_cause_t value) {
    switch (value) {
        case TLV_SCHEMA_CAUSE_NONE: return "none";
        case TLV_SCHEMA_CAUSE_SCHEMA: return "schema";
        case TLV_SCHEMA_CAUSE_READER: return "reader";
        case TLV_SCHEMA_CAUSE_CODEC: return "codec";
    }
    return "unknown";
}

const char* tlv_schema_definition_kind_string(tlv_schema_definition_kind_t value) {
    switch (value) {
        case TLV_SCHEMA_DEFINITION_UNKNOWN: return "unknown";
        case TLV_SCHEMA_DEFINITION_TABLE: return "table";
        case TLV_SCHEMA_DEFINITION_RULE: return "rule";
        case TLV_SCHEMA_DEFINITION_GROUP: return "group";
        case TLV_SCHEMA_DEFINITION_TYPE: return "type";
        case TLV_SCHEMA_DEFINITION_COMPONENT: return "component";
    }
    return "unknown";
}

const char* tlv_schema_issue_kind_string(tlv_schema_issue_kind_t kind) {
    switch (kind) {
        case TLV_SCHEMA_ISSUE_NONE: return "none";
        case TLV_SCHEMA_ISSUE_VALUE: return "value";
        case TLV_SCHEMA_ISSUE_DEFINITION: return "definition";
        case TLV_SCHEMA_ISSUE_MISSING: return "missing";
        case TLV_SCHEMA_ISSUE_DUPLICATE: return "duplicate";
        case TLV_SCHEMA_ISSUE_UNEXPECTED: return "unexpected";
        case TLV_SCHEMA_ISSUE_KIND: return "kind";
        case TLV_SCHEMA_ISSUE_LENGTH: return "length";
        case TLV_SCHEMA_ISSUE_ORDER: return "order";
        case TLV_SCHEMA_ISSUE_ASSERTION: return "assertion";
    }
    return "unknown";
}

static const char* schema_detail_name(uint16_t type) {
    return type == 1 ? "schema" : "unknown";
}
static const char* schema_code_name(uint16_t family, int code) {
    switch (family) {
        case 1: return tlv_schema_issue_kind_string(code);
        case 2: return tlv_schema_definition_kind_string(code);
        default: return "unknown";
    }
}
static const tlv_diagnostic_domain_t schema_domain = {5, "opentlv.schema", schema_detail_name,
                                                      schema_code_name};
typedef char
    schema_detail_fits[(sizeof(tlv_schema_detail_t) <= TLV_DIAGNOSTIC_DETAIL_MAX) ? 1 : -1];
tlv_result_t tlv_schema_detail_attach(tlv_diagnostic_causes_t* causes,
                                      const tlv_schema_detail_t* detail) {
    return tlv_diagnostic_causes_append(causes, &schema_domain, 1, detail, sizeof *detail);
}
tlv_result_t tlv_schema_detail_copy(const tlv_diagnostic_cause_t* cause,
                                    tlv_schema_detail_t* detail) {
    return tlv_diagnostic_cause_copy(cause, &schema_domain, 1, detail, sizeof *detail);
}
