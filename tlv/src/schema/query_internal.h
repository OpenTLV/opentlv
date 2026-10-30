// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel
#ifndef OPENTLV_SCHEMA_QUERY_INTERNAL_H
#define OPENTLV_SCHEMA_QUERY_INTERNAL_H
#include "tlv/schema/query.h"
#include "tlv/query/plan.h"
#include "query_diagnostic_internal.h"
static inline tlv_result_t schema_query_requirements(const tlv_schema_query_rule_t* rules,
                                                     size_t count, size_t depth, size_t nodes,
                                                     size_t* selector, size_t* assertion,
                                                     size_t* alignment,
                                                     tlv_schema_query_diagnostic_t* diagnostic);

/* A false assertion is a Schema finding: the common part carries SCHEMA and the
 * ASSERTION detail is the only cause; location and path are added by the caller. */
static inline void assertion_failure(tlv_query_diagnostic_t* d, tlv_tag_t tag, const char* name) {
    query_diag_init(d);
    tlv_diagnostic_init(&d->diagnostic, TLV_ERR_SCHEMA, TLV_DIAGNOSTIC_SEVERITY_ERROR);
    d->diagnostic.expected = "contextual Query assertion true";
    d->diagnostic.has_path = 1;
    d->kind = TLV_QUERY_ERROR_SCHEMA;
    d->cause = TLV_QUERY_CAUSE_SCHEMA;
    tlv_schema_detail_t detail = {0};
    detail.kind = TLV_SCHEMA_ISSUE_ASSERTION;
    detail.tag = tag;
    detail.field = name;
    (void)tlv_schema_detail_attach(&d->detail.causes, &detail);
}

static inline tlv_result_t schema_query_requirements(const tlv_schema_query_rule_t* rules,
                                                     size_t count, size_t depth, size_t nodes,
                                                     size_t* selector, size_t* assertion,
                                                     size_t* alignment,
                                                     tlv_schema_query_diagnostic_t* diagnostic) {
    if ((!rules && count) || !selector || !assertion || !alignment) return TLV_ERR_NULL_ARG;
    size_t selected = 0, asserted = 0, aligned = 1;
    for (size_t i = 0; i < count; ++i) {
        if (!rules[i].context || !rules[i].assertion) return TLV_ERR_NULL_ARG;
        if ((uintptr_t)rules[i].context % sizeof(uint32_t) ||
            (uintptr_t)rules[i].assertion % sizeof(uint32_t))
            return TLV_ERR_INVALID_ARG;
        tlv_query_error_kind_t kind = TLV_QUERY_ERROR_NONE;
        const char* expected = NULL;
        tlv_query_program_info_t context_info = {0}, assertion_info = {0};
        context_info.struct_size = sizeof context_info;
        assertion_info.struct_size = sizeof assertion_info;
        if (tlv_query_plan_info(rules[i].context, &context_info) != TLV_OK ||
            tlv_query_plan_info(rules[i].assertion, &assertion_info) != TLV_OK) {
            kind = TLV_QUERY_ERROR_IMAGE;
            expected = "valid Schema Query program image";
        } else if (context_info.result_kind != TLV_QUERY_RESULT_NODES ||
                   assertion_info.result_kind != TLV_QUERY_RESULT_BOOL) {
            kind = TLV_QUERY_ERROR_TYPE;
            expected = "node context selector and boolean assertion";
        }
        if (kind != TLV_QUERY_ERROR_NONE) {
            if (diagnostic) {
                memset(diagnostic, 0, sizeof *diagnostic);
                diagnostic->rule = i;
            }
            return query_error_unlocated(diagnostic ? &diagnostic->query : NULL,
                                         TLV_ERR_INVALID_VALUE, kind, expected);
        }
        size_t bytes, align;
        tlv_result_t rc = tlv_query_eval_size(rules[i].context, depth, nodes, &bytes, &align);
        if (rc != TLV_OK) return rc;
        if (bytes > selected) selected = bytes;
        if (align > aligned) aligned = align;
        rc = tlv_query_eval_size(rules[i].assertion, depth, nodes, &bytes, &align);
        if (rc != TLV_OK) return rc;
        if (bytes > asserted) asserted = bytes;
        if (align > aligned) aligned = align;
    }
    *selector = selected;
    *assertion = asserted;
    *alignment = aligned;
    return TLV_OK;
}

#endif
