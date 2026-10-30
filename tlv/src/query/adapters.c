// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel
#include "tlv/query/definition.h"
#include <string.h>
static int same_name(const char* text, size_t size, const char* name) {
    return name && strlen(name) == size && (!size || !memcmp(text, name, size));
}
tlv_result_t tlv_query_definition_resolve(const void* context, const char* ns, size_t ns_size,
                                          const char* name, size_t name_size, tlv_tag_t* tag) {
    if (!context || !name || !tag || (!ns && ns_size)) return TLV_ERR_NULL_ARG;
    const tlv_query_definition_resolver_t* resolver = context;
    if (resolver->count && !resolver->scopes) return TLV_ERR_NULL_ARG;
    tlv_tag_t found = {0};
    size_t matches = 0;
    for (size_t i = 0; i < resolver->count; ++i) {
        const tlv_query_definition_scope_t* scope = &resolver->scopes[i];
        if (ns_size && !same_name(ns, ns_size, scope->namespace_name)) continue;
        if (!scope->definitions || (scope->definitions->count && !scope->definitions->entries))
            return TLV_ERR_INVALID_ARG;
        for (size_t j = 0; j < scope->definitions->count; ++j) {
            const tlv_definition_t* definition = &scope->definitions->entries[j];
            if (same_name(name, name_size, definition->name)) {
                if (!definition->tag.size || !definition->tag.data) return TLV_ERR_INVALID_TAG;
                found = definition->tag;
                ++matches;
            }
        }
    }
    if (matches != 1) return matches ? TLV_ERR_INVALID_ARG : TLV_ERR_INVALID_TAG;
    *tag = found;
    return TLV_OK;
}
