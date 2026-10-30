// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel
#ifndef OPENTLV_QUERY_DEFINITION_H
#define OPENTLV_QUERY_DEFINITION_H
#include "tlv/query/program.h"
#ifdef __cplusplus
extern "C" {
#endif
/** @brief One explicit namespace of borrowed descriptive definitions. */
typedef struct tlv_query_definition_scope {
    const char* namespace_name;                   /**< NUL-terminated namespace. */
    const tlv_definition_registry_t* definitions; /**< Borrowed immutable registry. */
} tlv_query_definition_scope_t;
/** @brief Compile-time scope collection, not a protocol dictionary. */
typedef struct tlv_query_definition_resolver {
    const tlv_query_definition_scope_t* scopes; /**< Borrowed namespace entries. */
    size_t count;                               /**< Entry count. */
} tlv_query_definition_resolver_t;
/** @brief Resolve names using exact registry labels in explicit namespaces.
 * @param[in] context Required tlv_query_definition_resolver_t.
 * @param[in] namespace_name Bounded optional namespace.
 * @param[in] namespace_size Namespace bytes; zero searches all scopes.
 * @param[in] name Bounded descriptive name.
 * @param[in] name_size Name bytes.
 * @param[out] tag Borrowed tag, unchanged on failure.
 * @return OK, INVALID_TAG for unknown, INVALID_ARG for multiple matches,
 * or NULL_ARG for invalid pointer arguments. No allocation or codec policy. */
OPENTLV_QUERY_API tlv_result_t tlv_query_definition_resolve(const void* context,
                                                            const char* namespace_name,
                                                            size_t namespace_size, const char* name,
                                                            size_t name_size, tlv_tag_t* tag);
#ifdef __cplusplus
}
#endif
#endif
