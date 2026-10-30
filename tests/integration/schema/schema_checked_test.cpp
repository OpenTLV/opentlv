// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel
#include "schema_diagnostic_helpers.h"

#include "tlv/builtins/asn1/ber.h"
#include "tlv/reader/visitor.h"
#include "tlv/schema/schema.h"
#include <gtest/gtest.h>
#include <vector>

// A prepared handle skips only the definition check: results, diagnostics and
// runtime limits match the plain API for the same schema and input.

namespace {
using Wire = std::vector<uint8_t>;

const tlv_schema_entry_t   childFields[] = {{TLV_TAG(0x80), 1, 1, 0, "id", 0},
                                            {TLV_TAG(0x81), 0, 4, 0, "name", 0}};
const tlv_structure_rule_t childRules[] = {
    {&childFields[0], 1, 1, TLV_SCHEMA_PRIMITIVE, nullptr, 0},
    {&childFields[1], 0, 1, TLV_SCHEMA_PRIMITIVE, nullptr, 0}};
const tlv_structure_schema_t childSchema = {childRules, 2, 0,
                                            nullptr,    0, TLV_SCHEMA_ORDER_SEQUENCE};
const tlv_schema_entry_t     rootFields[] = {{TLV_TAG(0x30), 0, SIZE_MAX, 0, "record", 0}};
const tlv_structure_rule_t   rootRules[] = {
    {&rootFields[0], 1, SIZE_MAX, TLV_SCHEMA_CONSTRUCTED, &childSchema, 0}};
const tlv_structure_schema_t rootSchema = {rootRules, 1, 0, nullptr, 0, TLV_SCHEMA_ORDER_ANY};

// A rule whose length bounds are reversed is an invalid definition.
const tlv_schema_entry_t   invalidFields[] = {{TLV_TAG(0x80), 2, 1, 0, "id", 0}};
const tlv_structure_rule_t invalidRules[] = {
    {&invalidFields[0], 1, 1, TLV_SCHEMA_PRIMITIVE, nullptr, 0}};
const tlv_structure_schema_t invalidSchema = {invalidRules, 1, 0, nullptr, 0, TLV_SCHEMA_ORDER_ANY};

const Wire inputs[] = {
    {0x30, 0x05, 0x80, 0x01, 0x01, 0x81, 0x00},       // valid
    {},                                               // missing record
    {0x30, 0x02, 0x81, 0x00},                         // missing id
    {0x30, 0x06, 0x80, 0x02, 0x01, 0x02, 0x81, 0x00}, // id length
    {0x30, 0x05, 0x81, 0x00, 0x80, 0x01, 0x01},       // order
    {0x30, 0x05, 0x80, 0x01, 0x01, 0x82, 0x00},       // unexpected
    {0x30, 0x05, 0x80, 0x01},                         // truncated
};

const tlv_result_t results[] = {TLV_OK,         TLV_ERR_SCHEMA, TLV_ERR_SCHEMA,   TLV_ERR_SCHEMA,
                                TLV_ERR_SCHEMA, TLV_ERR_SCHEMA, TLV_ERR_TRUNCATED};

void expectSame(const tlv_schema_diagnostic_t& plain, const tlv_schema_diagnostic_t& checked) {
    EXPECT_EQ(plain.diagnostic.code, checked.diagnostic.code);
    EXPECT_EQ(plain.diagnostic.severity, checked.diagnostic.severity);
    EXPECT_EQ(plain.diagnostic.location.domain, checked.diagnostic.location.domain);
    EXPECT_EQ(plain.diagnostic.location.kind, checked.diagnostic.location.kind);
    EXPECT_EQ(plain.diagnostic.location.begin, checked.diagnostic.location.begin);
    EXPECT_EQ(plain.diagnostic.location.end, checked.diagnostic.location.end);
    ASSERT_EQ(plain.diagnostic.has_path, checked.diagnostic.has_path);
    if (plain.diagnostic.has_path) {
        ASSERT_EQ(plain.diagnostic.path.length, checked.diagnostic.path.length);
        EXPECT_EQ(plain.diagnostic.path.omitted, checked.diagnostic.path.omitted);
        for (size_t i = 0; i < plain.diagnostic.path.length; ++i)
            EXPECT_TRUE(
                tlv_tag_equal(plain.diagnostic.path.tags[i], checked.diagnostic.path.tags[i]));
    }
    ASSERT_EQ(plain.cause, checked.cause);
    switch (plain.cause) {
        case TLV_SCHEMA_CAUSE_SCHEMA:
            EXPECT_EQ(plain.detail.schema.kind, checked.detail.schema.kind);
            EXPECT_TRUE(tlv_tag_equal(plain.detail.schema.tag, checked.detail.schema.tag));
            EXPECT_EQ(plain.detail.schema.field, checked.detail.schema.field);
            EXPECT_EQ(plain.detail.schema.occurs, checked.detail.schema.occurs);
            EXPECT_EQ(plain.detail.schema.actual_length, checked.detail.schema.actual_length);
            break;
        case TLV_SCHEMA_CAUSE_READER: {
            const auto& a = schema_test_reader(&plain);
            const auto& b = schema_test_reader(&checked);
            EXPECT_EQ(a.operation, b.operation);
            ASSERT_EQ(a.has_tag, b.has_tag);
            if (a.has_tag) EXPECT_TRUE(tlv_tag_equal(a.tag, b.tag));
            ASSERT_EQ(a.has_tag_offset, b.has_tag_offset);
            if (a.has_tag_offset) EXPECT_EQ(a.tag_offset, b.tag_offset);
            ASSERT_EQ(a.has_length_offset, b.has_length_offset);
            if (a.has_length_offset) EXPECT_EQ(a.length_offset, b.length_offset);
            ASSERT_EQ(a.has_value_offset, b.has_value_offset);
            if (a.has_value_offset) EXPECT_EQ(a.value_offset, b.value_offset);
            ASSERT_EQ(a.has_declared_length, b.has_declared_length);
            if (a.has_declared_length) EXPECT_EQ(a.declared_length, b.declared_length);
            ASSERT_EQ(a.has_raw_length, b.has_raw_length);
            if (a.has_raw_length) {
                ASSERT_EQ(a.raw_length.size, b.raw_length.size);
                for (size_t i = 0; i < a.raw_length.size; ++i)
                    EXPECT_EQ(a.raw_length.data[i], b.raw_length.data[i]);
            }
            ASSERT_EQ(a.has_available, b.has_available);
            if (a.has_available) EXPECT_EQ(a.available, b.available);
            ASSERT_EQ(a.has_enclosing_end, b.has_enclosing_end);
            if (a.has_enclosing_end) EXPECT_EQ(a.enclosing_end, b.enclosing_end);
            ASSERT_EQ(a.has_required, b.has_required);
            if (a.has_required) EXPECT_EQ(a.required, b.required);
            break;
        }
        default:
            FAIL() << "Unexpected cause for these structural validation fixtures: " << plain.cause;
    }
}
} // namespace

