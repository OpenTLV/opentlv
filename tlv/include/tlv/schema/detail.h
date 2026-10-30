// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#ifndef OPENTLV_SCHEMA_DETAIL_H
#define OPENTLV_SCHEMA_DETAIL_H

#include "tlv/tag.h"
#include "tlv/diagnostic/cause.h"
#include "tlv/schema/export.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 * @ingroup schemas
 * @brief Schema finding detail, shared by Schema, Codec and Query diagnostics.
 *
 * Included by tlv/schema/schema.h. Integrations attach and copy this producer-owned
 * leaf through the generic cause transport without embedding foreign detail types.
 */

/** @addtogroup schemas
 * @{
 */

/** @brief Expected form of a value described by a #tlv_structure_rule_t. */
typedef enum tlv_schema_kind {
    /** The element may be primitive or constructed. */
    TLV_SCHEMA_ANY = 0,
    /** The element must be primitive. */
    TLV_SCHEMA_PRIMITIVE,
    /** The element must be constructed. */
    TLV_SCHEMA_CONSTRUCTED
} tlv_schema_kind_t;

/** @brief Schema failure detail, independent of the common result. */
typedef enum tlv_schema_issue_kind {
    /** No Schema-specific detail (success or a delegated error). */
    TLV_SCHEMA_ISSUE_NONE = 0,
    /** A required tag is absent: its occurrence count is below `min_occurs`. */
    TLV_SCHEMA_ISSUE_MISSING = 1,
    /** A tag occurs more often than `max_occurs` allows. */
    TLV_SCHEMA_ISSUE_DUPLICATE,
    /** A tag is prohibited or unknown in its parent and unknown tags are rejected. */
    TLV_SCHEMA_ISSUE_UNEXPECTED,
    /** A primitive value where a constructed one is required, or the reverse. */
    TLV_SCHEMA_ISSUE_KIND,
    /** A value length violates the bounds or required multiple of the tag's rule. */
    TLV_SCHEMA_ISSUE_LENGTH,
    /** An element appears before an earlier-listed rule's element in a scope whose
     * #tlv_structure_schema_t::order is #TLV_SCHEMA_ORDER_SEQUENCE. */
    TLV_SCHEMA_ISSUE_ORDER,
    /** Contextual compiled Query boolean assertion failed. */
    TLV_SCHEMA_ISSUE_ASSERTION,
    /** Input Value violates a valid semantic constraint. */
    TLV_SCHEMA_ISSUE_VALUE,
    /** Invalid schema definition; byte location is unknown. */
    TLV_SCHEMA_ISSUE_DEFINITION
} tlv_schema_issue_kind_t;

/**
 * @brief Returns a short name for a violation kind.
 *
 * @param[in] kind Violation kind.
 *
 * @return A static, NUL-terminated string such as `"missing"` or `"order"`, never `NULL`;
 *         an unrecognized value yields `"unknown"`.
 * @note Provided by OpenTLV::Schema; never allocates.
 */
OPENTLV_SCHEMA_API const char* tlv_schema_issue_kind_string(tlv_schema_issue_kind_t kind);

/** @brief Native definition object identified by a Schema diagnostic. */
typedef enum tlv_schema_definition_kind {
    TLV_SCHEMA_DEFINITION_UNKNOWN = 0, /**< No definition object was identified. */
    TLV_SCHEMA_DEFINITION_TABLE,       /**< The owner is a structural Schema table. */
    TLV_SCHEMA_DEFINITION_RULE,        /**< Indexed rule in the owner Schema table. */
    TLV_SCHEMA_DEFINITION_GROUP,       /**< Indexed group in the owner Schema table. */
    TLV_SCHEMA_DEFINITION_TYPE,        /**< The owner is a DER Schema type. */
    TLV_SCHEMA_DEFINITION_COMPONENT    /**< Indexed component in the owner DER type. */
} tlv_schema_definition_kind_t;

/**
 * @brief Returns the diagnostic name of a schema definition kind.
 *
 * @param[in] value Diagnostic enum value.
 * @return A static NUL-terminated string, or `"unknown"` for an unrecognized value.
 * @note Never returns NULL or allocates. Do not free or modify the string.
 * Provided by OpenTLV::Schema.
 */
OPENTLV_SCHEMA_API const char*
tlv_schema_definition_kind_string(tlv_schema_definition_kind_t value);

/** @brief Native definition evidence, independent of wire/text byte locations.
 * @note The API determines the owner's type. For DER OF types, component index zero
 * denotes the element descriptor. The owner is borrowed from the supplied definition. */
typedef struct tlv_schema_definition_location {
    tlv_schema_definition_kind_t kind; /**< Active object kind, or UNKNOWN. */
    const void* owner; /**< Borrowed Schema table or DER type, valid for known kind. */
    size_t index;      /**< Rule/group/component index for indexed kinds. */
} tlv_schema_definition_location_t;

