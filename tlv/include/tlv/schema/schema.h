// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#ifndef OPENTLV_SCHEMA_H
#define OPENTLV_SCHEMA_H

#include "tlv/result.h"
#include "tlv/config.h"
#include "tlv/element.h"
#include "tlv/format.h"
#include "tlv/diagnostic.h"
#include "tlv/reader/diagnostic.h"
#include "tlv/diagnostic/cause.h"
#include "tlv/schema/detail.h"
#include "tlv/schema/export.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 * @ingroup schemas
 * @brief Length schemas and structural schemas for validating TLV data.
 */

/** @addtogroup schemas
 * @{
 */

/** @brief Nesting capacity of the Schema validators' local traversal storage. */
enum { TLV_SCHEMA_MAX_DEPTH = 64 };

/** @brief Maximum distinct tables in one structural schema definition graph.
 * Shared and recursive references count once, independently of input nesting.
 * Exceeding this allocation-free checking capacity returns #TLV_ERR_UNSUPPORTED.
 */
enum { TLV_SCHEMA_MAX_TABLES = 256 };

/**
 * @brief Length rule for one tag in a #tlv_schema_t.
 *
 * Length bounds are inclusive; equal bounds specify an exact length.
 * A nonzero `length_multiple` additionally requires divisibility by that width.
 * #TLV_SCHEMA_LENGTH_ENDPOINTS permits only min_length or max_length.
 */
typedef struct {
    /** Tag this entry describes; borrows its bytes, which must outlive the schema. */
    tlv_tag_t tag;
    /** Minimum permitted value length in bytes, inclusive. */
    size_t min_length;
    /** Maximum permitted value length in bytes, inclusive; `SIZE_MAX` is unrestricted. */
    size_t max_length;
    /** Length policy bits; unknown bits are ignored. See #TLV_SCHEMA_LENGTH_ENDPOINTS. */
    uint32_t flags;
    /** Borrowed name of the field this entry describes, for example `"df_name"`, or `NULL` if
     * unnamed. Used only for diagnostics; never affects validation. */
    const char* name;
    /** Required value-length multiple in bytes; 0 disables this constraint.
     * Zero length is divisible by every nonzero width, subject to the bounds. */
    size_t length_multiple;
} tlv_schema_entry_t;

/** @brief Permit only the two bounds, rather than every length between them.
 * Equal bounds still specify an exact length; length_multiple also applies. */
enum { TLV_SCHEMA_LENGTH_ENDPOINTS = 1 };

/**
 * @brief Borrowed table of per-tag length rules.
 *
 * The table may be `static const`. There is no registration and no
 * allocation. `entries` may be `NULL` only when `count` is zero.
 *
 * @warning Keep the table alive and unchanged while using the schema or
 *          pointers returned by tlv_schema_find().
 */
typedef struct {
    /** Borrowed entries; may be `NULL` only when `count` is zero. */
    const tlv_schema_entry_t* entries;
    /** Number of entries. */
    size_t count;
} tlv_schema_t;

/**
 * @brief Looks up a tag's entry by linear search.
 *
 * Matches by tag size and bytes with tlv_tag_equal(), so a tag matches
 * regardless of the memory backing it. Entries with an invalid tag are
 * skipped. No sorting is required.
 *
 * @param[in] schema Schema to search.
 * @param[in] tag    Tag to find.
 *
 * @return The first matching entry, borrowed from the schema's table.
 * @return `NULL` for an unknown tag, `NULL` arguments, an invalid lookup tag,
 *         or a missing nonempty table. No separate failure channel is provided.
 */
OPENTLV_SCHEMA_API const tlv_schema_entry_t* tlv_schema_find(const tlv_schema_t* schema,
                                                             const tlv_tag_t* tag);

/**
 * @brief Validates a value length against an entry's bounds.
 *
 * Checks only the length, independently of parsing or tag lookup. Bounds are
 * inclusive; equal bounds specify an exact length, and `SIZE_MAX` can be used
 * as an unrestricted upper bound. A nonzero `length_multiple` requires
 * `length % length_multiple == 0`. #TLV_SCHEMA_LENGTH_ENDPOINTS additionally
 * requires length to equal one of the two bounds. Unknown flag bits are ignored.
 *
 * @param[in] entry  Entry providing the bounds.
 * @param[in] length Value length to check.
 *
 * @return #TLV_OK if the length satisfies both the bounds and the multiple.
 * @return #TLV_ERR_SCHEMA for an out-of-range length, a non-multiple,
 *         or an excluded intermediate length under a valid rule.
 * @return #TLV_ERR_INVALID_SCHEMA for reversed bounds.
 * @return #TLV_ERR_NULL_ARG if `entry` is `NULL`.
 */