TEST(Integration_Tlv_SchemaChecked, PrepareBorrowsOnlyAValidDefinition) {
    tlv_schema_checked_t    checked{};
    tlv_schema_diagnostic_t diagnostic;
    ASSERT_EQ(TLV_OK, tlv_schema_prepare(&checked, &rootSchema, &diagnostic));
    EXPECT_EQ(&rootSchema, checked.schema);

    // A failed preparation never leaves the handle on the previous definition.
    EXPECT_EQ(TLV_ERR_INVALID_SCHEMA, tlv_schema_prepare(&checked, &invalidSchema, &diagnostic));
    EXPECT_EQ(nullptr, checked.schema);
    EXPECT_EQ(TLV_SCHEMA_ISSUE_DEFINITION, diagnostic.detail.schema.kind);
    EXPECT_EQ(TLV_ERR_INVALID_SCHEMA, diagnostic.diagnostic.code);

    ASSERT_EQ(TLV_OK, tlv_schema_prepare(&checked, &rootSchema, nullptr));
    EXPECT_EQ(TLV_ERR_NULL_ARG, tlv_schema_prepare(&checked, nullptr, &diagnostic));
    EXPECT_EQ(nullptr, checked.schema);
    EXPECT_EQ(TLV_ERR_NULL_ARG, tlv_schema_prepare(nullptr, &rootSchema, &diagnostic));
    EXPECT_EQ(TLV_ERR_NULL_ARG, diagnostic.diagnostic.code);
}

TEST(Integration_Tlv_SchemaChecked, ValidateMatchesPlainValidation) {
    tlv_schema_checked_t checked;
    ASSERT_EQ(TLV_OK, tlv_schema_prepare(&checked, &rootSchema, nullptr));
    for (size_t i = 0; i < sizeof inputs / sizeof inputs[0]; ++i) {
        SCOPED_TRACE(i);
        const Wire&             wire = inputs[i];
        tlv_schema_diagnostic_t plain, prepared;
        const tlv_result_t expected = tlv_schema_validate(wire.data(), wire.size(), &tlv_format_ber,
                                                          &rootSchema, 8, 100, &plain);
        ASSERT_EQ(results[i], expected);
        if (expected != TLV_OK)
            ASSERT_EQ(expected == TLV_ERR_SCHEMA ? TLV_SCHEMA_CAUSE_SCHEMA
                                                 : TLV_SCHEMA_CAUSE_READER,
                      plain.cause);
        ASSERT_EQ(expected, tlv_schema_validate_checked(&checked, wire.data(), wire.size(),
                                                        &tlv_format_ber, 8, 100, &prepared));
        if (expected != TLV_OK) expectSame(plain, prepared);
    }
}

