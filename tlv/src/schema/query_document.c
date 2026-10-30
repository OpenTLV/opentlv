// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel
#include "tlv/schema/query_document.h"
#include "tlv/query/document.h"
#include "query_internal.h"

tlv_result_t tlv_schema_query_validate_document(const tlv_document_t* document,
                                                const tlv_schema_query_rule_t* rules, size_t count,
                                                size_t depth, size_t nodes, size_t work,
                                                tlv_schema_query_workspace_t* w, uint8_t* values,
                                                size_t capacity,
                                                tlv_tree_writer_workspace_t* staging,
                                                tlv_schema_query_diagnostic_t* diagnostic) {
    if (!count) return TLV_OK;
    if (!document || !w || (!w->contexts && w->context_capacity)) return TLV_ERR_NULL_ARG;
    size_t a, b, alignment;
    tlv_result_t rc =
        schema_query_requirements(rules, count, depth, nodes, &a, &b, &alignment, diagnostic);
    if (rc != TLV_OK) return rc;
    if (w->selector_size < a || w->assertion_size < b) return TLV_ERR_BUFFER_TOO_SHORT;
    if (diagnostic) memset(diagnostic, 0, sizeof *diagnostic);
    for (size_t i = 0; i < count; ++i) {
        if (diagnostic) diagnostic->rule = i;
        tlv_query_diagnostic_t* detail = diagnostic ? &diagnostic->query : NULL;
        tlv_query_exec_t* exec;
        rc = tlv_query_eval_init(rules[i].context, rules[i].environment, w->selector,
                                 w->selector_size, depth, nodes, work, &exec);
        if (rc != TLV_OK)
            return query_failure(detail, rc, TLV_QUERY_ERROR_EVENTS,
                                 "valid Schema Query execution");
        rc = tlv_document_query_evaluate(document, exec, NULL, values, capacity, staging, detail);
        if (rc != TLV_OK)
            return query_failure(detail, rc, TLV_QUERY_ERROR_EVENTS,
                                 "valid Schema Query execution");
        size_t contexts = 0;
        for (;;) {
            tlv_node_t* node;
            rc = tlv_document_query_next(exec, &node, detail);
            if (rc == TLV_END) break;
            if (rc != TLV_OK)
                return query_failure(detail, rc, TLV_QUERY_ERROR_EVENTS,
                                     "valid Schema Query execution");
            if (contexts == w->context_capacity)
                return query_limit(detail, "schema-contexts", w->context_capacity, 0, 0);
            w->contexts[contexts++].node = node;
        }
        for (size_t j = 0; j < contexts; ++j) {
            rc = tlv_query_eval_init(rules[i].assertion, rules[i].environment, w->assertion,
                                     w->assertion_size, depth, nodes, work, &exec);
            if (rc != TLV_OK)
                return query_failure(detail, rc, TLV_QUERY_ERROR_EVENTS,
                                     "valid Schema Query execution");
            rc = tlv_document_query_evaluate(document, exec, w->contexts[j].node, values, capacity,
                                             staging, detail);
            if (rc != TLV_OK)
                return query_failure(detail, rc, TLV_QUERY_ERROR_EVENTS,
                                     "valid Schema Query execution");
            tlv_query_result_t result;
            rc = tlv_query_exec_result(exec, &result);
            if (rc != TLV_OK)
                return query_failure(detail, rc, TLV_QUERY_ERROR_EVENTS,
                                     "valid Schema Query execution");
            if (!result.boolean) {
                if (diagnostic) {
                    assertion_failure(&diagnostic->query, tlv_node_tag(w->contexts[j].node),
                                      rules[i].name);
                    /* Count the complete ancestry, then fill the retained root
                     * prefix in reverse as parent links walk inward to outward. */
                    tlv_diagnostic_path_t* path = &diagnostic->query.diagnostic.path;
                    size_t parents = 0;
                    for (const tlv_node_t* p = tlv_node_parent(w->contexts[j].node); p;
                         p = tlv_node_parent(p))
                        ++parents;
                    path->length =
                        parents < TLV_DIAGNOSTIC_PATH_MAX ? parents : TLV_DIAGNOSTIC_PATH_MAX;
                    path->omitted = parents - path->length;
                    for (const tlv_node_t* p = tlv_node_parent(w->contexts[j].node); p;
                         p = tlv_node_parent(p)) {
                        if (--parents < path->length) path->tags[parents] = tlv_node_tag(p);
                    }
                }
                /* diagnostic-return: query.diagnostic.code is set for the failed assertion above.
                 */
                return TLV_ERR_SCHEMA;
            }
        }
    }
    return TLV_OK;
}
