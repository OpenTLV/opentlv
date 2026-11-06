// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

/* Symbol isolation regression (#763). The consumer defines every name that the
 * Library once exported outside the reserved tlv_ namespace, then runs the
 * operations that reached them. A remaining collision fails to link against
 * the amalgamation object or the static archives, or, through ELF
 * interposition, calls these definitions from the shared libraries and aborts.
 * Compiled with OPENTLV_COLLISION_AMALGAMATION beside opentlv.c, otherwise
 * against the conventional modules. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef OPENTLV_COLLISION_AMALGAMATION
#include "opentlv.h"
#else
#include "opentlv/builtin/emv/emv.h"
#include "opentlv/codec/codec.h"
#include "opentlv/core/formats/fixed.h"
#include "opentlv/document/document.h"
#include "opentlv/query/document.h"
#include "opentlv/query/plan.h"
#endif

#define COLLISION_NAMES(X)                                                                         \
    X(document_create_node)                                                                        \
    X(document_memory_allocate)                                                                    \
    X(document_memory_release)                                                                     \
    X(document_node_location)                                                                      \
    X(document_node_tag)                                                                           \
    X(document_parse_list)                                                                         \
    X(document_set_offset)                                                                         \
    X(emv_base_fields)                                                                             \
    X(emv_value_decode)                                                                            \
    X(emv_value_encode)                                                                            \
    X(query_candidate_alignment)                                                                   \
    X(query_candidate_size)                                                                        \
    X(query_document_end)                                                                          \
    X(query_document_handle)                                                                       \
    X(query_document_next)                                                                         \
    X(query_document_result_count)                                                                 \
    X(query_function_eval)                                                                         \
    X(query_live_overlap)                                                                          \
    X(query_retained_feed)                                                                         \
    X(query_retained_finish)                                                                       \
    X(query_retained_overlap)                                                                      \
    X(query_s1_filter)

/* Incompatible definitions: reaching any of them is a failure. */
#define DECLARE(name) void name(void);
#define DEFINE(name)                                                                               \
    void name(void) {                                                                              \
        fputs("interposed " #name "\n", stderr);                                                   \
        abort();                                                                                   \
    }
COLLISION_NAMES(DECLARE)
COLLISION_NAMES(DEFINE)
#undef DECLARE
#undef DEFINE

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "line %d: %s\n", __LINE__, #condition);                                \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

/* A source-free /80 plan runs the executor with or without the Query frontend. */
#define NONE TLV_QUERY_PLAN_NONE
static const struct {
    tlv_query_plan_t        header;
    tlv_query_instruction_t nodes[3];
    char                    source[1];
    uint8_t                 payload[1];
} plan = {{.magic = TLV_QUERY_PLAN_MAGIC,
           .version = TLV_QUERY_PLAN_VERSION,
           .count = 3,
           .root = 2,
           .text_offset = sizeof(tlv_query_plan_t) + 3 * sizeof(tlv_query_instruction_t),
           .reserved = sizeof(tlv_query_plan_t) + 3 * sizeof(tlv_query_instruction_t) + 2,
           .payload_size = 1,
           .level = TLV_QUERY_S0},
          {{.op = TLV_QUERY_OP_ROOT,
            .left = NONE,
            .right = NONE,
            .predicate_guard = NONE,
            .path_guard = NONE,
            .reuse = NONE},
           {.op = TLV_QUERY_OP_TEST,
            .left = NONE,
            .right = NONE,
            .low = 1,
            .predicate_guard = NONE,
            .path_guard = 0,
            .path_kind = TLV_QUERY_OP_CHILD,
            .reuse = NONE,
            .resolved = 1,
            .data_size = 1},
           {.op = TLV_QUERY_OP_CHILD,
            .left = 0,
            .right = 1,
            .predicate_guard = NONE,
            .path_guard = NONE,
            .reuse = NONE}},
          {0},
          {0x80}};
#undef NONE

static int constructed(const void* context, const tlv_tag_t* tag) {
    (void)context;
    return tag->size == 1 && tag->data[0] == 0x80;
}

/* Document parse, Source locations, encoding and Query evaluation on Document. */
static int document_query(void) {
    static const uint8_t     wire[] = {0x80, 3, 1, 1, 42, 2, 1, 7};
    const tlv_fixed_format_t config = {
        {1}, {1, TLV_BYTE_ORDER_BIG_ENDIAN}, TLV_ELEMENT_ORDER_TLV, TLV_LENGTH_SCOPE_VALUE};
    uint8_t                    output[sizeof wire];
    tlv_format_t               format;
    tlv_document_options_t     options;
    tlv_document_t*            document = NULL;
    tlv_reader_diagnostic_t    diagnostic;
    tlv_query_diagnostic_t     query_diagnostic;
    const tlv_query_program_t* program = NULL;
    tlv_query_exec_t*          exec = NULL;
    tlv_node_t*                selected = NULL;
    static uint64_t            workspace[8192];
    size_t                     size = 0, alignment = 0, padding, written = 0;

    CHECK(tlv_fixed_format_init(&format, &config) == TLV_OK);
    format.is_constructed = constructed;
    CHECK(tlv_document_options_init(&options, &format) == TLV_OK);
    options.retain_source_locations = 1;
    CHECK(tlv_document_parse(wire, sizeof wire, &options, &document, &diagnostic) == TLV_OK);
    CHECK(tlv_document_count(document) == 3);
    CHECK(tlv_node_source_location(tlv_node_first_child(tlv_document_first(document))).offset == 2);
    CHECK(tlv_document_encode(document, output, sizeof output, &written) == TLV_OK);
    CHECK(written == sizeof wire && memcmp(output, wire, sizeof wire) == 0);

    CHECK(tlv_query_plan_open(&plan, plan.header.reserved, &program, &query_diagnostic) == TLV_OK);
    CHECK(tlv_query_eval_size(program, 4, 16, &size, &alignment) == TLV_OK);
    padding = (alignment - (uintptr_t)workspace % alignment) % alignment;
    CHECK(padding <= sizeof workspace && size <= sizeof workspace - padding);
    CHECK(tlv_query_eval_init(program, NULL, (uint8_t*)workspace + padding, size, 4, 16, 1000,
                              &exec) == TLV_OK);
    CHECK(tlv_document_query_evaluate(document, exec, NULL, NULL, 0, NULL, &query_diagnostic) ==
          TLV_OK);
    CHECK(tlv_document_query_next(exec, &selected, &query_diagnostic) == TLV_OK);
    CHECK(selected == tlv_document_first(document));
    CHECK(tlv_document_query_next(exec, &selected, &query_diagnostic) == TLV_END);
    tlv_document_free(document);

    /* A truncated input reports the parse failure offset. */
    CHECK(tlv_document_parse(wire, sizeof wire - 1, &options, &document, &diagnostic) != TLV_OK);
    CHECK(document == NULL && diagnostic.diagnostic.code != TLV_OK);
    return 0;
}

/* EMV dictionary lookup and the shared EMV value codec. */
static int emv_codec(void) {
    static const uint8_t        wire[] = {0x31, 0x12, 0x25};
    const tlv_emv_definition_t* entry =
        tlv_emv_find(TLV_EMV_CONTEXT_BASE, &tlv_emv_tag_application_expiration_date);
    tlv_emv_date_t date;
    uint8_t        output[sizeof wire];
    size_t         written = 0;

    CHECK(entry && entry->codec && tlv_emv_symbol(entry));
    CHECK(tlv_codec_decode(entry->codec, wire, sizeof wire, &date, sizeof date, NULL) == TLV_OK);
    CHECK(date.year == 31 && date.month == 12 && date.day == 25);
    CHECK(tlv_codec_encode(entry->codec, &date, sizeof date, output, sizeof output, &written,
                           NULL) == TLV_OK);
    CHECK(written == sizeof wire && memcmp(output, wire, sizeof wire) == 0);
    return 0;
}

int main(void) {
    if (document_query() || emv_codec()) return 1;
    puts("PASS symbol isolation");
    return 0;
}
