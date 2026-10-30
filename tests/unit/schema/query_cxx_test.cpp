// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel
#include "diagnostic_assertions.h"
#include "query_diagnostic_helpers.h"
#include "tlv++/native.hpp"
#include "controlled_format.h"
#include "tlv++/query/program.hpp"
#include "tlv++/document/document.hpp"
#include "tlv/schema/query_document.h"
#include <gtest/gtest.h>
#include <vector>
#include <cstring>
namespace {
int constructed(const void*, const tlv_tag_t* tag) {
    return tag->size == 1 && (tag->data[0] == 0x6F || tag->data[0] == 0xA5);
}
tlv_format_t make_format() {
    auto result = controlled::format;
    result.is_constructed = constructed;
    return result;
}
const tlv_format_t format = make_format();
// A dead-end root followed by two matching branches and an unrelated root.
const uint8_t input[] = {0x6F, 2, 0x84, 0, 0x6F, 8, 0xA5, 6, 0x50, 1, 1, 0x50, 1, 2, 0x50, 1, 9};
tlv::bytes    bytes(const uint8_t* data, size_t size) {
    return {reinterpret_cast<const tlv::byte*>(data), size};
}
} // namespace

TEST(Unit_Tlvpp_FullQuery, ContextualSchemaAssertionsPreserveReaderFailures) {
    auto contexts = tlv::query_program::compile("//6F");
    ASSERT_TRUE(contexts);
    auto condition = tlv::query_program::compile("not(exists(.//50)) or exists(.//84)");
    ASSERT_TRUE(condition);
    tlv_schema_query_rule_t rule{tlv::native::handle(*contexts), tlv::native::handle(*condition),
                                 nullptr, "label-requires-name"};
    size_t                  a, b, alignment;
    ASSERT_EQ(TLV_OK, tlv_schema_query_size(&rule, 1, 3, 100, &a, &b, &alignment));
    tlv::detail::query_memory     selector(a), assertion(b);
    tlv_schema_query_context_t    selected[4]{};
    tlv_tree_frame_t              frames[4]{};
    tlv_schema_query_workspace_t  workspace{selector.data(), a, assertion.data(), b,
                                            selected,        4, frames,           4};
    tlv_schema_query_diagnostic_t diagnostic{};
    EXPECT_EQ(TLV_ERR_SCHEMA,
              TLV_DIAGNOSTIC_RESULT(diagnostic, tlv_schema_query_validate_buffer(
                                                    input, sizeof input, &format, &rule, 1, 3, 100,
                                                    100000, &workspace, &diagnostic)));
    EXPECT_EQ(0u, diagnostic.rule);
    EXPECT_STREQ("label-requires-name", query_test_schema(diagnostic.query).field);
    EXPECT_EQ(4u, diagnostic.query.diagnostic.location.begin);
    EXPECT_EQ(TLV_OK, tlv_schema_query_validate_buffer(input, 4, &format, &rule, 1, 3, 100, 100000,
                                                       &workspace, &diagnostic));
    std::vector<uint8_t> malformed(input, input + sizeof input);
    malformed.push_back(0x50);
    malformed.push_back(10);
    EXPECT_NE(TLV_ERR_SCHEMA,
              TLV_DIAGNOSTIC_RESULT(
                  diagnostic, tlv_schema_query_validate_buffer(malformed.data(), malformed.size(),
                                                               &format, &rule, 1, 3, 100, 100000,
                                                               &workspace, &diagnostic)));
    EXPECT_EQ(TLV_QUERY_ERROR_READER, diagnostic.query.kind);
    EXPECT_EQ(TLV_OK, tlv_schema_query_validate_buffer(nullptr, 0, nullptr, nullptr, 0, 0, 0, 0,
                                                       nullptr, nullptr));
    workspace.context_capacity = 1;
    EXPECT_EQ(TLV_ERR_LIMIT,
              TLV_DIAGNOSTIC_RESULT(diagnostic, tlv_schema_query_validate_buffer(
                                                    input, sizeof input, &format, &rule, 1, 3, 100,
                                                    100000, &workspace, &diagnostic)));
    EXPECT_STREQ("schema-contexts", diagnostic.query.limit);
    auto nested = tlv::query_program::compile("//A5");
    ASSERT_TRUE(nested);
    rule.context = tlv::native::handle(*nested);
    EXPECT_EQ(TLV_ERR_SCHEMA,
              TLV_DIAGNOSTIC_RESULT(diagnostic, tlv_schema_query_validate_buffer(
                                                    input, sizeof input, &format, &rule, 1, 3, 100,
                                                    100000, &workspace, &diagnostic)));
    ASSERT_EQ(1u, diagnostic.query.diagnostic.path.length);
    EXPECT_EQ(0x6F, diagnostic.query.diagnostic.path.tags[0].data[0]);
    EXPECT_STREQ("assertion",
                 tlv_schema_issue_kind_string(query_test_schema(diagnostic.query).kind));
    // A Reader workspace rejected before traversal is not a Reader input failure.
    auto* saved_frames = workspace.frames;
    workspace.frames = nullptr;
    EXPECT_EQ(TLV_ERR_NULL_ARG,
              TLV_DIAGNOSTIC_RESULT(diagnostic, tlv_schema_query_validate_buffer(
                                                    input, sizeof input, &format, &rule, 1, 3, 100,
                                                    100000, &workspace, &diagnostic)));
    EXPECT_EQ(TLV_QUERY_ERROR_ARGUMENT, diagnostic.query.kind);
    EXPECT_EQ(TLV_QUERY_CAUSE_NONE, diagnostic.query.cause);
    EXPECT_EQ(TLV_LOCATION_UNKNOWN, diagnostic.query.diagnostic.location.kind);
    workspace.frames = saved_frames;
}

TEST(Unit_Tlvpp_FullQuery, DocumentSchemaRunsTheSameContextualPrograms) {
    auto doc = tlv::document::parse(bytes(input, sizeof input),
                                    tlv::document_format(tlv::native::borrow_format(format)));
    ASSERT_TRUE(doc);
    auto contexts = tlv::query_program::compile("//6F");
    ASSERT_TRUE(contexts);
    auto condition = tlv::query_program::compile("not(exists(.//50)) or exists(.//84)");
    ASSERT_TRUE(condition);
    tlv_schema_query_rule_t rule{tlv::native::handle(*contexts), tlv::native::handle(*condition),
                                 nullptr, "name"};
    size_t                  a, b, alignment;
    ASSERT_EQ(TLV_OK, tlv_schema_query_size(&rule, 1, 3, 100, &a, &b, &alignment));
    tlv::detail::query_memory     selector(a), assertion(b);
    tlv_schema_query_context_t    selected[4]{};
    tlv_schema_query_workspace_t  w{selector.data(), a, assertion.data(), b,
                                    selected,        4, nullptr,          0};
    tlv_schema_query_diagnostic_t diagnostic{};
    EXPECT_EQ(TLV_ERR_SCHEMA,
              TLV_DIAGNOSTIC_RESULT(diagnostic, tlv_schema_query_validate_document(
                                                    tlv::native::handle(*doc), &rule, 1, 3, 100,
                                                    100000, &w, nullptr, 0, nullptr, &diagnostic)));
    EXPECT_EQ(TLV_SCHEMA_ISSUE_ASSERTION, query_test_schema(diagnostic.query).kind);
    EXPECT_STREQ("name", query_test_schema(diagnostic.query).field);
}
