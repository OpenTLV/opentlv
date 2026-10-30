// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel
#include "diagnostic_assertions.h"
#include "tlv/schema/query_document.h"
#include "tlv/query/plan.h"
#include "controlled_format.h"
#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <memory>
namespace {
struct Program {
    std::vector<uint64_t>    storage;
    tlv_query_program_info_t info{};
    tlv_query_diagnostic_t   diagnostic{};
    tlv_result_t             compile(const std::string&                 text,
                                     const tlv_query_compile_options_t* options = nullptr) {
        info.struct_size = sizeof info;
        size_t size, alignment;
        auto rc = TLV_DIAGNOSTIC_RESULT(diagnostic,
                                        tlv_query_compile_scratch(text.data(), text.size(), options,
                                                                  &size, &alignment, &diagnostic));
        if (rc != TLV_OK) return rc;
        std::vector<uint64_t> scratch((size + 7) / 8);
        rc = TLV_DIAGNOSTIC_RESULT(diagnostic, tlv_query_compile(text.data(), text.size(), options,
                                                                 scratch.data(), size, nullptr, 0,
                                                                 &info, &diagnostic));
        if (rc != TLV_OK) return rc;
        storage.resize((info.program_size + 7) / 8);
        return TLV_DIAGNOSTIC_RESULT(
            diagnostic, tlv_query_compile(text.data(), text.size(), options, scratch.data(), size,
                                          storage.data(), info.program_size, &info, &diagnostic));
    }
    const tlv_query_program_t* get() const {
        return reinterpret_cast<const tlv_query_program_t*>(storage.data());
    }
};
} // namespace

TEST(Unit_Tlv_QueryProgram, SchemaRuleImagesTypesAndAlignmentRemainDistinct) {
    Program selector, assertion, number;
    ASSERT_EQ(TLV_OK, selector.compile("//5A"));
    ASSERT_EQ(TLV_OK, assertion.compile("exists(//5A)"));
    ASSERT_EQ(TLV_OK, number.compile("count(//5A)"));
#if OPENTLV_DOCUMENT
    tlv_document_t*        raw = nullptr;
    tlv_document_options_t options;
    ASSERT_EQ(TLV_OK, tlv_document_options_init(&options, &controlled::format));
    ASSERT_EQ(TLV_OK, tlv_document_create(&options, &raw));
    std::unique_ptr<tlv_document_t, decltype(&tlv_document_free)> document(raw, tlv_document_free);
#endif
    for (bool context : {false, true}) {
        for (int defect = 0; defect < 3; ++defect) {
            SCOPED_TRACE(defect);
            auto  damaged = context ? selector.storage : assertion.storage;
            auto* image = reinterpret_cast<tlv_query_program_t*>(damaged.data());
            const tlv_query_program_t* invalid = image;
            if (defect == 0)
                invalid = reinterpret_cast<const tlv_query_program_t*>(
                    reinterpret_cast<const char*>(image) + 1);
            else if (defect == 1)
                image->magic = 0;
            else
                invalid = number.get();
            tlv_schema_query_rule_t rules[] = {{selector.get(), assertion.get(), nullptr, "valid"},
                                               {context ? invalid : selector.get(),
                                                context ? assertion.get() : invalid, nullptr,
                                                "invalid"}};
            const auto expected = defect == 0 ? TLV_ERR_INVALID_ARG : TLV_ERR_INVALID_VALUE;
            size_t     a = 777, b = 777, alignment = 777;
            EXPECT_EQ(expected, tlv_schema_query_size(rules, 2, 4, 8, &a, &b, &alignment));
            EXPECT_EQ(777u, a);
            EXPECT_EQ(777u, b);
            EXPECT_EQ(777u, alignment);
            tlv_schema_query_workspace_t workspace{};
            for (bool detailed : {false, true}) {
                tlv_schema_query_diagnostic_t diagnostic{};
                diagnostic.rule = 777;
                auto check = [&] {
                    if (!detailed) return;
                    if (defect == 0)
                        EXPECT_EQ(777u, diagnostic.rule);
                    else {
                        EXPECT_EQ(1u, diagnostic.rule);
                        EXPECT_EQ(defect == 1 ? TLV_QUERY_ERROR_IMAGE : TLV_QUERY_ERROR_TYPE,
                                  diagnostic.query.kind);
                    }
                };
                EXPECT_EQ(expected, tlv_schema_query_validate_buffer(
                                        nullptr, 0, &controlled::format, rules, 2, 4, 8, 1000,
                                        &workspace, detailed ? &diagnostic : nullptr));
                check();
#if OPENTLV_DOCUMENT
                diagnostic = {};
                diagnostic.rule = 777;
                EXPECT_EQ(expected, tlv_schema_query_validate_document(
                                        document.get(), rules, 2, 4, 8, 1000, &workspace, nullptr,
                                        0, nullptr, detailed ? &diagnostic : nullptr));
                check();
#endif
            }
        }
    }
}

