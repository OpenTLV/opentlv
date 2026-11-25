// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#include "adapter.h"

void bench_run_c(bench_operation op, const bench_input *in, uint8_t *destination, bench_output *out)
{
    tlv_diagnostic_t *details = in->diagnostic ? &out->diagnostic : NULL;
    switch (op) {
    case BENCH_FIELD_READ:
        out->result = tlv_fixed_length_read(&in->format.length, in->data, in->available,
                                            &out->length, &out->count);
        break;
    case BENCH_FIELD_WRITE:
        out->result = tlv_fixed_length_write(&in->format.length, in->length, destination,
                                             in->available, &out->count);
        break;
    case BENCH_HEADER_SIZE:
        out->result = tlv_fixed_header_size(&in->format, &out->count);
        break;
    case BENCH_ELEMENT_SIZE:
        out->result = tlv_fixed_element_size(&in->format, &in->source, &out->count);
        break;
    case BENCH_READ:
        out->result = tlv_read_with_diagnostic(in->data, in->available, &in->format,
                                               &out->view, &out->count, details);
        break;
    case BENCH_NEXT: {
        tlv_reader_t reader;
        out->result = tlv_reader_init(&reader, in->data, in->available, &in->format);
        if (out->result == TLV_OK) {
            out->result = tlv_reader_next_with_diagnostic(&reader, &out->view, details);
            out->position = reader.pos;
        }
        break;
    }
    case BENCH_WRITE:
        out->result = tlv_write_with_diagnostic(destination, in->available, &in->format,
                                                &in->source, &out->count, details);
        break;
    case BENCH_APPEND: {
        tlv_writer_t writer;
        out->result = tlv_writer_init(&writer, destination, in->available, &in->format);
        if (out->result == TLV_OK) {
            out->result = tlv_writer_write_with_diagnostic(&writer, &in->source, details);
            out->position = writer.pos;
        }
        break;
    }
    }
}
