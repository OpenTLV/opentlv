// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel
#ifndef OPENTLV_SCHEMA_QUERY_DOCUMENT_H
#define OPENTLV_SCHEMA_QUERY_DOCUMENT_H
#include "tlv/schema/query.h"
#include "tlv/document/document.h"
#include "tlv/writer/tree.h"
#include "tlv/schema-query-document/export.h"
#ifdef __cplusplus
extern "C" {
#endif
/** @file
 * @ingroup schemas
 * @brief Document contextual assertions; link OpenTLV::SchemaQueryDocument.
 */
/** @brief Validate the same assertion programs over an immutable Document, including D.
 * @param document Owning tree, alive and unchanged throughout validation.
 * @param rules Borrowed compiled rules.
 * @param count Rule count; zero skips validation explicitly.
 * @param depth Maximum depth.
 * @param nodes Explicit retained-node capacity.
 * @param work Native budget per selector/context assertion execution.
 * @param workspace Sequential execution and context storage from query_size.
 * @param values Caller-owned canonical Value snapshot if programs require Values.
 * @param capacity Value storage bytes.
 * @param staging Bounded Writer frames/scratch when Values are required.
 * @param diagnostic Optional original engine and Schema context failure.
 * @return OK, SCHEMA for false, or original Query/Writer/capacity failure.
 * Invalid program contents and rule result types return #TLV_ERR_INVALID_VALUE
 * with Query kind IMAGE or TYPE and the failing rule index.
 * @note No allocation. D uses the same VM and original Document contexts. Historical
 * Source offsets are unavailable. Structural Schema rules remain separately optional. */
OPENTLV_SCHEMA_QUERY_DOCUMENT_API tlv_result_t tlv_schema_query_validate_document(
    const tlv_document_t* document, const tlv_schema_query_rule_t* rules, size_t count,
    size_t depth, size_t nodes, size_t work, tlv_schema_query_workspace_t* workspace,
    uint8_t* values, size_t capacity, tlv_tree_writer_workspace_t* staging,
    tlv_schema_query_diagnostic_t* diagnostic);
#ifdef __cplusplus
}
#endif
#endif
