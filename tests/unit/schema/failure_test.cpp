// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel
#include "schema_diagnostic_helpers.h"
#include "codec_diagnostic_helpers.h"

#include "tlv/schema/schema.h"
#include "tlv/schema/constraint.h"
#include "tlv/codec/structure.h"
#include "tlv/reader/visitor.h"
#include "controlled_format.h"
#include <gtest/gtest.h>

TEST(Unit_Tlv_SchemaFailure, DefinitionIsIndependentOfInputAndOptionalChildren) {
    const tlv_schema_entry_t     field = {TLV_TAG(1), 0, 8, 0, "parent", 0};
    const tlv_structure_schema_t invalid = {nullptr, 1, 0, nullptr, 0, TLV_SCHEMA_ORDER_ANY};
    const tlv_structure_rule_t   rule = {&field, 0, 1, TLV_SCHEMA_CONSTRUCTED, &invalid, 0};
    const tlv_structure_schema_t root = {&rule, 1, 0, nullptr, 0, TLV_SCHEMA_ORDER_ANY};
    tlv_schema_diagnostic_t      diagnostic{};
    EXPECT_EQ(TLV_ERR_INVALID_SCHEMA, tlv_schema_check(&root, &diagnostic));
    EXPECT_EQ(TLV_SCHEMA_CAUSE_SCHEMA, diagnostic.cause);
    EXPECT_EQ(TLV_SCHEMA_ISSUE_DEFINITION, diagnostic.detail.schema.kind);
    EXPECT_FALSE(diagnostic.diagnostic.location.kind);
    EXPECT_EQ(TLV_SCHEMA_DEFINITION_TABLE, diagnostic.detail.schema.definition.kind);
    EXPECT_EQ(&invalid, diagnostic.detail.schema.definition.owner);
    EXPECT_EQ(TLV_ERR_INVALID_SCHEMA,
              tlv_schema_validate(nullptr, 0, &controlled::format, &root, 8, 16, &diagnostic));
    EXPECT_EQ(TLV_ERR_INVALID_SCHEMA, diagnostic.diagnostic.code);
    const uint8_t malformed[] = {1};
    EXPECT_EQ(TLV_ERR_INVALID_SCHEMA,
              tlv_schema_validate(malformed, sizeof(malformed), &controlled::format, &root, 8, 16,
                                  nullptr));
    tlv_schema_diagnostic_report_t report = {&diagnostic, 1, 42};
    EXPECT_EQ(TLV_ERR_INVALID_SCHEMA,
              tlv_schema_validate_all_diag(nullptr, 0, &controlled::format, &root, 8, 16,
                                           TLV_SCHEMA_UNKNOWN_BY_SCHEMA, &report, nullptr));
    EXPECT_EQ(0u, report.count);
}

TEST(Unit_Tlv_SchemaFailure, MissingHasOneResultAndExplicitScopeEnd) {
    const tlv_schema_entry_t     field = {TLV_TAG(1), 0, 8, 0, "required", 0};
    const tlv_structure_rule_t   rule = {&field, 1, 1, TLV_SCHEMA_PRIMITIVE, nullptr, 0};
    const tlv_structure_schema_t schema = {&rule, 1, 0, nullptr, 0, TLV_SCHEMA_ORDER_ANY};
    tlv_schema_diagnostic_t      diagnostic{};
    EXPECT_EQ(TLV_ERR_SCHEMA,
              tlv_schema_validate(nullptr, 0, &controlled::format, &schema, 8, 16, &diagnostic));
    EXPECT_EQ(TLV_ERR_SCHEMA, diagnostic.diagnostic.code);
    EXPECT_EQ(TLV_SCHEMA_CAUSE_SCHEMA, diagnostic.cause);
    EXPECT_EQ(TLV_SCHEMA_ISSUE_MISSING, diagnostic.detail.schema.kind);
    EXPECT_EQ(TLV_LOCATION_SCOPE_END, diagnostic.diagnostic.location.kind);
    EXPECT_EQ(TLV_LOCATION_INPUT, diagnostic.diagnostic.location.domain);
    EXPECT_EQ(diagnostic.diagnostic.location.begin, diagnostic.diagnostic.location.end);
    ASSERT_TRUE(diagnostic.diagnostic.location.kind);
    EXPECT_EQ(0u, diagnostic.diagnostic.location.begin);
    EXPECT_TRUE(tlv_tag_equal(field.tag, diagnostic.detail.schema.tag));
    EXPECT_EQ(1u, diagnostic.detail.schema.min_occurs);
    EXPECT_EQ(0u, diagnostic.detail.schema.occurs);
    tlv_schema_diagnostic_report_t report = {&diagnostic, 1, 0};
    EXPECT_EQ(TLV_ERR_SCHEMA,
              tlv_schema_validate_all_diag(nullptr, 0, &controlled::format, &schema, 8, 16,
                                           TLV_SCHEMA_UNKNOWN_BY_SCHEMA, &report, nullptr));
    EXPECT_EQ(1u, report.count);
    EXPECT_EQ(TLV_SCHEMA_ISSUE_MISSING, diagnostic.detail.schema.kind);
    EXPECT_EQ(TLV_LOCATION_SCOPE_END, diagnostic.diagnostic.location.kind);
    EXPECT_TRUE(diagnostic.diagnostic.location.kind);
    EXPECT_EQ(TLV_ERR_SCHEMA,
              tlv_schema_validate(nullptr, 0, &controlled::format, &schema, 8, 16, nullptr));
}

