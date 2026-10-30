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