OPENTLV_SCHEMA_API tlv_result_t tlv_schema_validate_length(const tlv_schema_entry_t* entry,
                                                           size_t length);

struct tlv_structure_schema;

/**
 * @brief Structural rule for one tag within a single parent.
 *
 * @see tlv_structure_schema_t
 */
typedef struct tlv_structure_rule {
    /** Required authoritative field Schema. Borrows immutable storage (including
     * tag bytes and name) that must outlive every use of this structural rule.
     * Multiple rules and dictionaries may share this same field Schema. */
    const tlv_schema_entry_t* entry;
    /**
     * Minimum occurrences within the parent: 1 makes a field required; 0
     * makes it optional or, together with a default value maintained outside
     * this schema-only engine, defaulted. Must be 0 when `group` is nonzero.
     */
    size_t min_occurs;
    /** Maximum occurrences within the parent: 1 prohibits duplicates; `SIZE_MAX` is unrestricted.
     */
    size_t max_occurs;
    /** Required form of the value. */
    tlv_schema_kind_t kind;
    /**
     * Schema for the value's children. Requires #TLV_SCHEMA_CONSTRUCTED and
     * validates the complete value, including required children in empty
     * containers. `NULL` leaves membership unrestricted, while wire structure
     * is still checked recursively.
     */
    const struct tlv_structure_schema* children;
    /**
     * Group this rule belongs to, or 0 if it stands alone (the default).
     * A nonzero value must match the `id` of an entry in the enclosing
     * #tlv_structure_schema_t::groups, and makes this rule one alternative of
     * that #tlv_structure_group_t, modeling an ASN.1-style CHOICE among the
     * group's member tags. `min_occurs` must be 0 for a grouped rule;
     * `max_occurs` still bounds how many times this specific alternative may
     * repeat, independently of the group's own bounds.
     */
    uint32_t group;
} tlv_structure_rule_t;

/**
 * @brief Ordering required among a scope's matched elements.
 *
 * @see tlv_structure_schema_t::order
 */
typedef enum tlv_schema_order {
    /** No relative order is required among matched elements, as for ASN.1 SET
     * and SET OF (the default, and the engine's original behavior). */
    TLV_SCHEMA_ORDER_ANY = 0,
    /**
     * Elements that match a rule must appear in the same relative order as
     * their rules are listed in the table, as for ASN.1 SEQUENCE and
     * SEQUENCE OF. Repeated matches of one rule may appear consecutively.
     * Elements that match no rule (accepted only when `allow_unknown` is
     * nonzero) are not constrained by this ordering.
     */
    TLV_SCHEMA_ORDER_SEQUENCE
} tlv_schema_order_t;

/**
 * @brief A CHOICE-like group of mutually related alternative rules.
 *
 * Occurrence bounds apply to the sum of matches across every rule that
 * shares this group's `id` through #tlv_structure_rule_t::group, instead of
 * to each rule individually: `min_occurs` 1 and `max_occurs` 1 require
 * exactly one occurrence of exactly one of the group's alternatives,
 * modeling an ASN.1 CHOICE component; `min_occurs` 0 models an optional one.
 *
 * @see tlv_structure_rule_t::group, tlv_structure_schema_t::groups
 */
typedef struct tlv_structure_group {
    /** Nonzero identifier, referenced by #tlv_structure_rule_t::group; unique within one
     * #tlv_structure_schema_t::groups table. */
    uint32_t id;
    /** Minimum total occurrences across all member rules, inclusive. */
    size_t min_occurs;
    /** Maximum total occurrences across all member rules, inclusive; `SIZE_MAX` is unrestricted.
     */
    size_t max_occurs;
    /** Borrowed name of the group, for example `"choice_field"`, for diagnostics; `NULL` if
     * unnamed. Never affects validation. */
    const char* name;
} tlv_structure_group_t;

