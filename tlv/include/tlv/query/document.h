// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel
#ifndef OPENTLV_QUERY_DOCUMENT_H
#define OPENTLV_QUERY_DOCUMENT_H

#include "tlv/export.h"
#include "tlv/document/document.h"
#include "tlv/query/query.h"

/** @file
 * @ingroup query
 * @brief Query-owned integration with the public Document API.
 * Link OpenTLV::tlv while the independent Query integration is extracted in #650.
 */
#ifdef __cplusplus
extern "C" {
#endif

/** @brief Opaque compiled Query execution, defined in tlv/query/program.h. */
struct tlv_query_exec;
/** @brief Query diagnostic, defined in tlv/query/program.h. */
struct tlv_query_diagnostic;
/** @brief Caller Writer staging, defined in tlv/writer/tree.h. */
struct tlv_tree_writer_workspace;

/** @brief Completed Query selection edit operation. */
typedef enum tlv_document_query_edit_kind {
    TLV_DOCUMENT_QUERY_REMOVE,  /**< Erase selected roots; selected ancestors dominate descendants.
                                 */
    TLV_DOCUMENT_QUERY_REPLACE, /**< Replace Values; selected ancestors dominate descendants. */
    TLV_DOCUMENT_QUERY_INSERT_AFTER /**< Insert one sibling immediately after each selected target.
                                     */
} tlv_document_query_edit_kind_t;

/** @brief Edit an already finalized compiled Query selection, never an active traversal.
 * @param document Live mutable Document used by execution.
 * @param exec Finished node-result execution; all targets are collected before mutation.
 * @param kind Operation selector.
 * @param tag Insertion identifier; ignored by other operations.
 * @param value Replacement/insertion Value, copied before editing; ignored by removal.
 * @param size Value bytes.
 * @param targets Exclusive caller array of node pointers for the complete selection.
 * @param capacity Array entries; short storage leaves the cursor and targets unchanged,
 * permitting retry of the same completed execution with a larger target array.
 * @param applied Required output, initialized to zero; successful edited selected roots.
 * Targets, applied and diagnostic must not overlap each other, the execution or a used
 * Value; such overlap returns #TLV_ERR_INVALID_ARG before writes.
 * @param[out] diagnostic Optional Query failure detail; NULL when not needed. A
 * constructed Value rejected by the Reader keeps the original Reader detail, with
 * #TLV_LOCATION_INPUT relative to `value`. An edit exceeding the Document depth or
 * element limit, including inside a constructed Value, reports #TLV_ERR_LIMIT with kind
 * #TLV_QUERY_ERROR_LIMIT, `limit` "document-depth" or "document-elements" and its
 * `configured` bound. A rejected Tag, Value framing or Document allocation keeps its
 * result with kind #TLV_QUERY_ERROR_ARGUMENT or #TLV_QUERY_ERROR_STORAGE.
 * @return OK for no matches, or native error. Invalid operation/capacity/revision makes
 * no edits. Remove cannot fail after collection. Replace/insert commit in preorder,
 * stopping at first failure; previous successful edits remain, without rollback.
 * Tag framing, constructed Value syntax and every target's depth/count limits are
 * prevalidated before editing; their failure makes no tree changes. Allocation or
 * identity exhaustion can still cause partial application during commit.
 * @note Ancestor dominance is resolved before editing. Insertion includes every selected
 * node once and preserves original sibling order. Document allocator owns temporary Value
 * copies and normal inserted nodes. Native pointers borrow Document; keep it alive through
 * collection. Any successful edit invalidates the execution's whole-document revision.
 * REPLACE stores identical bytes on primitive targets but parses them as children on
 * constructed targets. Overlap filtering is O(target count * depth), allocation-free.
 * @note Reentrancy, failed-execution and overlap guards return before initializing
 * `diagnostic` and leave it untouched, preserving the original failure of the execution;
 * after initialization, every non-#TLV_OK return has `diagnostic.code == rc`.
 * @return #TLV_ERR_INVALID_STATE during callbacks, for stale results or after result iteration has
 * begun.
 */
TLV_API tlv_result_t tlv_document_query_edit(tlv_document_t* document, struct tlv_query_exec* exec,
                                             tlv_document_query_edit_kind_t kind, tlv_tag_t tag,
                                             const uint8_t* value, size_t size,
                                             tlv_node_t** targets, size_t capacity, size_t* applied,
                                             struct tlv_query_diagnostic* diagnostic);

/** @brief Callback for a matching Document Node.
 * @param node Borrowed matching node.
 * @param context Caller context.
 * @return CONTINUE, STOP or ERROR following the Visitor contract.
 */
typedef tlv_visit_result_t (*tlv_document_query_visitor_t)(tlv_node_t* node, void* context);

/** @brief Visit every path match in Document order using the canonical Query matcher.
 * @param document Borrowed Document.
 * @param query Parsed Query, borrowed for the call.
 * @param visitor Required callback.
 * @param context Optional caller context.
 * @param[out] diagnostic Optional Query failure detail, initialized before the arguments
 * are checked; NULL when not needed. Include tlv/query/program.h for its definition.
 * @return OK on exhaustion or STOP, NULL_ARG for missing arguments, Query validation
 * errors, VISITOR for ERROR, or CALLBACK for an unknown callback result.
 * @warning Do not mutate or destroy the Document during traversal. Nodes borrow it.
 * Callback effects are not rolled back on failure.
 * @note Never allocates. Every non-#TLV_OK return has `diagnostic.code == rc` and no
 * location: missing arguments and an invalid parsed path have kind
 * #TLV_QUERY_ERROR_ARGUMENT, keeping the path validation result, #TLV_ERR_VISITOR and
 * #TLV_ERR_CALLBACK have #TLV_QUERY_ERROR_CALLBACK, and #TLV_ERR_INVALID_STATE has
 * #TLV_QUERY_ERROR_STATE.
 * @return #TLV_ERR_INVALID_STATE if a callback requests deferred erasure or destruction.
 */
TLV_API tlv_result_t tlv_document_query_visit(const tlv_document_t* document,
                                              const tlv_query_t* query,
                                              tlv_document_query_visitor_t visitor, void* context,
                                              struct tlv_query_diagnostic* diagnostic);

/** @brief Discover the complete encoded snapshot size for compiled Document Query Values.
 * @param[in] document Live owning Document.
 * @param[in,out] staging Caller-owned Tree Writer frames/output/scratch, disjoint
 * from Document storage. Short storage reports Writer requirements; resize
 * explicitly and repeat discovery from the start.
 * @param[out] bytes Complete encoded Document size, not a sum of nested Values.
 * @param[out] diagnostic Optional Query failure detail, initialized before the arguments
 * are checked; NULL when not needed.
 * @return OK, NULL argument, overflow or original Writer sizing error.
 * @note No allocation. Encodes the Document once through the canonical Writer.
 * Discovery is outside the execution work budget. It is unnecessary when program
 * info reports constructed_values_required == 0. Output may be reused as the
 * evaluation snapshot. Include tlv/writer/tree.h for the workspace definition.
 * @note Every non-#TLV_OK return has `diagnostic.code == rc` and no location. A Writer
 * result is classified by result: short staging is #TLV_ERR_BUFFER_TOO_SHORT with kind
 * #TLV_QUERY_ERROR_STORAGE and a missing argument has #TLV_QUERY_ERROR_ARGUMENT. */
TLV_API tlv_result_t tlv_document_query_value_size(const tlv_document_t* document,
                                                   struct tlv_tree_writer_workspace* staging,
                                                   size_t* bytes,
                                                   struct tlv_query_diagnostic* diagnostic);

/** @brief Evaluate a compiled program over Document nodes using a fresh retained execution.
 * @param[in] document Live Document; must remain unchanged through result consumption.
 * @param[in,out] exec Fresh execution created with tlv_query_eval_init, including D plans.
 * @param[in] context Optional live node in this Document; NULL selects its virtual root.
 * Erased/stale pointers are forbidden. Ownership validation is O(1).
 * @param[in,out] values Complete encoded snapshot storage, alive until execution reset.
 * May reuse discovery staging.data. NULL/zero is sufficient when constructed
 * Values are not required by the compiled program.
 * @param[in] capacity Snapshot bytes, discovered with query_value_size when needed.
 * @param[in,out] staging Caller-owned bounded Tree Writer frames and closing scratch.
 * Output data/capacity are used only during discovery; evaluation writes directly
 * into values. Frames/scratch must be disjoint from values, Query and Document storage.
 * May be NULL when program info reports constructed_values_required == 0.
 * @param[out] diagnostic Optional error detail, with retained source offset when available.
 * @return OK with finalized nodes/scalar; capacity, depth, candidate, work or original
 * Writer/Reader/evaluation errors. A foreign context or used execution is invalid.
 * @note No allocation. Uses public node navigation and the shared Query VM. Programs
 * that may inspect constructed Values encode the Document once; canonical Reader
 * decoding maps nodes to slices of that snapshot. Other programs skip encoding entirely.
 * Encoding follows current Document semantics, including edits. Historical wire
 * spellings are not preserved. `@offset` and `@hlen` use optional retained original
 * locations; unavailable locations produce SOURCE diagnostics. They never refer to
 * the canonical Value snapshot. Work charges use visited events
 * and actual byte extents, independently of spare buffer capacity. General Writer
 * encoding and Query evaluation may still be superlinear on deeply nested inputs.
 * Failure after execution begins is terminal until reset; no results have been emitted.
 * During evaluation and result callbacks, replacement/insertion and nested traversal
 * on this Document are rejected. Erase/free requests are deferred until the enclosing
 * operation returns from its callbacks, then invalidate execution. Borrowed node and
 * Value spans remain alive during callbacks. Outside those operations the caller must
 * keep the Document alive; this API does not retain ownership or synchronize threads.
 * Include tlv/query/program.h and, when needed, tlv/writer/tree.h for type definitions.
 * @return #TLV_ERR_INVALID_STATE for used execution, callback reentrancy or callback invalidation.
 */
TLV_API tlv_result_t tlv_document_query_evaluate(const tlv_document_t* document,
                                                 struct tlv_query_exec* exec,
                                                 const tlv_node_t* context, void* values,
                                                 size_t capacity,
                                                 struct tlv_tree_writer_workspace* staging,
                                                 struct tlv_query_diagnostic* diagnostic);

/** @brief Pull the next finalized unique node handle in current Document preorder.
 * @param[in,out] exec Successfully evaluated Document node execution.
 * @param[out] node Borrowed node; unchanged on exhaustion or error.
 * @param[out] diagnostic Optional Query failure detail; NULL when not needed. It is
 * written only on failure, so a pull loop does not clear it on every node.
 * @return OK, #TLV_END at exhaustion, NULL argument or invalid execution state/revision.
 * @note Document and Value storage must remain alive and unchanged. First is one pull;
 * all is repeated pulls. Any successful Document edit rejects subsequent pulls
 * before dereferencing retained nodes. Iteration allocates nothing and also works
 * without Source. Keep the native Document alive; revision is not a destruction token.
 * @note Every failure has `diagnostic.code == rc`, no location and kind
 * #TLV_QUERY_ERROR_STATE or #TLV_QUERY_ERROR_ARGUMENT. #TLV_END ends the iteration
 * normally and leaves `diagnostic` unchanged. Reentrancy, failed-execution and overlap
 * guards also leave it unchanged, preserving the original failure of the execution.
 * @return #TLV_ERR_INVALID_STATE for unfinished/failed execution, reentrancy or a stale Document
 * revision.
 */
TLV_API tlv_result_t tlv_document_query_next(struct tlv_query_exec* exec, tlv_node_t** node,
                                             struct tlv_query_diagnostic* diagnostic);

/** @brief Visit remaining finalized Document results; STOP resumes after the delivered node.
 * @param[in,out] exec Successfully evaluated Document node execution.
 * @param[in] visitor Required callback; must not edit the Document.
 * @param[in] context Optional callback context.
 * @param[out] diagnostic Optional Query failure detail; NULL when not needed. It must not
 * overlap the execution workspace.
 * @return OK on exhaustion/STOP, VISITOR on callback error (terminal), or pull errors.
 * @warning Previously delivered callbacks are never rolled back. Erase/free requests
 * are deferred until callback return and invalidate this execution. Same-execution
 * feed, finish, reset, bind and nested visits are rejected without altering it.
 * @note Every non-#TLV_OK return after initialization has `diagnostic.code == rc` and no
 * location: #TLV_ERR_VISITOR and #TLV_ERR_CALLBACK have kind #TLV_QUERY_ERROR_CALLBACK,
 * #TLV_ERR_INVALID_STATE has #TLV_QUERY_ERROR_STATE and a missing argument has
 * #TLV_QUERY_ERROR_ARGUMENT. Reentrancy, failed-execution and overlap guards return before
 * initialization and leave `diagnostic` untouched.
 * @return #TLV_ERR_INVALID_STATE for callback reentrancy, invalidated results or deferred mutation.
 */
TLV_API tlv_result_t tlv_document_query_program_visit(struct tlv_query_exec* exec,
                                                      tlv_document_query_visitor_t visitor,
                                                      void* context,
                                                      struct tlv_query_diagnostic* diagnostic);

/**
 * @brief Finds the first element addressed by a path query.
 *
 * Uses the query language of tlv_query_parse(), for example `6F/A5/50`: the first tag names
 * a top-level element, each further tag one of its direct children. The elements
 * addressed by the query are searched in document order, and a step that leads to a dead end
 * does not hide a later match.
 *
 * @param[in] document Document to search.
 * @param[in] query    Parsed query.
 * @param[out] node    Required output; receives the first borrowed matching node,
 *                    or `NULL` when no node matches. Must not overlap the inputs.
 *
 * @return #TLV_OK on a completed search, including no match.
 * @return #TLV_ERR_NULL_ARG if a required argument is `NULL`.
 * @return #TLV_ERR_INVALID_ARG if the query is empty or has an invalid step count.
 * @return #TLV_ERR_INVALID_TAG_SIZE if the query contains invalid step boundaries.
 * @note Never allocates. On failure, `*node` is unchanged. Query validation happens
 * even for an empty Document. The returned node borrows the Document.
 */
TLV_API tlv_result_t tlv_document_find_path(const tlv_document_t* document,
                                            const tlv_query_t* query, tlv_node_t** node);

#ifdef __cplusplus
}
#endif
#endif