TEST(Integration_Tlv_SchemaChecked, ReportMatchesPlainReport) {
    tlv_schema_checked_t checked;
    ASSERT_EQ(TLV_OK, tlv_schema_prepare(&checked, &rootSchema, nullptr));
    for (size_t i = 0; i < sizeof inputs / sizeof inputs[0]; ++i) {
        SCOPED_TRACE(i);
        const Wire&                    wire = inputs[i];
        tlv_schema_diagnostic_t        plainEntries[4], preparedEntries[4];
        tlv_schema_diagnostic_t        plainFailure, preparedFailure;
        tlv_schema_diagnostic_report_t plain = {plainEntries, 4, 99};
        tlv_schema_diagnostic_report_t prepared = {preparedEntries, 4, 99};
        const tlv_result_t             expected =
            tlv_schema_validate_all_diag(wire.data(), wire.size(), &tlv_format_ber, &rootSchema, 8,
                                         100, TLV_SCHEMA_UNKNOWN_BY_SCHEMA, &plain, &plainFailure);
        ASSERT_EQ(results[i], expected);
        if (expected == TLV_ERR_TRUNCATED) ASSERT_EQ(TLV_SCHEMA_CAUSE_READER, plainFailure.cause);
        ASSERT_EQ(expected, tlv_schema_validate_all_checked(
                                &checked, wire.data(), wire.size(), &tlv_format_ber, 8, 100,
                                TLV_SCHEMA_UNKNOWN_BY_SCHEMA, &prepared, &preparedFailure));
        ASSERT_EQ(plain.count, prepared.count);
        for (size_t j = 0; j < plain.count && j < 4; ++j)
            expectSame(plainEntries[j], preparedEntries[j]);
        if (expected != TLV_OK && expected != TLV_ERR_SCHEMA)
            expectSame(plainFailure, preparedFailure);
    }
}

TEST(Integration_Tlv_SchemaChecked, RuntimeLimitsAreCheckedOnEveryCall) {
    const Wire&                    wire = inputs[0];
    tlv_schema_checked_t           checked;
    tlv_schema_diagnostic_t        entries[1];
    tlv_schema_diagnostic_report_t report = {entries, 1, 99};
    ASSERT_EQ(TLV_OK, tlv_schema_prepare(&checked, &rootSchema, nullptr));
    EXPECT_EQ(TLV_ERR_LIMIT, tlv_schema_validate_checked(&checked, wire.data(), wire.size(),
                                                         &tlv_format_ber, 0, 100, nullptr));
    EXPECT_EQ(TLV_ERR_LIMIT, tlv_schema_validate_checked(&checked, wire.data(), wire.size(),
                                                         &tlv_format_ber, 8, 1, nullptr));
    EXPECT_EQ(TLV_ERR_LIMIT, tlv_schema_validate_all_checked(
                                 &checked, wire.data(), wire.size(), &tlv_format_ber, 8, 1,
                                 TLV_SCHEMA_UNKNOWN_BY_SCHEMA, &report, nullptr));
    EXPECT_EQ(0u, report.count);
}

TEST(Integration_Tlv_SchemaChecked, RejectsMissingArgumentsAndUnpreparedHandles) {
    const Wire&                    wire = inputs[0];
    tlv_schema_checked_t           checked{};
    tlv_schema_diagnostic_t        diagnostic;
    tlv_schema_diagnostic_t        entries[1];
    tlv_schema_diagnostic_report_t report = {entries, 1, 99};
    EXPECT_EQ(TLV_ERR_INVALID_STATE,
              tlv_schema_validate_checked(&checked, wire.data(), wire.size(), &tlv_format_ber, 8,
                                          100, &diagnostic));
    EXPECT_EQ(TLV_ERR_INVALID_STATE, diagnostic.diagnostic.code);
    EXPECT_EQ(TLV_DIAGNOSTIC_SEVERITY_ERROR, diagnostic.diagnostic.severity);
    EXPECT_EQ(TLV_ERR_INVALID_STATE, tlv_schema_validate_all_checked(
                                         &checked, wire.data(), wire.size(), &tlv_format_ber, 8,
                                         100, TLV_SCHEMA_UNKNOWN_BY_SCHEMA, &report, nullptr));
    EXPECT_EQ(0u, report.count);

    ASSERT_EQ(TLV_OK, tlv_schema_prepare(&checked, &rootSchema, nullptr));
    EXPECT_EQ(TLV_ERR_NULL_ARG, tlv_schema_validate_checked(nullptr, wire.data(), wire.size(),
                                                            &tlv_format_ber, 8, 100, &diagnostic));
    EXPECT_EQ(TLV_ERR_NULL_ARG, diagnostic.diagnostic.code);
    EXPECT_EQ(TLV_ERR_NULL_ARG,
              tlv_schema_validate_checked(&checked, nullptr, 1, &tlv_format_ber, 8, 100, nullptr));
    EXPECT_EQ(TLV_ERR_NULL_ARG, tlv_schema_validate_checked(&checked, wire.data(), wire.size(),
                                                            nullptr, 8, 100, nullptr));
    EXPECT_EQ(TLV_ERR_NULL_ARG, tlv_schema_validate_all_checked(
                                    &checked, wire.data(), wire.size(), &tlv_format_ber, 8, 100,
                                    TLV_SCHEMA_UNKNOWN_BY_SCHEMA, nullptr, nullptr));
    EXPECT_EQ(TLV_ERR_INVALID_ARG,
              tlv_schema_validate_all_checked(&checked, wire.data(), wire.size(), &tlv_format_ber,
                                              8, 100, static_cast<tlv_schema_unknown_policy_t>(3),
                                              &report, nullptr));
}