/**
 * @brief Schema-specific failure evidence without common diagnostic metadata.
 *
 * Identifies which rule or #tlv_structure_group_t a violation breaks, the tag
 * involved, the schema field or group name if it has one, and the
 * expected-versus-actual detail for whichever of `kind`'s cases applies. A
 * field not applicable to `kind` is left unset, indicated by its paired
 * `has_*` flag being zero. Every field is a fixed-size value or a borrowed
 * pointer, so filling one never allocates.
 *
 * #TLV_SCHEMA_ISSUE_MISSING and #TLV_SCHEMA_ISSUE_DUPLICATE can be reported
 * against a #tlv_structure_group_t instead of a single #tlv_structure_rule_t,
 * signaled by `is_group`: `tag` is then the group's first member rule's tag
 * (missing) or the actual offending member's tag (duplicate), `field` is the
 * group's `name`, and the occurrence fields are the group's own bounds and
 * summed occurrences rather than one rule's.
 *
 * Native definition failures identify the owning table/type and offending
 * member independently of byte locations.
 *
 * @note This fixed-size value contains no result, location or path. It is the
 * single Schema detail type: #tlv_schema_diagnostic_t pairs it with one common
 * #tlv_diagnostic_t, and integrations attach it as a delegated Schema cause frame.
 */
typedef struct tlv_schema_detail {
    /** Which rule was violated; see #tlv_schema_issue_kind_t. */
    tlv_schema_issue_kind_t kind;
    /** Affected tag; for #TLV_SCHEMA_ISSUE_MISSING, the tag that is absent. Borrows the input
     * buffer, immutable format identifier storage, or the schema for a missing tag. */
    tlv_tag_t tag;
    /** Faulty native definition object, or UNKNOWN for input violations. */
    tlv_schema_definition_location_t definition;
    /** Borrowed schema name for `tag` (the violated rule's `entry->name`, or the violated group's
     * `name` when `is_group` is nonzero), or `NULL` if it has none or no rule matched
     * (#TLV_SCHEMA_ISSUE_UNEXPECTED). */
    const char* field;
    /** Nonzero if this #TLV_SCHEMA_ISSUE_MISSING or #TLV_SCHEMA_ISSUE_DUPLICATE is reported
     * against a #tlv_structure_group_t rather than a single #tlv_structure_rule_t. */
    int is_group;
    /** Nonzero if `min_occurs`, `max_occurs` and `occurs` are set (#TLV_SCHEMA_ISSUE_MISSING,
     * #TLV_SCHEMA_ISSUE_DUPLICATE). */
    int has_occurs;
    /** Minimum permitted occurrences of `tag` in its parent, or of the group's members combined
     * when `is_group` is nonzero. */
    size_t min_occurs;
    /** Maximum permitted occurrences of `tag` in its parent, or of the group's members combined
     * when `is_group` is nonzero. */
    size_t max_occurs;
    /** Occurrences found so far at the point of the violation, of `tag` or, when `is_group` is
     * nonzero, of the group's members combined. */
    size_t occurs;
    /** Nonzero if `min_length`, `max_length`, `actual_length`, `length_multiple` and `length_flags`
     * are set (#TLV_SCHEMA_ISSUE_LENGTH). */
    int has_length;
    /** Minimum permitted value length in bytes, inclusive. */
    size_t min_length;
    /** Maximum permitted value length in bytes, inclusive. */
    size_t max_length;
    /** Actual value length in bytes. */
    size_t actual_length;
    /** Nonzero if `expected_form` and `actual_constructed` are set (#TLV_SCHEMA_ISSUE_KIND). */
    int has_form;
    /** Form the rule requires. */
    tlv_schema_kind_t expected_form;
    /** Nonzero if the actual value was constructed, zero if primitive. */
    int actual_constructed;
    /** Required value-length multiple for a length violation; 0 means unrestricted. */
    size_t length_multiple;
    /** Schema length-policy flags, including #TLV_SCHEMA_LENGTH_ENDPOINTS, when has_length. */
    uint32_t length_flags;
} tlv_schema_detail_t;

/** @} */

/** @brief Copy producer-owned schema detail into a generic cause chain.
 * Required pointers must be non-NULL; returns the Core append result. */
OPENTLV_SCHEMA_API tlv_result_t tlv_schema_detail_attach(tlv_diagnostic_causes_t* causes,
                                                         const tlv_schema_detail_t* detail);
/** @brief Copy matching schema detail out of a frame; mismatches leave output unchanged. */
OPENTLV_SCHEMA_API tlv_result_t tlv_schema_detail_copy(const tlv_diagnostic_cause_t* cause,
                                                       tlv_schema_detail_t* detail);

#ifdef __cplusplus
}
#endif

#endif /* OPENTLV_SCHEMA_DETAIL_H */
