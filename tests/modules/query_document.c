// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#include "tlv/document/integration.h"
#include "tlv/query/document.h"
#include "tlv/query/plan.h"
#include "tlv/formats/fixed.h"
#include <stdlib.h>
#include <string.h>

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) return __LINE__;                                                         \
    } while (0)

/* A source-free /80 plan exercises the executor with the frontend disabled. */
static const struct {
    tlv_query_plan_t        header;
    tlv_query_instruction_t nodes[3];
    char                    source[1];
    uint8_t                 payload[1];
} plan = {.header = {.magic = TLV_QUERY_PLAN_MAGIC,
                     .version = TLV_QUERY_PLAN_VERSION,
                     .count = 3,
                     .root = 2,
                     .text_offset = sizeof(tlv_query_plan_t) + 3 * sizeof(tlv_query_instruction_t),
                     .reserved = sizeof(tlv_query_plan_t) + 3 * sizeof(tlv_query_instruction_t) + 2,
                     .payload_size = 1,
                     .level = TLV_QUERY_S0},
#define N TLV_QUERY_PLAN_NONE
          .nodes = {{.op = TLV_QUERY_OP_ROOT,
                     .left = N,
                     .right = N,
                     .predicate_guard = N,
                     .path_guard = N,
                     .reuse = N},
                    {.op = TLV_QUERY_OP_TEST,
                     .left = N,
                     .right = N,
                     .low = 1,
                     .predicate_guard = N,
                     .path_guard = 0,
                     .path_kind = TLV_QUERY_OP_CHILD,
                     .reuse = N,
                     .resolved = 1,
                     .data_size = 1},
                    {.op = TLV_QUERY_OP_CHILD,
                     .left = 0,
                     .right = 1,
                     .predicate_guard = N,
                     .path_guard = N,
                     .reuse = N}},
#undef N
          .payload = {0x80}};

typedef struct memory {
    size_t live;
    int    fail;
} memory_t;

static void* allocate(void* context, size_t size) {
    memory_t* memory = context;
    void*     pointer = memory->fail ? NULL : malloc(size);
    if (pointer) ++memory->live;
    return pointer;
}
static void release(void* context, void* pointer) {
    memory_t* memory = context;
    --memory->live;
    free(pointer);
}
static int constructed(const void* context, const tlv_tag_t* tag) {
    (void)context;
    return tag->size == 1 && tag->data[0] == 0x80;
}

