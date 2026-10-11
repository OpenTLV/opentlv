// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel
#ifndef OPENTLV_TEST_DIAGNOSTIC_INVARIANT_H
#define OPENTLV_TEST_DIAGNOSTIC_INVARIANT_H
#include "tlv/diagnostic.h"
#include "tlv/query/program.h"
static inline int test_diagnostic_matches(tlv_result_t rc, const tlv_diagnostic_t* diagnostic) {
    return rc == TLV_OK || diagnostic->code == rc;
}
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
            test_query_cause_matches(diagnostic) &&
            /* An expression span is stored once: primary or related, never both. */
            (diagnostic->diagnostic.location.domain != TLV_LOCATION_EXPRESSION ||
             diagnostic->expression.kind == TLV_LOCATION_UNKNOWN));
}
#endif
