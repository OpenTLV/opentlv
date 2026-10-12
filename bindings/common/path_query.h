// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel
#ifndef OPENTLV_BINDING_PATH_QUERY_H
#define OPENTLV_BINDING_PATH_QUERY_H

/* A rejected V1 path text is a Query failure, not a Reader failure. Bindings
 * report it through their Query error projection: the parse result's Query kind,
 * the EXPRESSION location tlv_query_parse_n() reported and no lower-layer cause. */
#include <tlv/query/program.h>

static inline tlv_query_diagnostic_t opentlv_binding_path_failure(tlv_result_t            code,
                                                                  const tlv_diagnostic_t* parse) {
    tlv_query_diagnostic_t diagnostic = {0};
    if (parse) diagnostic.diagnostic = *parse;
    diagnostic.diagnostic.code = code;
    diagnostic.kind = code == TLV_ERR_SYNTAX  ? TLV_QUERY_ERROR_SYNTAX
                      : code == TLV_ERR_LIMIT ? TLV_QUERY_ERROR_LIMIT
                                              : TLV_QUERY_ERROR_ARGUMENT;
    diagnostic.cause = TLV_QUERY_CAUSE_NONE;
    return diagnostic;
}
#endif