TEST(Unit_Tlv_QueryProgram, DeepSchemaPathsMatchBetweenBufferAndDocument) {
    Program contexts, condition;
    ASSERT_EQ(TLV_OK, contexts.compile("//50"));
    ASSERT_EQ(TLV_OK, condition.compile("exists(5A)"));
    auto nested_format = controlled::format;
    nested_format.is_constructed = [](const void*, const tlv_tag_t* tag) {
        return tag->data[0] >= 0x80 ? 1 : 0;
    };
    tlv_schema_query_rule_t rule{contexts.get(), condition.get(), nullptr, "deep"};
    size_t                  a, b, alignment;
    ASSERT_EQ(TLV_OK, tlv_schema_query_size(&rule, 1, 64, 100, &a, &b, &alignment));
    std::vector<uint8_t> selector_storage(a + alignment), assertion_storage(b + alignment);
    auto                 aligned = [alignment](std::vector<uint8_t>& storage) {
        return reinterpret_cast<void*>(
            (reinterpret_cast<uintptr_t>(storage.data()) + alignment - 1) & ~(alignment - 1));
    };
    tlv_schema_query_context_t   selected[1]{};
    tlv_tree_frame_t             frames[64]{};
    tlv_schema_query_workspace_t workspace{
        aligned(selector_storage), a, aligned(assertion_storage), b, selected, 1, frames, 64};
    for (size_t depth : {31u, 32u, 33u, 40u}) {
        SCOPED_TRACE(depth);
        std::vector<uint8_t> wire = {0x50, 0};
        for (size_t i = depth; i-- > 0;)
            wire.insert(wire.begin(),
                        {static_cast<uint8_t>(0x80 + i), static_cast<uint8_t>(wire.size())});
        tlv_schema_query_diagnostic_t diagnostic{};
        ASSERT_EQ(TLV_ERR_SCHEMA,
                  tlv_schema_query_validate_buffer(wire.data(), wire.size(), &nested_format, &rule,
                                                   1, 64, 100, 1000000, &workspace, &diagnostic));
        const size_t retained = depth < 32 ? depth : 32;
        ASSERT_EQ(retained, diagnostic.query.diagnostic.path.length);
        EXPECT_EQ(depth - retained, diagnostic.query.diagnostic.path.omitted);
        for (size_t i = 0; i < retained; ++i)
            EXPECT_EQ(0x80 + i, diagnostic.query.diagnostic.path.tags[i].data[0]);
#if OPENTLV_DOCUMENT
        tlv_document_options_t options;
        ASSERT_EQ(TLV_OK, tlv_document_options_init(&options, &nested_format));
        options.max_depth = 64;
        tlv_document_t* raw = nullptr;
        ASSERT_EQ(TLV_OK, tlv_document_parse(wire.data(), wire.size(), &options, &raw, nullptr));
        std::unique_ptr<tlv_document_t, decltype(&tlv_document_free)> document(raw,
                                                                               tlv_document_free);
        tlv_schema_query_diagnostic_t                                 doc_diagnostic{};
        ASSERT_EQ(TLV_ERR_SCHEMA, tlv_schema_query_validate_document(
                                      document.get(), &rule, 1, 64, 100, 1000000, &workspace,
                                      nullptr, 0, nullptr, &doc_diagnostic));
        ASSERT_EQ(retained, doc_diagnostic.query.diagnostic.path.length);
        EXPECT_EQ(diagnostic.query.diagnostic.path.omitted,
                  doc_diagnostic.query.diagnostic.path.omitted);
        for (size_t i = 0; i < retained; ++i)
            EXPECT_TRUE(tlv_tag_equal(diagnostic.query.diagnostic.path.tags[i],
                                      doc_diagnostic.query.diagnostic.path.tags[i]));
#endif
    }
}
