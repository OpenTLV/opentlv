// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#include "tlv/query/program.h"

const char* tlv_query_error_kind_string(tlv_query_error_kind_t value) {
    switch (value) {
        case TLV_QUERY_ERROR_NONE: return "none";
        case TLV_QUERY_ERROR_SYNTAX: return "syntax";
        case TLV_QUERY_ERROR_CAPABILITY: return "capability";
        case TLV_QUERY_ERROR_LIMIT: return "limit";
        case TLV_QUERY_ERROR_STORAGE: return "storage";
        case TLV_QUERY_ERROR_EVENTS: return "events";
        case TLV_QUERY_ERROR_SOURCE: return "source";
        case TLV_QUERY_ERROR_READER: return "reader";
        case TLV_QUERY_ERROR_BINDING: return "binding";
        case TLV_QUERY_ERROR_CARDINALITY: return "cardinality";
        case TLV_QUERY_ERROR_CODEC: return "codec";
        case TLV_QUERY_ERROR_IMAGE_VERSION: return "image_version";
        case TLV_QUERY_ERROR_STATE: return "state";
        case TLV_QUERY_ERROR_CALLBACK: return "callback";
        case TLV_QUERY_ERROR_TYPE: return "type";
        case TLV_QUERY_ERROR_IMAGE: return "image";
        case TLV_QUERY_ERROR_SCHEMA: return "schema";
        case TLV_QUERY_ERROR_ARGUMENT: return "argument";
    }
    return "unknown";
}

const char* tlv_query_cause_string(tlv_query_cause_t value) {
    switch (value) {
        case TLV_QUERY_CAUSE_NONE: return "none";
        case TLV_QUERY_CAUSE_READER: return "reader";
        case TLV_QUERY_CAUSE_CODEC: return "codec";
        case TLV_QUERY_CAUSE_SCHEMA: return "schema";
    }
    return "unknown";
}

const char* tlv_query_conversion_violation_string(tlv_query_conversion_violation_t value) {
    switch (value) {
        case TLV_QUERY_CONVERSION_NONE: return "none";
        case TLV_QUERY_CONVERSION_RESULT: return "result";
        case TLV_QUERY_CONVERSION_TYPE: return "type";
        case TLV_QUERY_CONVERSION_UTF8: return "utf8";
    }
    return "unknown";
}
