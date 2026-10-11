// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#include "document_internal.h"
#include "../query/program_internal.h"
#include "tlv/size.h"
#include <string.h>

tlv_result_t tlv_document_query_visit(const tlv_document_t* document, const tlv_query_t* query,
                                      tlv_document_query_visitor_t visitor, void* context,
                                      tlv_query_diagnostic_t* diagnostic) {
    tlv_query_matcher_t matcher;
    tlv_node_t* node;
    size_t depth = 0;
    tlv_result_t rc;
    query_diag_init(diagnostic);
    if (!document || !query || !visitor)
        return query_error_unlocated(diagnostic, TLV_ERR_NULL_ARG, TLV_QUERY_ERROR_ARGUMENT,
                                     "required Document path Query arguments");
    rc = tlv_query_matcher_init(&matcher, query);
    if (rc != TLV_OK)
        return query_failure(diagnostic, rc, TLV_QUERY_ERROR_ARGUMENT, "valid parsed path Query");
    node = document->first;
    while (node) {
        tlv_tag_t tag = document_node_tag(node);
        if (tlv_query_matcher_visit(&matcher, &tag, depth)) {
            document_query_callback((tlv_document_t*)document, 1);
            tlv_visit_result_t result = visitor(node, context);
            if (document_query_callback((tlv_document_t*)document, 0))
                return query_error_unlocated(diagnostic, TLV_ERR_INVALID_STATE,
                                             TLV_QUERY_ERROR_STATE,
                                             "Document kept alive during path Query callbacks");
            if (result == TLV_VISIT_STOP) return TLV_OK;
            if (result != TLV_VISIT_CONTINUE)
                return query_error_unlocated(
                    diagnostic, result == TLV_VISIT_ERROR ? TLV_ERR_VISITOR : TLV_ERR_CALLBACK,
                    TLV_QUERY_ERROR_CALLBACK, "visitor continue or stop");
        }
        if (node->first) {
            node = node->first;
            ++depth;
        } else {
            while (node->parent && !node->next) {
                node = node->parent;
                --depth;
            }
            node = node->next;
        }
    }
    return TLV_OK;
}

static tlv_visit_result_t first_path_match(tlv_node_t* node, void* context) {
    *(tlv_node_t**)context = node;
    return TLV_VISIT_STOP;
}

tlv_result_t tlv_document_find_path(const tlv_document_t* document, const tlv_query_t* query,
                                    tlv_node_t** node) {
    tlv_node_t* result = NULL;
    tlv_result_t rc;
    if (!node) return TLV_ERR_NULL_ARG;
    rc = tlv_document_query_visit(document, query, first_path_match, &result, NULL);
    if (rc != TLV_OK) return rc;
    *node = result;
    return TLV_OK;
}