/**
 * @brief Borrowed set of structural rules for one parent scope.
 *
 * Rules apply within one parent only, and tags must be unique in each table.
 * Unknown tags are rejected unless `allow_unknown` is nonzero.
 *
 * @warning Rule tables are borrowed and must stay valid and unchanged while in use.
 */
typedef struct tlv_structure_schema {
    /** Borrowed rules; tags must be unique. */
    const tlv_structure_rule_t* rules;
    /** Number of rules. */
    size_t count;
    /** Nonzero accepts tags that match no rule. */
    int allow_unknown;
    /** Borrowed table of alternative groups referenced by `rules[*].group`; may be `NULL` only
     * when `group_count` is zero. */
    const tlv_structure_group_t* groups;
    /** Number of entries in `groups`. */
    size_t group_count;
    /** Ordering required among this scope's matched elements. */
    tlv_schema_order_t order;
} tlv_structure_schema_t;

/** @brief Unknown-tag policy applied by tlv_schema_validate_all_diag(). */
typedef enum tlv_schema_unknown_policy {
    /** Each scope follows its own #tlv_structure_schema_t::allow_unknown. */
    TLV_SCHEMA_UNKNOWN_BY_SCHEMA = 0,
    /** Tags without a rule are accepted in every scope. */
    TLV_SCHEMA_UNKNOWN_ALLOW,
    /** Tags without a rule are reported in every scope. */
    TLV_SCHEMA_UNKNOWN_REJECT
} tlv_schema_unknown_policy_t;

/** @brief Layer whose typed detail is active in #tlv_schema_diagnostic_t::detail. */
typedef enum tlv_schema_cause {
    TLV_SCHEMA_CAUSE_NONE,   /**< No typed detail, for example an argument or resource failure. */
    TLV_SCHEMA_CAUSE_SCHEMA, /**< A Schema finding or definition failure; `detail.schema`. */
    TLV_SCHEMA_CAUSE_READER, /**< The Reader rejected the input; `detail.causes`. */
    TLV_SCHEMA_CAUSE_CODEC   /**< A Value conversion rejected the input; `detail.causes`. */
} tlv_schema_cause_t;

/**
 * @brief Returns the diagnostic name of a Schema cause.
 *
 * @param[in] value Diagnostic enum value.
 * @return A static NUL-terminated string, or `"unknown"` for an unrecognized value.
 * @note Never returns NULL or allocates. Do not free or modify the string.
 * Available even when the corresponding capability is disabled.
 */
OPENTLV_SCHEMA_API const char* tlv_schema_cause_string(tlv_schema_cause_t value);

/**
 * @brief Common failure diagnostic paired with one discriminated typed detail.
 *
 * Shared by validation and definition checks. The common base carries the
 * code, severity, primary location, expected text and enclosing path. `cause`
 * selects the active `detail` member: the Schema finding that explains the
 * violation, the original Reader detail when the wire input itself is
 * malformed, or producer-owned cause frames when a builtin rejects a Value it interprets,
 * such as a non-canonical DER INTEGER. The common base owns its bounded path value, so copies
 * retain valid external Tag borrows without self-references.
 *
 * @note Report entries are always Schema findings with cause #TLV_SCHEMA_CAUSE_SCHEMA.
 * @see tlv_schema_diagnostic_init
 */
typedef struct tlv_schema_diagnostic {
    /** Common result, severity, primary location and enclosing path. */
    tlv_diagnostic_t diagnostic;
    /** Active member of detail. */
    tlv_schema_cause_t cause;
    /** @brief Discriminated typed detail; only the member selected by cause is active. */
    union {
        /** Violated rule, affected tag and expected-versus-actual evidence. */
        tlv_schema_detail_t schema;
        /** Delegated conversion detail, outermost first; copy out through the producer API.
         * Active for TLV_SCHEMA_CAUSE_READER and TLV_SCHEMA_CAUSE_CODEC. Primary metadata stays in
         * diagnostic. */
        tlv_diagnostic_causes_t causes;
    } detail;
} tlv_schema_diagnostic_t;

/**
 * @brief Resets a schema diagnostic to all-unset.
 *
 * @param[out] diagnostic Diagnostic to initialize; must not be `NULL`.
 */
