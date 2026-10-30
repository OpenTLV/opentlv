// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel
#include "codec_diagnostic_helpers.h"
#include "tlv/schema/number.h"
#include <gtest/gtest.h>

TEST(Unit_Tlv_Schema, NumberCompositionSelectsSchemaWidthsWithoutCopyingPolicy) {
    const tlv_schema_entry_t  field = {TLV_TAG(1), 1, 3, TLV_SCHEMA_LENGTH_ENDPOINTS, nullptr, 0};
    const tlv_schema_number_t config = {&field, {TLV_NUMBER_BINARY_BE, 0, 0}};
    const auto                codec = tlv_schema_number_codec(&config);
    const uint64_t            number = 256;
    uint8_t                   wire[3] = {0xAA, 0xAA, 0xAA};
    size_t                    written = 99;
    ASSERT_EQ(TLV_OK, tlv_codec_encode(&codec, &number, sizeof(number), wire, sizeof(wire),
                                       &written, NULL));
    EXPECT_EQ(3u, written);
    EXPECT_EQ(0, wire[0]);
    EXPECT_EQ(1, wire[1]);
    EXPECT_EQ(0, wire[2]);
    uint64_t decoded = 99;
    EXPECT_EQ(TLV_ERR_SCHEMA,
              tlv_codec_decode(&codec, wire + 1, 2, &decoded, sizeof(decoded), NULL));
    EXPECT_EQ(99u, decoded);
    ASSERT_EQ(TLV_OK, tlv_codec_decode(&codec, wire, 3, &decoded, sizeof(decoded), NULL));
    EXPECT_EQ(number, decoded);
    EXPECT_EQ(TLV_ERR_BUFFER_TOO_SHORT,
              tlv_codec_encode(&codec, &number, sizeof(number), wire, 2, &written, NULL));
    EXPECT_EQ(0u, written);
    EXPECT_EQ(1, wire[1]);
    const tlv_schema_number_t fixed = {&field, {TLV_NUMBER_BINARY_BE, 2, 0}};
    EXPECT_EQ(TLV_ERR_SCHEMA, tlv_schema_number_encode(&fixed, &number, sizeof(number), nullptr, 0,
                                                       &written, NULL));
    EXPECT_EQ(0u, written);
    const tlv_schema_entry_t  impossible = {TLV_TAG(1), 9, 12, 0, nullptr, 0};
    const tlv_schema_number_t unsupported = {&impossible, {TLV_NUMBER_BINARY_BE, 0, 0}};
    EXPECT_EQ(TLV_ERR_SCHEMA, tlv_schema_number_encode(&unsupported, &number, sizeof(number),
                                                       nullptr, 0, &written, NULL));
    EXPECT_EQ(TLV_ERR_NULL_ARG,
              tlv_schema_number_decode(nullptr, wire, 3, &decoded, sizeof(decoded), NULL));
    EXPECT_EQ(TLV_ERR_NULL_ARG, tlv_schema_number_encode(nullptr, &number, sizeof(number), nullptr,
                                                         0, &written, NULL));
}

TEST(Unit_Tlv_Schema, NumberCompositionKeepsTheRepresentationReasonFromSizing) {
    const tlv_schema_entry_t  field = {TLV_TAG(1), 1, 3, 0, nullptr, 0};
    const tlv_schema_number_t two_digits = {&field, {TLV_NUMBER_BCD, 0, 2}};
    const uint64_t            number = 100;
    uint8_t                   wire[3];
    size_t                    written = 99;
    // Sizing rejects 100 for two BCD digits; the reason survives under the caller's operation.
    for (uint8_t* data : {static_cast<uint8_t*>(nullptr), wire}) {
        tlv_codec_diagnostic_t diagnostic;
        EXPECT_EQ(TLV_ERR_INVALID_VALUE,
                  tlv_schema_number_encode(&two_digits, &number, sizeof(number), data,
                                           data ? sizeof(wire) : 0, &written, &diagnostic));
        EXPECT_EQ(TLV_ERR_INVALID_VALUE, diagnostic.diagnostic.code);
        EXPECT_EQ(data ? TLV_CODEC_OP_ENCODE : TLV_CODEC_OP_MEASURE, diagnostic.codec.operation);
        EXPECT_EQ(TLV_CODEC_REASON_RANGE, diagnostic.codec.reason);
        EXPECT_EQ(0u, written);
    }
}

TEST(Unit_Tlv_Schema, NumberCompositionPreservesSchemaDiagnostic) {
    const tlv_schema_entry_t  field = {TLV_TAG(1), 3, 1, 0, "number", 0};
    const tlv_schema_number_t config = {&field, {TLV_NUMBER_BINARY_BE, 0, 0}};
    const uint8_t             wire[] = {1};
    uint64_t                  decoded = 99;
    tlv_codec_diagnostic_t    diagnostic{};
    EXPECT_EQ(TLV_ERR_INVALID_SCHEMA,
              tlv_schema_number_decode(&config, wire, 1, &decoded, sizeof decoded, &diagnostic));
    EXPECT_EQ(TLV_ERR_INVALID_SCHEMA, diagnostic.diagnostic.code);
    EXPECT_EQ(TLV_CODEC_CAUSE_SCHEMA, codec_test_cause(&diagnostic));
    EXPECT_EQ(TLV_SCHEMA_ISSUE_DEFINITION, codec_test_schema(&diagnostic).kind);
    EXPECT_STREQ("number", codec_test_schema(&diagnostic).field);
    EXPECT_EQ(99u, decoded);

    // Sizing succeeds, then the Schema rejects width 2: the caller's operation is kept.
    const tlv_schema_entry_t  endpoints = {TLV_TAG(1), 1, 3, TLV_SCHEMA_LENGTH_ENDPOINTS,
                                           nullptr,    0};
    const tlv_schema_number_t fixed = {&endpoints, {TLV_NUMBER_BINARY_BE, 2, 0}};
    const uint64_t            number = 1;
    uint8_t                   out[3];
    size_t                    written = 99;
    for (uint8_t* data : {static_cast<uint8_t*>(nullptr), out}) {
        EXPECT_EQ(TLV_ERR_SCHEMA,
                  tlv_schema_number_encode(&fixed, &number, sizeof number, data,
                                           data ? sizeof out : 0, &written, &diagnostic));
        EXPECT_EQ(data ? TLV_CODEC_OP_ENCODE : TLV_CODEC_OP_MEASURE, diagnostic.codec.operation);
        EXPECT_EQ(TLV_CODEC_CAUSE_SCHEMA, codec_test_cause(&diagnostic));
        EXPECT_EQ(TLV_SCHEMA_ISSUE_LENGTH, codec_test_schema(&diagnostic).kind);
        EXPECT_EQ(0u, written);
    }
}
