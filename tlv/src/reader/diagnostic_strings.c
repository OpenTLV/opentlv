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