OPENTLV_SCHEMA_API void tlv_schema_diagnostic_init(tlv_schema_diagnostic_t* diagnostic);

/**
 * @brief Checks every reachable structural schema table independently of input.
 * Shared and recursive references are supported; every distinct table is checked once.
 * The worklist holds at most #TLV_SCHEMA_MAX_TABLES identities. Pointer lookup is linear
 * in the number of discovered tables, in addition to each table's local rule/group checks.
 * Definition graph depth is independent of the input nesting limit.
 * @param schema Borrowed schema graph.
 * @param diagnostic Optional failure detail; definition errors have unknown byte location.
 * @return #TLV_OK, #TLV_ERR_NULL_ARG, #TLV_ERR_INVALID_SCHEMA, or
 * #TLV_ERR_UNSUPPORTED when the definition exceeds bounded checking capacity.
 */
OPENTLV_SCHEMA_API tlv_result_t tlv_schema_check(const tlv_structure_schema_t* schema,
                                                 tlv_schema_diagnostic_t* diagnostic);

/**
 * @brief A structural schema whose definition has passed tlv_schema_check().
 *
 * Lets repeated validation against one immutable schema skip the per-call
 * definition check. Prepare it with tlv_schema_prepare() and use it with
 * tlv_schema_validate_checked() or tlv_schema_validate_all_checked(), which
 * run the same validation as tlv_schema_validate() and
 * tlv_schema_validate_all_diag(); runtime limits are still checked on every call.
 *
 * The handle borrows the schema graph. Every reachable table, rule, entry and
 * group must stay alive and unchanged while the handle is used. The handle
 * cannot detect mutation; prepare it again after changing the definition.
 * It holds no other state and may be copied.
 *
 * @see tlv_schema_prepare
 */
typedef struct tlv_schema_checked {
    /** Borrowed checked schema, or `NULL` when not prepared. Set by tlv_schema_prepare(). */
    const tlv_structure_schema_t* schema;
} tlv_schema_checked_t;

/**
 * @brief Checks a schema definition once and prepares a handle for repeated validation.
 *
 * Runs tlv_schema_check() on `schema`. On success, `checked` borrows `schema`;
 * on any failure it is reset to unprepared, so a handle is never left
 * referring to a definition that did not pass the check.
 *
 * @param[out] checked    Handle to prepare; must not be `NULL`.
 * @param[in]  schema     Borrowed schema graph; see #tlv_schema_checked_t for
 *                        its lifetime and immutability requirements.
 * @param[out] diagnostic Optional failure detail, as for tlv_schema_check().
 *
 * @return #TLV_OK if the definition is valid and `checked` is prepared.
 * @return #TLV_ERR_NULL_ARG if `checked` or `schema` is `NULL`.
 * @return Any other error of tlv_schema_check().
 */
OPENTLV_SCHEMA_API tlv_result_t tlv_schema_prepare(tlv_schema_checked_t* checked,
                                                   const tlv_structure_schema_t* schema,
                                                   tlv_schema_diagnostic_t* diagnostic);

/**
 * @brief Validates framing, nesting, lengths, occurrence counts, ordering,
 * alternative groups and child membership.
 *
 * Never decodes values. All tables are borrowed and immutable during use.
 * Limits and offsets follow Tree Reader. Uses TLV_SCHEMA_MAX_DEPTH structural frames
 * and bounded schema-context storage
 * without allocation or recursion. Counts are checked by rescanning each
 * scope per rule and per group: O((rules + groups) * (rules + elements)) per
 * scope. Input and schema errors leave no partial application objects.
 *
 * A scope whose #tlv_structure_schema_t::order is #TLV_SCHEMA_ORDER_SEQUENCE
 * additionally requires matched elements to appear in the same relative
 * order as their rules are listed. A scope with a nonempty
 * #tlv_structure_schema_t::groups additionally requires, for each group, the
 * total occurrences of its member tags to be within the group's own
 * `min_occurs`/`max_occurs`, on top of each member's own per-rule bounds.
 *
 * `format->is_constructed` receives `format->context`; `NULL` treats values as
 * opaque.
 *
 * @param[in]  data          Encoded input.
 * @param[in]  size          Input size in bytes.
 * @param[in]  format        Reader format.
 * @param[in]  schema        Structural schema to validate against.
 * @param[in]  max_depth     Runtime nesting limit; actual depth is also bounded by
 * TLV_SCHEMA_MAX_DEPTH.
 * @param[in]  max_elements  Maximum total elements, as for tlv_tree_reader_visit().
 * @param[out] diagnostic Optional first failure, including kind and location anchor.
 * A schema violation carries the same tag, enclosing path, field name and
 * expected-versus-actual detail as the matching tlv_schema_validate_all_diag() entry.
 * Definition and argument failures have no byte location.
 *
 * @return #TLV_OK if the data conforms to the schema.
 * @return #TLV_ERR_INVALID_SCHEMA for an invalid definition, checked before input.
 * @return #TLV_ERR_SCHEMA for a valid schema rejecting input, including MISSING.
 * @return #TLV_ERR_NULL_ARG for a missing required pointer.
 * @return Any other error of tlv_tree_reader_visit().
 */
