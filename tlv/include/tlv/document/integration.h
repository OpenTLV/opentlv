// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel
#ifndef OPENTLV_DOCUMENT_INTEGRATION_H
#define OPENTLV_DOCUMENT_INTEGRATION_H

#include "tlv/document/document.h"

/** @file
 * @ingroup document
 * @brief Public lifetime and metadata boundary for Document consumers.
 * Integrations combine these operations with public node navigation, identity,
 * revision and mutation APIs. No private node or Document layout is exposed.
 */
#ifdef __cplusplus
extern "C" {
#endif

/** @brief Copy the effective Document options for an integration.
 * @param document Live Document, or NULL for an all-zero result.
 * @return Options with the actual allocator, including the default allocator.
 * The allocator pointer borrows the Document; copy its value if needed beyond
 * this call. Format and allocator contexts retain their original lifetimes.
 * @note Temporary integration storage must use nonzero allocation sizes and
 * release non-NULL allocations through the same allocator before destruction.
 */
OPENTLV_DOCUMENT_API tlv_document_options_t tlv_document_options(const tlv_document_t* document);

/** @brief Test ownership of a live node in constant time.
 * @param document Live Document, or NULL.
 * @param node Live node, or NULL. A stale address must instead be checked with
 * tlv_document_node_identity(), which does not dereference the candidate.
 * @return Nonzero if node belongs to document; zero for NULL arguments.
 */
OPENTLV_DOCUMENT_API int tlv_document_contains(const tlv_document_t* document,
                                               const tlv_node_t* node);

/** @brief Observe whether a Document has an active callback lifetime guard.
 * @param document Live Document, or NULL.
 * @return Nonzero during guarded operations, zero otherwise or for NULL.
 * @note Integrations use this to reject nested execution before touching state.
 */
OPENTLV_DOCUMENT_API int tlv_document_callbacks_active(const tlv_document_t* document);

/** @brief Enter or leave a nested callback lifetime guard.
 * @param document Required live Document; callers must balance every entry with
 * one exit, including on failure. This API does not synchronize threads.
 * @param active Nonzero enters; zero exits an existing guard.
 * @return Zero on entry or a nested exit. Outermost exit returns zero when
 * unchanged, one after deferred erasures, or two after deferred destruction.
 * @note While guarded, fallible edits fail with INVALID_STATE. Erase/free defer
 * reclamation until the outermost exit, preserving borrowed nodes and Values.
 * An exit returning two destroys the Document: do not access it again. An exit
 * returning one invalidates retained traversal state. Revision and retirement
 * epoch advance through the ordinary mutation operations during reclamation.
 * The caller must keep nesting representable in size_t and must not exit an
 * unguarded Document. No allocation is performed by entry or an unchanged exit.
 */
OPENTLV_DOCUMENT_API int tlv_document_callback(tlv_document_t* document, int active);

#ifdef __cplusplus
}
#endif
#endif