int main(void) {
    const uint8_t      wire[] = {0x80, 3, 1, 1, 42, 2, 1, 7};
    uint8_t            output[sizeof wire];
    const uint8_t      replacement = 43;
    tlv_fixed_format_t config = {
        {1}, {1, TLV_BYTE_ORDER_BIG_ENDIAN}, TLV_ELEMENT_ORDER_TLV, TLV_LENGTH_SCOPE_VALUE};
    tlv_format_t            format;
    memory_t                memory = {0, 0};
    tlv_allocator_t         allocator = {&memory, allocate, release};
    tlv_document_options_t  options;
    tlv_document_t*         document = NULL;
    tlv_reader_diagnostic_t diagnostic;
    size_t                  written = 0;
    CHECK(tlv_fixed_format_init(&format, &config) == TLV_OK);
    format.is_constructed = constructed;
    CHECK(tlv_document_options_init(&options, &format) == TLV_OK);
    options.allocator = &allocator;
    options.retain_source_locations = 1;
    CHECK(tlv_document_parse(wire, sizeof wire, &options, &document, &diagnostic) == TLV_OK);
    CHECK(tlv_document_count(document) == 3);
    CHECK(tlv_document_options(document).format == &format);
    CHECK(tlv_document_options(document).allocator->context == &memory);
    CHECK(tlv_document_options(document).max_depth == options.max_depth);
    CHECK(tlv_document_encode(document, output, sizeof output, &written) == TLV_OK);
    CHECK(written == sizeof wire && memcmp(output, wire, sizeof wire) == 0);
    tlv_node_t*                root = tlv_document_first(document);
    const tlv_query_program_t* program = NULL;
    tlv_query_diagnostic_t     query_diagnostic;
    uint64_t                   workspace[8192];
    tlv_query_exec_t*          exec = NULL;
    CHECK(tlv_query_plan_open(&plan, plan.header.reserved, &program, &query_diagnostic) == TLV_OK);
    CHECK(tlv_query_eval_init(program, NULL, workspace, sizeof workspace, 4, 16, 1000, &exec) ==
          TLV_OK);
    CHECK(tlv_document_query_evaluate(document, exec, NULL, NULL, 0, NULL, &query_diagnostic) ==
          TLV_OK);
    tlv_node_t* selected = NULL;
    CHECK(tlv_document_query_next(exec, &selected, &query_diagnostic) == TLV_OK);
    CHECK(selected == root);
    CHECK(tlv_document_query_next(exec, &selected, &query_diagnostic) == TLV_END);

#if OPENTLV_QUERY_FRONTEND
    tlv_query_t query;
    tlv_node_t* found = NULL;
    CHECK(tlv_query_parse("80/01", &query, NULL) == TLV_OK);
    CHECK(tlv_document_find_path(document, &query, &found) == TLV_OK);
    CHECK(found == tlv_node_first_child(root));
#endif

    tlv_node_t* child = tlv_node_first_child(root);
    tlv_node_t* sibling = tlv_node_next(root);
    uint64_t    identity = tlv_node_identity(child);
    uint64_t    revision = tlv_document_revision(document);
    CHECK(identity != 0 && tlv_document_node_identity(document, child) == identity);
    CHECK(tlv_document_contains(document, child));
    CHECK(tlv_node_source_location(child).has_offset);
    CHECK(tlv_node_source_location(child).offset == 2);
    CHECK(tlv_node_source_location(child).header_size == 2);
    memory.fail = 1;
    CHECK(tlv_node_set_value(child, &replacement, 1) == TLV_ERR_OUT_OF_MEMORY);
    CHECK(tlv_document_revision(document) == revision);
    CHECK(tlv_node_source_location(child).has_offset);
    memory.fail = 0;
    CHECK(tlv_node_set_value(child, &replacement, 1) == TLV_OK);
    CHECK(tlv_document_revision(document) == revision + 1);
    CHECK(tlv_node_identity(child) == identity);
    CHECK(!tlv_node_source_location(child).has_offset);
    CHECK(!tlv_node_source_location(root).has_offset);
    CHECK(tlv_node_source_location(sibling).has_offset);
    CHECK(tlv_document_encode(document, output, sizeof output, &written) == TLV_OK);
    CHECK(output[4] == replacement && memcmp(output, wire, 4) == 0);
    CHECK(memcmp(output + 5, wire + 5, 3) == 0);

    CHECK(tlv_document_callback(document, 1) == 0);
    CHECK(tlv_document_callback(document, 1) == 0);
    CHECK(tlv_document_callbacks_active(document));
    CHECK(tlv_node_set_value(child, wire, 1) == TLV_ERR_INVALID_STATE);
    CHECK(tlv_document_insert(document, NULL, NULL, tlv_tag(wire, 1), NULL, 0, NULL) ==
          TLV_ERR_INVALID_STATE);
    tlv_node_erase(child);
    tlv_node_erase(root);
    CHECK(tlv_document_count(document) == 3);
    CHECK(tlv_node_value_data(child)[0] == replacement);
    CHECK(tlv_document_callback(document, 0) == 0);
    CHECK(tlv_document_callbacks_active(document));
    CHECK(tlv_document_callback(document, 0) == 1);
    CHECK(!tlv_document_callbacks_active(document));
    CHECK(tlv_document_count(document) == 1);
    CHECK(tlv_document_retire_epoch(document) == 1);
    CHECK(tlv_document_revision(document) == revision + 2);
    CHECK(tlv_document_first(document) == sibling);
    CHECK(tlv_document_callback(document, 1) == 0);
    tlv_document_free(document);
    CHECK(tlv_node_value_data(sibling)[0] == 7);
    CHECK(tlv_document_callback(document, 0) == 2);
    CHECK(memory.live == 0);

    CHECK(tlv_document_parse(wire, sizeof wire - 1, &options, &document, &diagnostic) != TLV_OK);
    CHECK(document == NULL && memory.live == 0);
    CHECK(diagnostic.diagnostic.code != TLV_OK);
    CHECK(tlv_document_options(NULL).format == NULL);
    CHECK(!tlv_document_contains(NULL, NULL));
    CHECK(!tlv_document_callbacks_active(NULL));
    return 0;
}
