// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#include "tlv/codec/diagnostic.h"

const char* tlv_codec_operation_string(tlv_codec_operation_t value) {
    switch (value) {
        case TLV_CODEC_OP_DECODE: return "decode";
        case TLV_CODEC_OP_ENCODE: return "encode";
        case TLV_CODEC_OP_MEASURE: return "measure";
    }
    return "unknown";
}

const char* tlv_codec_cause_string(tlv_codec_cause_t value) {
    switch (value) {
        case TLV_CODEC_CAUSE_NONE: return "none";
        case TLV_CODEC_CAUSE_READER: return "reader";
        case TLV_CODEC_CAUSE_SCHEMA: return "schema";
    }
    return "unknown";
}

const char* tlv_codec_violation_string(tlv_codec_violation_t value) {
    switch (value) {
        case TLV_CODEC_VIOLATION_NONE: return "none";
        case TLV_CODEC_VIOLATION_RESULT: return "result";
        case TLV_CODEC_VIOLATION_SIZE: return "size";
        case TLV_CODEC_VIOLATION_TYPE: return "type";
        case TLV_CODEC_VIOLATION_UTF8: return "utf8";
    }
    return "unknown";
}

const char* tlv_codec_reason_string(tlv_codec_reason_t value) {
    switch (value) {
        case TLV_CODEC_REASON_NONE: return "none";
        case TLV_CODEC_REASON_LENGTH: return "length";
        case TLV_CODEC_REASON_DIGIT: return "digit";
        case TLV_CODEC_REASON_TEXT: return "text";
        case TLV_CODEC_REASON_RANGE: return "range";
        case TLV_CODEC_REASON_CANONICAL: return "canonical";
        case TLV_CODEC_REASON_SYNTAX: return "syntax";
        case TLV_CODEC_REASON_OBJECT: return "object";
    }
    return "unknown";
}