TEST(Unit_Tlv_SchemaFailure, ConstraintDefinitionAndInputRemainDistinct) {
    const tlv_value_constraint_t reversed = {TLV_VALUE_CONSTRAINT_RANGE, 2, 1, nullptr, 0, nullptr};
    const tlv_value_constraint_t invalid = {
        TLV_VALUE_CONSTRAINT_ALLOWED_VALUES, 0, 0, nullptr, 1, nullptr};
    const tlv_value_constraint_t valid = {TLV_VALUE_CONSTRAINT_RANGE, 1, 2, nullptr, 0, nullptr};
    EXPECT_EQ(TLV_ERR_INVALID_SCHEMA, tlv_value_constraint_check(&reversed));
    EXPECT_EQ(TLV_ERR_INVALID_SCHEMA, tlv_value_constraint_validate(&invalid, 1));
    EXPECT_EQ(TLV_ERR_SCHEMA, tlv_value_constraint_validate(&valid, 3));
    EXPECT_EQ(TLV_OK, tlv_value_constraint_validate(&valid, 2));
    EXPECT_EQ(TLV_ERR_NULL_ARG, tlv_value_constraint_check(nullptr));
}

TEST(Unit_Tlv_SchemaFailure, RecursiveDefinitionsAndCapabilityBoundsAreDistinct) {
    const tlv_schema_entry_t field = {TLV_TAG(1), 0, SIZE_MAX, 0, "recursive", 0};
    tlv_structure_schema_t   schemas[TLV_SCHEMA_MAX_TABLES + 1]{};
    tlv_structure_rule_t     rules[TLV_SCHEMA_MAX_TABLES + 1]{};
    for (size_t i = 0; i < TLV_SCHEMA_MAX_TABLES + 1; ++i) {
        rules[i] = {&field,
                    0,
                    1,
                    TLV_SCHEMA_CONSTRUCTED,
                    i < TLV_SCHEMA_MAX_TABLES ? &schemas[i + 1] : nullptr,
                    0};
        schemas[i] = {&rules[i], 1, 0, nullptr, 0, TLV_SCHEMA_ORDER_ANY};
    }
    tlv_schema_diagnostic_t diagnostic{};
    EXPECT_EQ(TLV_ERR_UNSUPPORTED, tlv_schema_check(schemas, &diagnostic));
    EXPECT_EQ(TLV_ERR_UNSUPPORTED, diagnostic.diagnostic.code);
    EXPECT_EQ(TLV_SCHEMA_ISSUE_NONE, diagnostic.detail.schema.kind);
    EXPECT_FALSE(diagnostic.diagnostic.location.kind);
    rules[TLV_SCHEMA_MAX_TABLES - 1].children = nullptr;
    EXPECT_EQ(TLV_OK, tlv_schema_check(schemas, &diagnostic));
    rules[0].children = schemas;
    EXPECT_EQ(TLV_OK, tlv_schema_check(schemas, &diagnostic));
    EXPECT_EQ(TLV_OK, diagnostic.diagnostic.code);
    EXPECT_EQ(TLV_ERR_NULL_ARG, tlv_schema_check(nullptr, &diagnostic));
    EXPECT_EQ(TLV_SCHEMA_ISSUE_NONE, diagnostic.detail.schema.kind);
    EXPECT_FALSE(diagnostic.diagnostic.location.kind);
}

