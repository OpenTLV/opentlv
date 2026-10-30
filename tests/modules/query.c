// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel
#include "tlv/query/plan.h"
#include "tlv/query/definition.h"
#include "tlv/formats/fixed.h"
#include <string.h>
#include <stdio.h>

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

static tlv_visit_result_t visit(const tlv_tree_event_t* event, void* context) {
    size_t* count = context;
    if (event->element.tag.size != 1 || event->element.tag.data[0] != 1) return TLV_VISIT_ERROR;
    ++*count;
    return TLV_VISIT_CONTINUE;
}

#if OPENTLV_QUERY_FRONTEND
static tlv_result_t external_decode(const void* context, const tlv_tree_event_t* event,
                                    const uint8_t* data, size_t size, void* scratch,
                                    size_t capacity, tlv_query_result_t* result,
                                    tlv_diagnostic_envelope_t* diagnostic) {
    (void)context;
    (void)event;
    (void)data;
    (void)size;
    (void)scratch;
    (void)capacity;
    (void)result;
    static const tlv_diagnostic_domain_t domain = {0x8000, "test.external", NULL, NULL};
    const int                            detail = 42;
    if (diagnostic) {
        tlv_diagnostic_init(&diagnostic->diagnostic, TLV_ERR_INVALID_VALUE,
                            TLV_DIAGNOSTIC_SEVERITY_ERROR);
        (void)tlv_diagnostic_causes_append(&diagnostic->causes, &domain, 1, &detail, sizeof detail);
    }
    return TLV_ERR_INVALID_VALUE;
}
#endif

int main(void) {
    const uint8_t      wire[] = {1, 1, 42, 2, 1, 7};
    tlv_fixed_format_t config = {
        {1}, {1, TLV_BYTE_ORDER_BIG_ENDIAN}, TLV_ELEMENT_ORDER_TLV, TLV_LENGTH_SCOPE_VALUE};
    tlv_format_t format;
    CHECK(tlv_fixed_format_init(&format, &config) == TLV_OK);
    const tlv_query_program_t* program = NULL;
    tlv_query_diagnostic_t     diagnostic;
    CHECK(tlv_query_plan_open(&plan, plan.header.reserved, &program, &diagnostic) == TLV_OK);
    uint64_t          workspace[8192];
    tlv_query_exec_t* exec = NULL;
    CHECK(tlv_query_exec_init(program, workspace, sizeof workspace, 4, 16, 1000, &exec) == TLV_OK);
    tlv_tree_frame_t  frames[4];
    tlv_tree_reader_t reader;
    CHECK(tlv_tree_reader_init(&reader, wire, sizeof wire, &format, frames, 4, 4, 16) == TLV_OK);
    size_t count = 0;
    CHECK(tlv_query_program_visit(&reader, exec, visit, &count, &diagnostic) == TLV_OK);
    CHECK(count == 1);

    CHECK(tlv_query_exec_reset(exec) == TLV_OK);
    CHECK(tlv_tree_reader_init(&reader, wire, sizeof wire, &format, frames, 4, 4, 16) == TLV_OK);
    tlv_tree_event_t event;
    int              matched = 0;
    count = 0;
    while (tlv_tree_reader_next_event(&reader, &event) == TLV_OK) {
        CHECK(tlv_query_exec_feed(exec, &event, &matched, &diagnostic) == TLV_OK);
        count += matched != 0;
    }
    CHECK(tlv_query_exec_finish(exec, &diagnostic) == TLV_OK);
    CHECK(count == 1);
    CHECK(tlv_config_query_frontend() == OPENTLV_QUERY_FRONTEND);
    CHECK(tlv_config_query_set_operations() == OPENTLV_QUERY_SET_OPERATIONS);
#if OPENTLV_QUERY_FRONTEND
    uint64_t                 image[4096], scratch[8192];
    tlv_query_program_info_t info = {0};
    info.struct_size = sizeof info;
    CHECK(tlv_query_compile("/01", 3, NULL, scratch, sizeof scratch, image, sizeof image, &info,
                            &diagnostic) == TLV_OK);
    CHECK(tlv_query_compile("/01[", 4, NULL, scratch, sizeof scratch, image, sizeof image, &info,
                            &diagnostic) == TLV_ERR_SYNTAX);
    CHECK(diagnostic.kind == TLV_QUERY_ERROR_SYNTAX);
    CHECK(diagnostic.diagnostic.location.domain == TLV_LOCATION_EXPRESSION);
    const tlv_query_hook_t  hook = {99, TLV_QUERY_NUM, 0, 1, NULL, external_decode};
    tlv_query_environment_t environment = {0};
    environment.hooks = &hook;
    environment.hook_count = 1;
    tlv_query_compile_options_t options;
    tlv_query_compile_options_init(&options);
    options.environment = &environment;
    CHECK(tlv_query_compile("num(x'01')", 10, &options, scratch, sizeof scratch, image,
                            sizeof image, &info, &diagnostic) == TLV_OK);
    program = (const tlv_query_program_t*)image;
    CHECK(tlv_query_eval_init(program, &environment, workspace, sizeof workspace, 4, 4, 1000,
                              &exec) == TLV_OK);
    CHECK(tlv_query_exec_finish(exec, &diagnostic) == TLV_ERR_INVALID_VALUE);
    CHECK(diagnostic.kind == TLV_QUERY_ERROR_CODEC);
    CHECK(diagnostic.reported == TLV_ERR_INVALID_VALUE);
    CHECK(diagnostic.detail.causes.count == 1);
    CHECK(strcmp(diagnostic.detail.causes.frames[0].domain->name, "test.external") == 0);
    int detail = 0;
    CHECK(tlv_diagnostic_cause_copy(&diagnostic.detail.causes.frames[0],
                                    diagnostic.detail.causes.frames[0].domain, 1, &detail,
                                    sizeof detail) == TLV_OK);
    CHECK(detail == 42);

#endif
    return 0;
}
