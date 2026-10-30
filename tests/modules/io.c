// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#include "tlv/formats/fixed.h"
#include "tlv/reader/tree.h"
#include "tlv/writer/copy.h"
#include "tlv/writer/tree.h"
#include "tlv/diagnostic/cause.h"
#include <string.h>

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) return __LINE__;                                                         \
    } while (0)

/* Compile-time capacity proof for the largest current leaf detail. */
typedef char
    reader_detail_fits[(sizeof(tlv_reader_detail_t) <= TLV_DIAGNOSTIC_DETAIL_MAX) ? 1 : -1];

typedef struct event_source {
    tlv_element_t element;
    int           emitted;
} event_source_t;

typedef struct budget {
    size_t       total;
    tlv_result_t result;
} budget_t;

static tlv_result_t next_event(void* context, tlv_tree_event_t* event) {
    event_source_t* source = context;
    if (source->emitted) return TLV_END;
    source->emitted = 1;
    memset(event, 0, sizeof *event);
    event->kind = TLV_TREE_ELEMENT;
    event->element = source->element;
    return TLV_OK;
}

static tlv_result_t charge(void* context, size_t amount) {
    budget_t* budget = context;
    budget->total += amount;
    return budget->result;
}

int main(void) {
    const uint8_t      wire[] = {1, 1, 42};
    uint8_t            output[3] = {0};
    tlv_fixed_format_t config = {
        {1}, {1, TLV_BYTE_ORDER_BIG_ENDIAN}, TLV_ELEMENT_ORDER_TLV, TLV_LENGTH_SCOPE_VALUE};
    tlv_format_t            format;
    tlv_reader_t            reader;
    tlv_element_t           element;
    tlv_reader_diagnostic_t diagnostic;
    size_t                  written = 0;
    CHECK(tlv_fixed_format_init(&format, &config) == TLV_OK);
    CHECK(tlv_reader_init(&reader, wire, sizeof wire, &format) == TLV_OK);
    CHECK(tlv_reader_next(&reader, &element) == TLV_OK);
    CHECK(element.value.size == 1 && element.value.data[0] == 42);
    CHECK(tlv_copy_element(&element, &format, output, sizeof output, &written) == TLV_OK);
    CHECK(written == sizeof wire && memcmp(wire, output, sizeof wire) == 0);
    {
        uint8_t                     scratch[3];
        tlv_tree_writer_frame_t     frame;
        tlv_tree_writer_workspace_t workspace = {&frame,         1, output, sizeof output, scratch,
                                                 sizeof scratch, 0, 0};
        event_source_t              source = {element, 0};
        budget_t                    budget = {0, TLV_OK};
        CHECK(tlv_tree_writer_measure_events_observed(&format, next_event, &source, &workspace, 1,
                                                      1, &written, NULL, charge,
                                                      &budget) == TLV_OK);
        CHECK(budget.total == 4 && written == sizeof wire);
        CHECK(memcmp(wire, output, sizeof wire) == 0);
        source.emitted = 0;
        written = 99;
        budget.result = TLV_ERR_LIMIT;
        CHECK(tlv_tree_writer_measure_events_observed(&format, next_event, &source, &workspace, 1,
                                                      1, &written, NULL, charge,
                                                      &budget) == TLV_ERR_LIMIT);
        CHECK(written == 99);
        source.emitted = 0;
        budget.result = TLV_END;
        CHECK(tlv_tree_writer_measure_events_observed(&format, next_event, &source, &workspace, 1,
                                                      1, &written, NULL, charge,
                                                      &budget) == TLV_ERR_CALLBACK);
        CHECK(written == 99);
        source.emitted = 0;
        CHECK(tlv_tree_writer_measure_events_observed(&format, next_event, &source, &workspace, 1,
                                                      1, &written, NULL, NULL, NULL) == TLV_OK);
    }
    CHECK(tlv_reader_next(&reader, &element) == TLV_END);
    CHECK(strcmp(tlv_reader_operation_string(TLV_READER_OP_VALUE), "value") == 0);
    CHECK(strcmp(tlv_writer_operation_string(TLV_WRITER_OP_COPY), "copy") == 0);
    CHECK(tlv_reader_init(&reader, wire, sizeof wire - 1, &format) == TLV_OK);
    CHECK(tlv_reader_next_diag(&reader, &element, &diagnostic) == TLV_ERR_TRUNCATED);
    CHECK(diagnostic.diagnostic.code == TLV_ERR_TRUNCATED);
    return 0;
}