TEST(Unit_Tlv_SchemaFailure, SharedDagIsCheckedByIdentityIncludingUnvisitedDefinitions) {
    const tlv_schema_entry_t fields[] = {{TLV_TAG(1), 0, SIZE_MAX, 0, "a", 0},
                                         {TLV_TAG(2), 0, SIZE_MAX, 0, "b", 0}};
    tlv_structure_schema_t   tables[40]{};
    tlv_structure_rule_t     rules[40][2]{};
    for (size_t i = 0; i < 40; ++i) {
        tables[i] = {rules[i], 2, 0, nullptr, 0, TLV_SCHEMA_ORDER_ANY};
        for (size_t j = 0; j < 2; ++j)
            rules[i][j] = {
                &fields[j], 0, 1, TLV_SCHEMA_CONSTRUCTED, i + 1 < 40 ? &tables[i + 1] : nullptr, 0};
    }
    EXPECT_EQ(TLV_OK, tlv_schema_check(tables, nullptr));
    EXPECT_EQ(TLV_OK, tlv_schema_validate(nullptr, 0, &controlled::format, tables, 0, 0, nullptr));
    tlv_schema_diagnostic_report_t report{};
    EXPECT_EQ(TLV_OK, tlv_schema_validate_all_diag(nullptr, 0, &controlled::format, tables, 0, 0,
                                                   TLV_SCHEMA_UNKNOWN_BY_SCHEMA, &report, nullptr));
    rules[39][1].entry = nullptr;
    tlv_schema_diagnostic_t diagnostic{};
    EXPECT_EQ(TLV_ERR_INVALID_SCHEMA, tlv_schema_check(tables, &diagnostic));
    EXPECT_EQ(TLV_SCHEMA_DEFINITION_RULE, diagnostic.detail.schema.definition.kind);
    EXPECT_EQ(&tables[39], diagnostic.detail.schema.definition.owner);
    EXPECT_EQ(1u, diagnostic.detail.schema.definition.index);
    EXPECT_EQ(TLV_LOCATION_UNKNOWN, diagnostic.diagnostic.location.kind);
    EXPECT_EQ(TLV_ERR_INVALID_SCHEMA, tlv_schema_check(tables, nullptr));
}

TEST(Unit_Tlv_SchemaFailure, MalformedInputKeepsTheReaderCause) {
    // The declared length 5 exceeds the one available byte, so the Reader rejects the input
    // before any Schema rule applies.
    static const uint8_t         input[] = {1, 5, 0xAA};
    const tlv_schema_entry_t     field = {TLV_TAG(1), 0, 8, 0, "field", 0};
    const tlv_structure_rule_t   rule = {&field, 0, 1, TLV_SCHEMA_ANY, nullptr, 0};
    const tlv_structure_schema_t schema = {&rule, 1, 0, nullptr, 0, TLV_SCHEMA_ORDER_ANY};
    tlv_tree_frame_t             frames[TLV_SCHEMA_MAX_DEPTH];
    tlv_tree_reader_t            reader;
    tlv_reader_diagnostic_t      original;
    tlv_reader_diagnostic_init(&original);
    ASSERT_EQ(TLV_OK, tlv_tree_reader_init(&reader, input, sizeof input, &controlled::format,
                                           frames, TLV_SCHEMA_MAX_DEPTH, 8, 16));
    const tlv_result_t failure = tlv_tree_reader_visit(&reader, nullptr, nullptr, &original);
    ASSERT_NE(TLV_OK, failure);
    const auto same_reader = [&](const tlv_diagnostic_t& common, const tlv_reader_detail_t& d) {
        EXPECT_EQ(failure, common.code);
        EXPECT_EQ(original.diagnostic.location.kind, common.location.kind);
        EXPECT_EQ(original.diagnostic.location.begin, common.location.begin);
        EXPECT_EQ(original.detail.operation, d.operation);
        EXPECT_EQ(original.detail.has_declared_length, d.has_declared_length);
        EXPECT_EQ(original.detail.declared_length, d.declared_length);
        EXPECT_EQ(original.detail.has_available, d.has_available);
        EXPECT_EQ(original.detail.available, d.available);
    };

    tlv_schema_diagnostic_t diagnostic{};
    EXPECT_EQ(failure, tlv_schema_validate(input, sizeof input, &controlled::format, &schema, 8, 16,
                                           &diagnostic));
    ASSERT_EQ(TLV_SCHEMA_CAUSE_READER, diagnostic.cause);
    same_reader(diagnostic.diagnostic, schema_test_reader(&diagnostic));

    tlv_schema_diagnostic_t        entries[1]{};
    tlv_schema_diagnostic_report_t report = {entries, 1, 0};
    diagnostic = {};
    EXPECT_EQ(failure,
              tlv_schema_validate_all_diag(input, sizeof input, &controlled::format, &schema, 8, 16,
                                           TLV_SCHEMA_UNKNOWN_BY_SCHEMA, &report, &diagnostic));
    EXPECT_EQ(0u, report.count);
    ASSERT_EQ(TLV_SCHEMA_CAUSE_READER, diagnostic.cause);
    same_reader(diagnostic.diagnostic, schema_test_reader(&diagnostic));

    // A structure codec with a Schema reports the same failure as its own Reader cause.
    const tlv_structure_codec_t codec = {nullptr,
                                         &controlled::format,
                                         &schema,
                                         8,
                                         16,
                                         [](const void*, const tlv_format_t*, const uint8_t*,
                                            size_t, void*, size_t,
                                            tlv_codec_diagnostic_t*) { return TLV_OK; },
                                         nullptr};
    tlv_codec_diagnostic_t      codec_diagnostic;
    uint8_t                     value;
    EXPECT_EQ(failure, tlv_structure_decode(&codec, input, sizeof input, &value, sizeof value,
                                            &codec_diagnostic));
    ASSERT_EQ(TLV_CODEC_CAUSE_READER, codec_test_cause(&codec_diagnostic));
    same_reader(codec_diagnostic.diagnostic, codec_test_reader(&codec_diagnostic));
}