OPENTLV_SCHEMA_API tlv_result_t tlv_schema_validate(const uint8_t* data, size_t size,
                                                    const tlv_format_t* format,
                                                    const tlv_structure_schema_t* schema,
                                                    size_t max_depth, size_t max_elements,
                                                    tlv_schema_diagnostic_t* diagnostic);

/**
 * @brief Validates input against a prepared schema without repeating its definition check.
 *
 * Behaves exactly like tlv_schema_validate() with `checked->schema`, except
 * that the definition check done by tlv_schema_prepare() is not repeated.
 * Never returns #TLV_ERR_INVALID_SCHEMA.
 *
 * @param[in]  checked      Prepared handle; its schema must be unchanged since preparation.
 * @param[in]  data         Encoded input.
 * @param[in]  size         Input size in bytes.
 * @param[in]  format       Reader format.
 * @param[in]  max_depth    Runtime nesting limit, as for tlv_schema_validate().
 * @param[in]  max_elements Maximum total elements, as for tlv_tree_reader_visit().
 * @param[out] diagnostic   Optional first failure, as for tlv_schema_validate().
 *
 * @return #TLV_OK if the data conforms to the schema.
 * @return #TLV_ERR_NULL_ARG for a missing required pointer.
 * @return #TLV_ERR_INVALID_STATE if `checked` is not prepared.
 * @return Any other error of tlv_schema_validate() except #TLV_ERR_INVALID_SCHEMA.
 */
OPENTLV_SCHEMA_API tlv_result_t tlv_schema_validate_checked(const tlv_schema_checked_t* checked,
                                                            const uint8_t* data, size_t size,
                                                            const tlv_format_t* format,
                                                            size_t max_depth, size_t max_elements,
                                                            tlv_schema_diagnostic_t* diagnostic);

/**
 * @brief Caller-provided storage for the violations of tlv_schema_validate_all_diag().
 *
 * @see tlv_schema_validate_all_diag
 */
typedef struct tlv_schema_diagnostic_report {
    /** Destination for the first `capacity` violations; may be `NULL` only if `capacity` is
     * zero. */
    tlv_schema_diagnostic_t* diagnostics;
    /** Number of entries `diagnostics` can hold. */
    size_t capacity;
    /** Receives the total number of violations found, which can exceed `capacity`. */
    size_t count;
} tlv_schema_diagnostic_report_t;

