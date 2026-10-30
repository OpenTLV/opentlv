// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel
#ifndef OPENTLV_SCHEMA_QUERY_DIAGNOSTIC_INTERNAL_H
#define OPENTLV_SCHEMA_QUERY_DIAGNOSTIC_INTERNAL_H
#include "tlv/query/program.h"
#include <string.h>
/* Schema-owned composition of public Query diagnostics. */
static inline void query_diag_init(tlv_query_diagnostic_t* d) {
    if (d) memset(d, 0, sizeof *d);
}
static inline tlv_result_t query_error(tlv_query_diagnostic_t* d, tlv_result_t code,
                                       tlv_query_error_kind_t kind, size_t begin, size_t end,
                                       const char* expected) {
    if (d) {
        tlv_diagnostic_init(&d->diagnostic, code, TLV_DIAGNOSTIC_SEVERITY_ERROR);
        tlv_diagnostic_set_location(&d->diagnostic, TLV_LOCATION_EXPRESSION, TLV_LOCATION_SPAN,
                                    begin, end);
        d->diagnostic.expected = expected;
        d->kind = code == TLV_ERR_INVALID_STATE ? TLV_QUERY_ERROR_STATE
                  : code == TLV_ERR_CALLBACK    ? TLV_QUERY_ERROR_CALLBACK
                                                : kind;
        memset(&d->expression, 0, sizeof d->expression);
        d->cause = TLV_QUERY_CAUSE_NONE;
    }
    return code;
}
/* Before another location becomes primary, keep a primary expression span as the
 * related secondary span, so each span is stored exactly once. */
static inline void query_relate_expression(tlv_query_diagnostic_t* d) {
    if (d->diagnostic.location.domain == TLV_LOCATION_EXPRESSION &&
        d->diagnostic.location.kind != TLV_LOCATION_UNKNOWN)
        d->expression = d->diagnostic.location;
}
static inline void query_input_location(tlv_query_diagnostic_t* d, size_t offset) {
    query_relate_expression(d);
    tlv_diagnostic_set_location(&d->diagnostic, TLV_LOCATION_INPUT, TLV_LOCATION_POINT, offset,
                                offset);
}
/* The Reader diagnostic becomes the common part unchanged; its detail is the only cause. */
static inline void query_reader_failure(tlv_query_diagnostic_t* d, tlv_result_t rc,
                                        const tlv_reader_diagnostic_t* reader) {
    d->kind = rc == TLV_ERR_INVALID_STATE ? TLV_QUERY_ERROR_STATE : TLV_QUERY_ERROR_READER;
    memset(&d->expression, 0, sizeof d->expression);
    d->cause = TLV_QUERY_CAUSE_READER;
    d->detail.reader = reader->detail;
    d->diagnostic = reader->diagnostic;
}
/* Admission/lifecycle failures have no expression evidence, even at byte zero. */
static inline tlv_result_t query_error_unlocated(tlv_query_diagnostic_t* d, tlv_result_t code,
                                                 tlv_query_error_kind_t kind,
                                                 const char* expected) {
    tlv_result_t rc = query_error(d, code, kind, 0, 0, expected);
    if (d) memset(&d->diagnostic.location, 0, sizeof d->diagnostic.location);
    return rc;
}
static inline tlv_result_t query_limit(tlv_query_diagnostic_t* d, const char* name,
                                       size_t configured, size_t begin, size_t end) {
    if (d) {
        d->limit = name;
        d->configured = configured;
    }
    return query_error(d, TLV_ERR_LIMIT, TLV_QUERY_ERROR_LIMIT, begin, end, NULL);
}

/* A lower-layer call that reports no Query detail is classified by its result:
 * argument, storage, capability and callback results never take the caller's category. */
static inline tlv_query_error_kind_t query_result_kind(tlv_result_t rc,
                                                       tlv_query_error_kind_t kind) {
    switch (rc) {
        case TLV_ERR_NULL_ARG:
        case TLV_ERR_INVALID_ARG: return TLV_QUERY_ERROR_ARGUMENT;
        case TLV_ERR_BUFFER_TOO_SHORT:
        case TLV_ERR_OVERFLOW:
        case TLV_ERR_NATIVE_SIZE:
        case TLV_ERR_OUT_OF_MEMORY: return TLV_QUERY_ERROR_STORAGE;
        case TLV_ERR_UNSUPPORTED: return TLV_QUERY_ERROR_CAPABILITY;
        case TLV_ERR_LIMIT: return TLV_QUERY_ERROR_LIMIT;
        case TLV_ERR_CALLBACK:
        case TLV_ERR_VISITOR: return TLV_QUERY_ERROR_CALLBACK;
        default: return kind;
    }
}

/* Preserve callee detail; state results always have their own top-level kind. */
static inline tlv_result_t query_failure(tlv_query_diagnostic_t* d, tlv_result_t rc,
                                         tlv_query_error_kind_t kind, const char* expected) {
    if (rc != TLV_OK && d && d->kind == TLV_QUERY_ERROR_NONE)
        query_error_unlocated(d, rc, query_result_kind(rc, kind), expected);
    if (rc == TLV_ERR_INVALID_STATE && d) d->kind = TLV_QUERY_ERROR_STATE;
    if (d && rc != TLV_OK) {
        d->diagnostic.code = rc;
    }
    return rc;
}
#endif
