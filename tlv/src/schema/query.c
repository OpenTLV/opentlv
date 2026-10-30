// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel
#include "tlv/schema/query.h"
#include "tlv/query/plan.h"
#include "query_internal.h"
#include <string.h>

tlv_result_t tlv_schema_query_size(const tlv_schema_query_rule_t* rules, size_t count, size_t depth,
                                   size_t nodes, size_t* selector, size_t* assertion,
                                   size_t* alignment) {
    return schema_query_requirements(rules, count, depth, nodes, selector, assertion, alignment,
                                     NULL);
}
static tlv_result_t buffer_evaluate(const uint8_t* data, size_t size, const tlv_format_t* format,
                                    tlv_query_exec_t* exec, size_t depth, size_t nodes,
                                    tlv_schema_query_workspace_t* workspace,
                                    tlv_query_diagnostic_t* diagnostic) {
    tlv_tree_reader_t reader;
    tlv_result_t rc = tlv_tree_reader_init(&reader, data, size, format, workspace->frames,
                                           workspace->frame_capacity, depth, nodes);
    if (rc != TLV_OK) {
        /* Initialization rejects workspace or Format configuration, not input, so there
         * is no Reader cause to report. */
        query_diag_init(diagnostic);
        return query_error_unlocated(diagnostic, rc,
                                     query_result_kind(rc, TLV_QUERY_ERROR_ARGUMENT),
                                     "readable Format and Reader frame workspace");
    }
    tlv_reader_diagnostic_t original;
    for (;;) {
        tlv_tree_event_t event;
        /* Tree preflight failures leave detail untouched. Reset only the marker;
         * materialize missing detail on failure, never on the successful event path. */
        if (diagnostic) original.diagnostic.code = TLV_OK;
        rc = tlv_tree_reader_next_event_diag(&reader, &event, diagnostic ? &original : NULL);
        if (rc == TLV_END) return tlv_query_exec_finish(exec, diagnostic);
        if (rc != TLV_OK) {
            if (diagnostic) {
                if (original.diagnostic.code == TLV_OK) {
                    tlv_reader_diagnostic_init(&original);
                    tlv_diagnostic_init(&original.diagnostic, rc, TLV_DIAGNOSTIC_SEVERITY_ERROR);
                }
                query_diag_init(diagnostic);
                query_reader_failure(diagnostic, rc, &original);
            }
            return rc;
        }
        int matched;
        rc = tlv_query_exec_feed(exec, &event, &matched, diagnostic);
        if (rc != TLV_OK) return rc;
    }
}

static void context_path(const uint8_t* data, size_t size, const tlv_format_t* format, size_t depth,
                         size_t nodes, tlv_schema_query_workspace_t* workspace, size_t wanted,
                         tlv_diagnostic_path_t* path) {
    tlv_tree_reader_t reader;
    if (tlv_tree_reader_init(&reader, data, size, format, workspace->frames,
                             workspace->frame_capacity, depth, nodes) != TLV_OK)
        return;
    tlv_tag_t ancestors[TLV_DIAGNOSTIC_PATH_MAX];
    size_t ordinal = 0;
    for (;;) {
        tlv_tree_event_t event;
        if (tlv_tree_reader_next_event(&reader, &event) != TLV_OK) return;
        if (event.kind == TLV_TREE_END) continue;
        if (ordinal++ == wanted) {
            size_t count =
                event.depth < TLV_DIAGNOSTIC_PATH_MAX ? event.depth : TLV_DIAGNOSTIC_PATH_MAX;
            memcpy(path->tags, ancestors, count * sizeof(*ancestors));
            path->length = count;
            path->omitted = event.depth - count;
            return;
        }
        if (event.depth < TLV_DIAGNOSTIC_PATH_MAX) ancestors[event.depth] = event.element.tag;
    }
}
tlv_result_t tlv_schema_query_validate_buffer(const uint8_t* data, size_t size,
                                              const tlv_format_t* format,
                                              const tlv_schema_query_rule_t* rules, size_t count,
                                              size_t depth, size_t nodes, size_t work,
                                              tlv_schema_query_workspace_t* w,
                                              tlv_schema_query_diagnostic_t* diagnostic) {
    if (!count) return TLV_OK;
    if ((!data && size) || !format || !w || (!w->contexts && w->context_capacity))
        return TLV_ERR_NULL_ARG;
    size_t selector_size, assertion_size, alignment;
    tlv_result_t rc = schema_query_requirements(rules, count, depth, nodes, &selector_size,
                                                &assertion_size, &alignment, diagnostic);
    if (rc != TLV_OK) return rc;
    if (!w->selector || !w->assertion || (uintptr_t)w->selector % alignment ||
        (uintptr_t)w->assertion % alignment)
        return TLV_ERR_INVALID_ARG;
    if (w->selector_size < selector_size || w->assertion_size < assertion_size)
        return TLV_ERR_BUFFER_TOO_SHORT;
    for (size_t i = 0; i < count; ++i)
        if (rules[i].context->level == TLV_QUERY_D || rules[i].assertion->level == TLV_QUERY_D)
            return TLV_ERR_UNSUPPORTED;
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
        rc = buffer_evaluate(data, size, format, exec, depth, nodes, w, detail);
        if (rc != TLV_OK)
            return query_failure(detail, rc, TLV_QUERY_ERROR_EVENTS,
                                 "valid Schema Query execution");
        size_t contexts = 0;
        for (;;) {
            tlv_tree_event_t event;
            size_t ordinal;
            rc = tlv_query_result_next_ordinal(exec, &event, &ordinal);
            if (rc == TLV_END) break;
            if (rc != TLV_OK)
                return query_failure(detail, rc, TLV_QUERY_ERROR_EVENTS,
                                     "valid Schema Query execution");
            if (contexts == w->context_capacity)
                return query_limit(detail, "schema-contexts", w->context_capacity, 0, 0);
            w->contexts[contexts].ordinal = ordinal;
            w->contexts[contexts++].event = event;
        }
        for (size_t j = 0; j < contexts; ++j) {
            rc = tlv_query_eval_init(rules[i].assertion, rules[i].environment, w->assertion,
                                     w->assertion_size, depth, nodes, work, &exec);
            if (rc != TLV_OK)
                return query_failure(detail, rc, TLV_QUERY_ERROR_EVENTS,
                                     "valid Schema Query execution");
            rc = tlv_query_exec_context(exec, w->contexts[j].ordinal);
            if (rc != TLV_OK)
                return query_failure(detail, rc, TLV_QUERY_ERROR_EVENTS,
                                     "valid Schema Query execution");
            rc = buffer_evaluate(data, size, format, exec, depth, nodes, w, detail);
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
                    assertion_failure(&diagnostic->query, w->contexts[j].event.element.tag,
                                      rules[i].name);
                    if (w->contexts[j].event.source.data)
                        tlv_diagnostic_set_location(
                            &diagnostic->query.diagnostic, TLV_LOCATION_INPUT, TLV_LOCATION_POINT,
                            w->contexts[j].event.offset, w->contexts[j].event.offset);
                    context_path(data, size, format, depth, nodes, w, w->contexts[j].ordinal,
                                 &diagnostic->query.diagnostic.path);
                }
                /* diagnostic-return: query.diagnostic.code is set for the failed assertion above.
                 */
                return TLV_ERR_SCHEMA;
            }
        }
    }
    return TLV_OK;
}
