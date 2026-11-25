// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#include "adapter.h"

static tlv_result_t read_field(bench_c_state *s)
{
    return tlv_fixed_length_read(&s->input.format.length, s->input.data, s->input.available,
                                 &s->output.length, &s->output.count);
}

static tlv_result_t write_field(bench_c_state *s)
{
    return tlv_fixed_length_write(&s->input.format.length, s->input.length, s->destination,
                                  s->input.available, &s->output.count);
}

static tlv_result_t header_size(bench_c_state *s)
{
    return tlv_fixed_header_size(&s->input.format, &s->output.count);
}

static tlv_result_t element_size(bench_c_state *s)
{
    return tlv_fixed_element_size(&s->input.format, &s->input.source, &s->output.count);
}

static tlv_result_t read_element(bench_c_state *s)
{
    return tlv_read_with_diagnostic(s->input.data, s->input.available, &s->input.format,
                                    &s->output.view, &s->output.count, s->details);
}

static tlv_result_t next_element(bench_c_state *s)
{
    return tlv_reader_next_with_diagnostic(&s->reader, &s->output.view, s->details);
}

static tlv_result_t write_element(bench_c_state *s)
{
    return tlv_write_with_diagnostic(s->destination, s->input.available, &s->input.format,
                                     &s->input.source, &s->output.count, s->details);
}

static tlv_result_t append_element(bench_c_state *s)
{
    return tlv_writer_write_with_diagnostic(&s->writer, &s->input.source, s->details);
}

bench_c_step bench_select_c(bench_operation op)
{
    switch (op) {
    case BENCH_FIELD_READ: return read_field;
    case BENCH_FIELD_WRITE: return write_field;
    case BENCH_HEADER_SIZE: return header_size;
    case BENCH_ELEMENT_SIZE: return element_size;
    case BENCH_READ: return read_element;
    case BENCH_NEXT: return next_element;
    case BENCH_WRITE: return write_element;
    case BENCH_APPEND: return append_element;
    }
    return NULL;
}
