// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel
#ifndef OPENTLV_TEST_DIAGNOSTIC_INVARIANT_H
#define OPENTLV_TEST_DIAGNOSTIC_INVARIANT_H
#include "tlv/diagnostic.h"
#include "tlv/query/program.h"
#include "tlv/schema/schema.h"
static inline int test_diagnostic_matches(tlv_result_t rc, const tlv_diagnostic_t* diagnostic) {
    return rc == TLV_OK || diagnostic->code == rc;
}
/* The #551 Query failure model as allowed (result, kind) pairs. The shared assertions
 * check every observed Query failure against this list, and
 * scripts/check_diagnostic_returns.py checks every literal Query producer with it.
 * ARGUMENT keeps the Tag and Length validation result of a caller-supplied path Query
 * or Document edit (#588); STORAGE includes exhausted Document allocation. */
#define TEST_QUERY_KIND_PAIRS(X)                                                                   \
    X(TLV_ERR_SYNTAX, TLV_QUERY_ERROR_SYNTAX)                                                      \
    X(TLV_ERR_UNSUPPORTED, TLV_QUERY_ERROR_CAPABILITY)                                             \
    X(TLV_ERR_LIMIT, TLV_QUERY_ERROR_LIMIT)                                                        \
    X(TLV_ERR_BUFFER_TOO_SHORT, TLV_QUERY_ERROR_STORAGE)                                           \
    X(TLV_ERR_OVERFLOW, TLV_QUERY_ERROR_STORAGE)                                                   \
    X(TLV_ERR_NATIVE_SIZE, TLV_QUERY_ERROR_STORAGE)                                                \
    X(TLV_ERR_OUT_OF_MEMORY, TLV_QUERY_ERROR_STORAGE)                                              \
    X(TLV_ERR_INVALID_VALUE, TLV_QUERY_ERROR_EVENTS)                                               \
    X(TLV_ERR_INVALID_VALUE, TLV_QUERY_ERROR_SOURCE)                                               \
    X(TLV_ERR_UNSUPPORTED, TLV_QUERY_ERROR_SOURCE)                                                 \
    X(TLV_ERR_INVALID_VALUE, TLV_QUERY_ERROR_BINDING)                                              \
    X(TLV_ERR_INVALID_VALUE, TLV_QUERY_ERROR_CARDINALITY)                                          \
    X(TLV_ERR_UNSUPPORTED, TLV_QUERY_ERROR_IMAGE_VERSION)                                          \
    X(TLV_ERR_INVALID_STATE, TLV_QUERY_ERROR_STATE)                                                \
    X(TLV_ERR_CALLBACK, TLV_QUERY_ERROR_CALLBACK)                                                  \
    X(TLV_ERR_VISITOR, TLV_QUERY_ERROR_CALLBACK)                                                   \
    X(TLV_ERR_INVALID_VALUE, TLV_QUERY_ERROR_TYPE)                                                 \
    X(TLV_ERR_OVERFLOW, TLV_QUERY_ERROR_TYPE)                                                      \
    X(TLV_ERR_INVALID_VALUE, TLV_QUERY_ERROR_IMAGE)                                                \
    X(TLV_ERR_SCHEMA, TLV_QUERY_ERROR_SCHEMA)                                                      \
    X(TLV_ERR_NULL_ARG, TLV_QUERY_ERROR_ARGUMENT)                                                  \
    X(TLV_ERR_INVALID_ARG, TLV_QUERY_ERROR_ARGUMENT)                                               \
    X(TLV_ERR_INVALID_TAG, TLV_QUERY_ERROR_ARGUMENT)                                               \
    X(TLV_ERR_INVALID_TAG_SIZE, TLV_QUERY_ERROR_ARGUMENT)                                          \
    X(TLV_ERR_INVALID_LENGTH, TLV_QUERY_ERROR_ARGUMENT)
#define TEST_QUERY_KIND_PAIR(result, kind_value) || (rc == (result) && kind == (kind_value))
/* READER and CODEC preserve the lower-layer result, and CAPABILITY the result of a
 * Tag adapter or name resolver, so they also accept a propagated result. */
static inline int test_query_kind_allowed(tlv_result_t rc, tlv_query_error_kind_t kind) {
    return kind == TLV_QUERY_ERROR_READER || kind == TLV_QUERY_ERROR_CODEC ||
           kind == TLV_QUERY_ERROR_CAPABILITY TEST_QUERY_KIND_PAIRS(TEST_QUERY_KIND_PAIR);
}
#undef TEST_QUERY_KIND_PAIR
/* Kind and cause agree in both directions, so a helper that leaves a stale cause fails. */
static inline int test_query_cause_matches(const tlv_query_diagnostic_t* diagnostic) {
    const tlv_query_error_kind_t kind = diagnostic->kind;
    switch (diagnostic->cause) {
        case TLV_QUERY_CAUSE_NONE:
            return kind != TLV_QUERY_ERROR_READER && kind != TLV_QUERY_ERROR_CODEC &&
                   kind != TLV_QUERY_ERROR_SCHEMA;
        case TLV_QUERY_CAUSE_READER:
            return kind == TLV_QUERY_ERROR_READER || kind == TLV_QUERY_ERROR_STATE;
        case TLV_QUERY_CAUSE_CODEC:
            return (kind == TLV_QUERY_ERROR_CODEC && diagnostic->detail.codec.reported != TLV_OK) ||
                   kind == TLV_QUERY_ERROR_CALLBACK || kind == TLV_QUERY_ERROR_STATE;
        case TLV_QUERY_CAUSE_SCHEMA: return kind == TLV_QUERY_ERROR_SCHEMA;
    }
    return 0;
}
static inline int test_query_diagnostic_matches(tlv_result_t                  rc,
                                                const tlv_query_diagnostic_t* diagnostic) {
    return rc == TLV_OK ||
           (diagnostic->diagnostic.code == rc && diagnostic->kind != TLV_QUERY_ERROR_NONE &&
            (rc != TLV_ERR_INVALID_STATE || diagnostic->kind == TLV_QUERY_ERROR_STATE) &&
            test_query_kind_allowed(rc, diagnostic->kind) && test_query_cause_matches(diagnostic) &&
            /* An expression span is stored once: primary or related, never both. */
            (diagnostic->diagnostic.location.domain != TLV_LOCATION_EXPRESSION ||
             diagnostic->expression.kind == TLV_LOCATION_UNKNOWN));
}
/* A Schema finding always selects Schema detail; a Reader cause never claims a finding. */
static inline int test_schema_diagnostic_matches(tlv_result_t                   rc,
                                                 const tlv_schema_diagnostic_t* diagnostic) {
    return rc == TLV_OK ||
           (diagnostic->diagnostic.code == rc &&
            (rc != TLV_ERR_SCHEMA || diagnostic->cause == TLV_SCHEMA_CAUSE_SCHEMA) &&
            (diagnostic->cause == TLV_SCHEMA_CAUSE_NONE ||
             diagnostic->cause == TLV_SCHEMA_CAUSE_SCHEMA ||
             diagnostic->cause == TLV_SCHEMA_CAUSE_READER));
}
#endif
