// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel
#ifndef OPENTLV_TEST_QUERY_SET_OPERATIONS_H
#define OPENTLV_TEST_QUERY_SET_OPERATIONS_H

// Query set operations (|, intersect and except) are the optional feature
// OPENTLV_QUERY_SET_OPERATIONS. Without it the compiler rejects them with
// TLV_ERR_UNSUPPORTED and a capability diagnostic; scenarios that need them
// check that rejection instead of their results.

#include "opentlv/query/program.h"
#include <gtest/gtest.h>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

inline bool query_uses_set_operations(const std::string& text) {
    if (text.find('|') != std::string::npos) return true;
    const auto word = [](char c) {
        return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-';
    };
    for (const char* name : {"intersect", "except"}) {
        const size_t length = std::strlen(name);
        for (size_t at = text.find(name); at != std::string::npos; at = text.find(name, at + 1)) {
            const size_t end = at + length;
            if ((at == 0 || !word(text[at - 1])) && (end == text.size() || !word(text[end])))
                return true;
        }
    }
    return false;
}

inline ::testing::AssertionResult query_set_operations_rejected(const std::string& text) {
    size_t                 size = 0, alignment = 0;
    tlv_query_diagnostic_t diagnostic{};
    tlv_result_t           rc = tlv_query_compile_scratch(text.data(), text.size(), nullptr, &size,
                                                          &alignment, &diagnostic);
    if (rc == TLV_OK) {
        std::vector<uint64_t>    scratch(size / sizeof(uint64_t) + 1);
        tlv_query_program_info_t info{};
        info.struct_size = sizeof info;
        rc = tlv_query_compile(text.data(), text.size(), nullptr, scratch.data(), size, nullptr, 0,
                               &info, &diagnostic);
    }
    if (rc == TLV_ERR_UNSUPPORTED && diagnostic.kind == TLV_QUERY_ERROR_CAPABILITY)
        return ::testing::AssertionSuccess();
    return ::testing::AssertionFailure()
           << text << " compiled with result " << rc << " and diagnostic kind " << diagnostic.kind
           << " without set operations";
}

// True when text needs the disabled set operations; then checks the rejection.
inline bool query_set_operations_unavailable(const std::string& text) {
    if (tlv_config_query_set_operations() || !query_uses_set_operations(text)) return false;
    EXPECT_TRUE(query_set_operations_rejected(text));
    return true;
}

// Ends the current test after the rejection check when text needs the disabled
// set operations. It is not skipped: the distribution gates require every
// GoogleTest scenario to run and complete.
#define OPENTLV_END_WITHOUT_SET_OPERATIONS(text)                                                   \
    do {                                                                                           \
        if (query_set_operations_unavailable(text)) return;                                        \
    } while (0)

#endif