/**
 * @brief Validates a TLV structure and reports every schema violation.
 *
 * Applies the same rules as tlv_schema_validate() (required and optional
 * tags, occurrence limits, ordering, alternative groups, parent-child
 * membership, primitive or constructed form, and per-tag length bounds), but
 * continues after a violation and records each one with its enclosing scope path, affected tag,
 * field name, byte offset and expected-versus-actual detail. The tag and length rules come from the
 * same #tlv_structure_rule_t entries, so nothing is defined twice. Never
 * decodes values. No allocation and no recursion.
 *
 * Wire-level errors (a truncated or malformed element, or an exceeded limit)
 * make the input unparseable and abort the call before any violation is
 * recorded. The contents of a scope that violates its rules are still
 * checked: an element rejected as unexpected or of the wrong form is not
 * descended into, but its siblings are checked.
 *
 * The entire reachable definition is checked before reading input. Missing
 * findings use the enclosing scope end, with SCOPE_END anchor, including offset
 * zero at an empty root.
 *
 * Violations are recorded in scope order; the order of violations within one
 * scope is not part of the contract.
 *
 * @param[in]     data          Encoded input.
 * @param[in]     size          Input size in bytes.
 * @param[in]     format        Reader format.
 * @param[in]     schema        Structural schema to validate against.
 * @param[in]     max_depth     Runtime nesting limit; actual depth is also bounded by
 * TLV_SCHEMA_MAX_DEPTH.
 * @param[in]     max_elements  Maximum total elements, as for tlv_tree_reader_visit().
 * @param[in]     unknown       Policy for tags without a rule.
 * @param[in,out] report        Receives the violations; `count` is set on #TLV_OK
 *                              and #TLV_ERR_SCHEMA and is zero on other errors.
 * @param[out] diagnostic Optional fatal failure detail, including native definition
 *                        indices or an INPUT location relative to data. Individual
 *                        schema findings and their paths are stored in report.
 *
 * @return #TLV_OK if the data conforms; `report->count` is zero.
 * @return #TLV_ERR_SCHEMA if at least one violation was found; `report->count`
 *         is their total number, of which the first `report->capacity` are stored.
 * @return #TLV_ERR_NULL_ARG for missing required arguments.
 * @return #TLV_ERR_INVALID_SCHEMA for an invalid rule or group table.
 * @return #TLV_ERR_INVALID_ARG for an invalid `unknown` value.
 * @return #TLV_ERR_LIMIT if the schema nests deeper than #TLV_DIAGNOSTIC_PATH_MAX
 *         tags, or as for tlv_tree_reader_visit().
 * @return Any other error of tlv_tree_reader_visit().
 *
 * @see tlv_schema_diagnostic_t
 */
OPENTLV_SCHEMA_API tlv_result_t tlv_schema_validate_all_diag(const uint8_t* data, size_t size,
                                                             const tlv_format_t* format,
                                                             const tlv_structure_schema_t* schema,
                                                             size_t max_depth, size_t max_elements,
                                                             tlv_schema_unknown_policy_t unknown,
                                                             tlv_schema_diagnostic_report_t* report,
                                                             tlv_schema_diagnostic_t* diagnostic);

/**
 * @brief Reports every violation against a prepared schema without repeating its definition check.
 *
 * Behaves exactly like tlv_schema_validate_all_diag() with `checked->schema`,
 * except that the definition check done by tlv_schema_prepare() is not
 * repeated. Never returns #TLV_ERR_INVALID_SCHEMA.
 *
 * @param[in]     checked      Prepared handle; its schema must be unchanged since preparation.
 * @param[in]     data         Encoded input.
 * @param[in]     size         Input size in bytes.
 * @param[in]     format       Reader format.
 * @param[in]     max_depth    Runtime nesting limit, as for tlv_schema_validate_all_diag().
 * @param[in]     max_elements Maximum total elements, as for tlv_tree_reader_visit().
 * @param[in]     unknown      Policy for tags without a rule.
 * @param[in,out] report       Receives the violations, as for tlv_schema_validate_all_diag().
 * @param[out]    diagnostic   Optional fatal failure detail, as for
 *                             tlv_schema_validate_all_diag().
 *
 * @return #TLV_OK if the data conforms; `report->count` is zero.
 * @return #TLV_ERR_SCHEMA if at least one violation was found.
 * @return #TLV_ERR_NULL_ARG for missing required arguments.
 * @return #TLV_ERR_INVALID_STATE if `checked` is not prepared.
 * @return Any other error of tlv_schema_validate_all_diag() except #TLV_ERR_INVALID_SCHEMA.
 */
OPENTLV_SCHEMA_API tlv_result_t tlv_schema_validate_all_checked(
    const tlv_schema_checked_t* checked, const uint8_t* data, size_t size,
    const tlv_format_t* format, size_t max_depth, size_t max_elements,
    tlv_schema_unknown_policy_t unknown, tlv_schema_diagnostic_report_t* report,
    tlv_schema_diagnostic_t* diagnostic);

#ifdef __cplusplus
}
#endif

/** @} */

#endif /* OPENTLV_SCHEMA_H */
