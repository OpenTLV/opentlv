// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel
#include "tlv/schema/query.h"
#include "tlv/query/plan.h"
#include "tlv/formats/fixed.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "line %d: %s\n", __LINE__, #x);                                        \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
/* A source-free /01 plan exercises the executor with the frontend disabled. */
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
          .payload = {1}};

/* Source-free false() boolean plan. */
static const struct {
    tlv_query_plan_t        header;
    tlv_query_instruction_t nodes[1];
    char                    source[1];
} assertion_plan = {
    .header = {.magic = TLV_QUERY_PLAN_MAGIC,
               .version = TLV_QUERY_PLAN_VERSION,
               .count = 1,
               .root = 0,
               .text_offset = sizeof(tlv_query_plan_t) + sizeof(tlv_query_instruction_t),
               .reserved = sizeof(tlv_query_plan_t) + sizeof(tlv_query_instruction_t) + 1,
               .level = TLV_QUERY_S2},
    .nodes = {{.op = TLV_QUERY_OP_BOOL,
               .left = TLV_QUERY_PLAN_NONE,
               .right = TLV_QUERY_PLAN_NONE,
               .predicate_guard = TLV_QUERY_PLAN_NONE,
               .path_guard = TLV_QUERY_PLAN_NONE,
               .reuse = TLV_QUERY_PLAN_NONE,
               .type = TLV_QUERY_PLAN_BOOL,
               .folded = 0}}};
int main(void) {
    tlv_fixed_format_t config = {
        {1}, {1, TLV_BYTE_ORDER_BIG_ENDIAN}, TLV_ELEMENT_ORDER_TLV, TLV_LENGTH_SCOPE_VALUE};
    tlv_format_t format;
    CHECK(tlv_fixed_format_init(&format, &config) == TLV_OK);

    const uint8_t              wire[] = {1, 1, 42, 1, 1, 43};
    const tlv_query_program_t *selector, *assertion;
    tlv_query_diagnostic_t     query;
    CHECK(tlv_query_plan_open(&plan, plan.header.reserved, &selector, &query) == TLV_OK);
    CHECK(tlv_query_plan_open(&assertion_plan, assertion_plan.header.reserved, &assertion,
                              &query) == TLV_OK);
    tlv_schema_query_rule_t rules[2] = {{selector, assertion, NULL, "first"},
                                        {selector, assertion, NULL, "second"}};
    size_t                  a, b, alignment;
    CHECK(tlv_schema_query_size(rules, 2, 4, 8, &a, &b, &alignment) == TLV_OK);
    void *selected = malloc(a + alignment), *asserted = malloc(b + alignment);
    CHECK(selected && asserted);
    tlv_schema_query_context_t   contexts[8];
    tlv_tree_frame_t             frames[5];
    tlv_schema_query_workspace_t workspace = {
        (void*)(((uintptr_t)selected + alignment - 1) / alignment * alignment),
        a,
        (void*)(((uintptr_t)asserted + alignment - 1) / alignment * alignment),
        b,
        contexts,
        8,
        frames,
        5};
    tlv_schema_query_diagnostic_t diagnostic;
    CHECK(tlv_schema_query_validate_buffer(wire, sizeof wire, &format, rules, 2, 4, 8, 10000,
                                           &workspace, &diagnostic) == TLV_ERR_SCHEMA);
    CHECK(diagnostic.query.diagnostic.location.domain == TLV_LOCATION_INPUT);
    CHECK(diagnostic.query.diagnostic.location.begin == 0);
    CHECK(diagnostic.rule == 0 && diagnostic.query.kind == TLV_QUERY_ERROR_SCHEMA);
    CHECK(diagnostic.query.cause == TLV_QUERY_CAUSE_SCHEMA);
    CHECK(diagnostic.query.detail.causes.count == 1);
    tlv_schema_detail_t detail;
    CHECK(tlv_schema_detail_copy(&diagnostic.query.detail.causes.frames[0], &detail) == TLV_OK);
    CHECK(detail.kind == TLV_SCHEMA_ISSUE_ASSERTION && detail.field == rules[0].name);
    CHECK(detail.tag.size == 1 && detail.tag.data[0] == 1);
    free(selected);
    free(asserted);
    return 0;
}
