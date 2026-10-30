// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel
#ifndef OPENTLV_DOCUMENT_INTERNAL_H
#define OPENTLV_DOCUMENT_INTERNAL_H
#include "tlv/document/document.h"
#include "tlv/reader/diagnostic.h"
/* Optional source location, then tag bytes, follow in the same allocation. */
struct tlv_node {
    uint64_t identity;
    tlv_document_t* document;
    tlv_node_t* parent;
    tlv_node_t* prev;
    tlv_node_t* next;
    tlv_node_t* first;
    tlv_node_t* last;
    uint8_t* value;
    size_t value_size;
    size_t tag_size;
    int constructed;
    int pending_erase;
};

struct tlv_document {
    tlv_document_options_t options;
    tlv_allocator_t allocator;
    tlv_node_t* first;
    tlv_node_t* last;
    size_t count;
    uint64_t revision;
    uint64_t retire_epoch;
    size_t query_callbacks;
    int query_pending;
    uint64_t next_identity;
};

void* document_memory_allocate(const tlv_document_t* document, size_t size);
void document_memory_release(const tlv_document_t* document, void* memory);
tlv_tag_t document_node_tag(const tlv_node_t* node);
tlv_document_source_location_t* document_node_location(tlv_node_t* node);
void document_set_offset(tlv_reader_diagnostic_t* out, size_t offset);
tlv_result_t document_create_node(tlv_document_t* document, tlv_node_t* parent, tlv_tag_t tag,
                                  const uint8_t* value, size_t length, int constructed,
                                  size_t depth, size_t element_offset,
                                  tlv_reader_diagnostic_t* diagnostic, tlv_node_t** created);
tlv_result_t document_parse_list(tlv_document_t* document, tlv_node_t* parent, const uint8_t* data,
                                 size_t size, size_t depth, size_t base,
                                 tlv_reader_diagnostic_t* diagnostic);

/* Metadata bridge for optional consumers; topology remains public node navigation. */
const tlv_format_t* document_format(const tlv_document_t* document);
int document_contains(const tlv_document_t* document, const tlv_node_t* node);
/* Exit returns 0 unchanged, 1 deferred erase, 2 deferred destruction. */
int document_query_callback(tlv_document_t* document, int active);

#if OPENTLV_QUERY
/* Detail of a failed Query edit, zero-initialized by the caller. `reader` is the
 * Reader detail of a rejected constructed Value, valid only when its code equals the
 * returned result. `limit` names an exceeded Document limit, otherwise NULL. */
typedef struct document_edit_failure {
    tlv_reader_diagnostic_t reader;
    const char* limit;
    size_t configured;
} document_edit_failure_t;
tlv_result_t document_edit_targets(tlv_document_t* document, tlv_node_t** targets, size_t count,
                                   tlv_document_query_edit_kind_t kind, tlv_tag_t tag,
                                   const uint8_t* value, size_t size, size_t* applied,
                                   document_edit_failure_t* failure);
#endif

#endif
