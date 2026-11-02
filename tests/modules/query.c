// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel
#include "opentlv/query/plan.h"
#include "opentlv/query/definition.h"
#include "opentlv/core/formats/fixed.h"
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

static void* aligned_storage(uint8_t* storage, size_t capacity, size_t size, size_t alignment) {
    const size_t padding = (alignment - (uintptr_t)storage % alignment) % alignment;
    if (padding > capacity || size > capacity - padding) return NULL;
    return storage + padding;
}

static tlv_visit_result_t visit(const tlv_tree_event_t* event, void* context) {
    size_t* count = context;
    if (event->element.tag.size != 1 || event->element.tag.data[0] != 1) return TLV_VISIT_ERROR;
    ++*count;
    return TLV_VISIT_CONTINUE;
}

#if OPENTLV_QUERY_FRONTEND
static tlv_result_t compile(const char* text, const tlv_query_compile_options_t* options,
                            size_t offset, uint8_t* image, size_t capacity,
                            const tlv_query_program_t** program,
                            tlv_query_diagnostic_t*     diagnostic) {
    uint8_t      scratch[65536];
    size_t       size, alignment;
    tlv_result_t rc =
        tlv_query_compile_scratch(text, strlen(text), options, &size, &alignment, diagnostic);
    if (rc != TLV_OK) return rc;
    void* temporary = aligned_storage(scratch + offset, sizeof scratch - offset, size, alignment);
    if (!temporary) return TLV_ERR_BUFFER_TOO_SHORT;
    tlv_query_program_info_t info = {0};
    info.struct_size = sizeof info;
    rc =
        tlv_query_compile(text, strlen(text), options, temporary, size, NULL, 0, &info, diagnostic);
    if (rc != TLV_OK) return rc;
    void* output = aligned_storage(image, capacity, info.program_size, info.program_alignment);
    if (!output) return TLV_ERR_BUFFER_TOO_SHORT;
    rc = tlv_query_compile(text, strlen(text), options, temporary, size, output, info.program_size,
                           &info, diagnostic);
    if (rc == TLV_OK) *program = output;
    return rc;
}

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

static int check_query(size_t offset) {
    const uint8_t      wire[] = {1, 1, 42, 2, 1, 7};
    tlv_fixed_format_t config = {
        {1}, {1, TLV_BYTE_ORDER_BIG_ENDIAN}, TLV_ELEMENT_ORDER_TLV, TLV_LENGTH_SCOPE_VALUE};
    tlv_format_t format;
    CHECK(tlv_fixed_format_init(&format, &config) == TLV_OK);
    const tlv_query_program_t* program = NULL;
    tlv_query_diagnostic_t     diagnostic;
    CHECK(tlv_query_plan_open(&plan, plan.header.reserved, &program, &diagnostic) == TLV_OK);
    uint8_t workspace[65536];
    size_t  workspace_size, workspace_alignment;
    CHECK(tlv_query_exec_size(program, 4, &workspace_size, &workspace_alignment) == TLV_OK);
    void* storage = aligned_storage(workspace + offset, sizeof workspace - offset, workspace_size,
                                    workspace_alignment);
    CHECK(storage != NULL);
    tlv_query_exec_t* exec = NULL;
    CHECK(tlv_query_exec_init(program, storage, workspace_size, 4, 16, 1000, &exec) == TLV_OK);
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
    uint8_t image[32768];
    CHECK(compile("/01", NULL, offset, image + offset, sizeof image - offset, &program,
                  &diagnostic) == TLV_OK);
    CHECK(compile("/01[", NULL, offset, image + offset, sizeof image - offset, &program,
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
    CHECK(compile("num(x'01')", &options, offset, image + offset, sizeof image - offset, &program,
                  &diagnostic) == TLV_OK);
    CHECK(tlv_query_eval_size(program, 4, 4, &workspace_size, &workspace_alignment) == TLV_OK);
    storage = aligned_storage(workspace + offset, sizeof workspace - offset, workspace_size,
                              workspace_alignment);
    CHECK(storage != NULL);
    CHECK(tlv_query_eval_init(program, &environment, storage, workspace_size, 4, 4, 1000, &exec) ==
          TLV_OK);
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

int main(void) {
    /* Exercise every residue of the executors' 8/16-byte alignment requirements. */
    for (size_t offset = 0; offset < 16; ++offset) CHECK(check_query(offset) == 0);
    return 0;
}
