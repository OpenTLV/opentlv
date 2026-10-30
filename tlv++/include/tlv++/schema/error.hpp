// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel
#ifndef OPENTLV_TLVPP_SCHEMA_ERROR_HPP
#define OPENTLV_TLVPP_SCHEMA_ERROR_HPP
#include "tlv++/error.hpp"
#include "tlv/schema/schema.h"
/** @file
 * @brief Schema-owned failure detail and projection to the common C++ error.
 */
namespace tlv {
/** @brief Typed schema definition kind diagnostic category. */
enum class schema_definition_kind {
    unknown = TLV_SCHEMA_DEFINITION_UNKNOWN /**< Canonical unknown category. */,
    table = TLV_SCHEMA_DEFINITION_TABLE /**< Canonical table category. */,
    rule = TLV_SCHEMA_DEFINITION_RULE /**< Canonical rule category. */,
    group = TLV_SCHEMA_DEFINITION_GROUP /**< Canonical group category. */,
    type = TLV_SCHEMA_DEFINITION_TYPE /**< Canonical type category. */,
    component = TLV_SCHEMA_DEFINITION_COMPONENT /**< Canonical component category. */
};
/** @brief Canonical static name, or unknown for unrecognized categories. */
inline const char* message(schema_definition_kind value) noexcept {
    return tlv_schema_definition_kind_string(static_cast<tlv_schema_definition_kind_t>(value));
}

/** @brief Category of structural Schema violation. */
enum class schema_issue {
    none = TLV_SCHEMA_ISSUE_NONE,             /**< No Schema reason. */
    value = TLV_SCHEMA_ISSUE_VALUE,           /**< Semantic Value constraint. */
    definition = TLV_SCHEMA_ISSUE_DEFINITION, /**< Malformed definition. */
    missing = TLV_SCHEMA_ISSUE_MISSING,       /**< Required field or group absent. */
    duplicate = TLV_SCHEMA_ISSUE_DUPLICATE,   /**< Too many occurrences. */
    unexpected = TLV_SCHEMA_ISSUE_UNEXPECTED, /**< Unknown or forbidden identifier. */
    kind = TLV_SCHEMA_ISSUE_KIND,             /**< Primitive/constructed mismatch. */
    length = TLV_SCHEMA_ISSUE_LENGTH,         /**< Invalid Value length. */
    order = TLV_SCHEMA_ISSUE_ORDER,           /**< Invalid sibling order. */
    assertion = TLV_SCHEMA_ISSUE_ASSERTION    /**< Query assertion failed. */
};
/** @brief Allocation-free Schema failure retaining complete native evidence.
 * External tags, field names, definition objects and cause descriptors remain borrowed.
 * The common error projection retains status, location, path and expected/actual text.
 */
class schema_failure : public error {
public:
    /** @brief Copy native evidence; its external borrows must outlive this object. */
    explicit schema_failure(const tlv_schema_diagnostic_t& value) noexcept
        : error(project(value)), raw_(value) {}
    /** @brief Translate a preflight result without additional evidence. */
    static schema_failure from_c(tlv_result_t rc) noexcept {
        tlv_schema_diagnostic_t value{};
        tlv_diagnostic_init(&value.diagnostic, rc, TLV_DIAGNOSTIC_SEVERITY_ERROR);
        return schema_failure(value);
    }
    /** @brief Common diagnostic projection. */
    tlv::error failure() const noexcept {
        return *this;
    }
    /** @brief Full copied Schema evidence, including delegated producer frames. */
    const tlv_schema_diagnostic_t& native() const noexcept {
        return raw_;
    }
    /** @brief Typed Schema reason, or none for other failures. */
    schema_issue schema_kind() const noexcept {
        return raw_.cause == TLV_SCHEMA_CAUSE_SCHEMA
                   ? static_cast<schema_issue>(raw_.detail.schema.kind)
                   : schema_issue::none;
    }
    /** @brief Native definition coordinate; owner borrows the supplied schema/type. */
    // Return an independent snapshot, including when called on a temporary error.
    // cppcheck-suppress returnByReference
    tlv_schema_definition_location_t definition() const noexcept {
        return raw_.cause == TLV_SCHEMA_CAUSE_SCHEMA ? raw_.detail.schema.definition
                                                     : tlv_schema_definition_location_t{};
    }
    /** @brief Typed category of the identified definition object. */
    schema_definition_kind definition_kind() const noexcept {
        return static_cast<schema_definition_kind>(definition().kind);
    }

private:
    static tlv::error project(const tlv_schema_diagnostic_t& value) noexcept {
        const tlv_tag_t*    tag = nullptr;
        tlv_reader_detail_t reader{};
        if (value.cause == TLV_SCHEMA_CAUSE_SCHEMA && value.detail.schema.tag.size)
            tag = &value.detail.schema.tag;
        else if (value.cause == TLV_SCHEMA_CAUSE_READER && value.detail.causes.count &&
                 tlv_reader_detail_copy(&value.detail.causes.frames[0], &reader) == TLV_OK &&
                 reader.has_tag)
            tag = &reader.tag;
        return detail::error_access::diagnostic(value.diagnostic, operation::schema, tag,
                                                &value.diagnostic.path);
    }
    tlv_schema_diagnostic_t raw_;
};
} // namespace tlv
#endif
