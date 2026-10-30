// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel
#include "tlv/codec/diagnostic.h"
#include <gtest/gtest.h>
#include <cstring>

namespace {
const tlv_diagnostic_domain_t foreign = {0x8001, "test.provider", nullptr, nullptr};
}

TEST(Unit_Tlv_CodecDiagnostic, PreservesForeignEvidenceAndCommonDetailsByValue) {
    tlv_codec_diagnostic_t diagnostic;
    tlv_codec_diagnostic_init(&diagnostic, TLV_CODEC_OP_ENCODE);
    diagnostic.codec.reported = 12345;
    diagnostic.codec.violation = TLV_CODEC_VIOLATION_RESULT;
    diagnostic.codec.representation = "object";
    diagnostic.diagnostic.expected = "expected object";
    diagnostic.diagnostic.actual = "actual object";
    tlv_location_set(&diagnostic.diagnostic.location, TLV_LOCATION_OUTPUT, TLV_LOCATION_SPAN, 4, 7);
    const uint64_t payload = 42;
    ASSERT_EQ(TLV_OK, tlv_diagnostic_causes_append(&diagnostic.causes, &foreign, 1, &payload,
                                                   sizeof payload));
    tlv_codec_diagnostic_result(&diagnostic, TLV_ERR_CALLBACK);
    tlv_diagnostic_envelope_t envelope;
    tlv_codec_diagnostic_export(&envelope, &diagnostic);
    const auto copied = envelope;
    std::memset(&diagnostic, 0, sizeof diagnostic);
    std::memset(&envelope, 0, sizeof envelope);
    ASSERT_EQ(2u, copied.causes.count);
    EXPECT_EQ(0u, copied.causes.omitted);
    EXPECT_EQ(TLV_ERR_CALLBACK, copied.diagnostic.code);
    EXPECT_STREQ("expected object", copied.diagnostic.expected);
    EXPECT_STREQ("actual object", copied.diagnostic.actual);
    EXPECT_EQ(4u, copied.diagnostic.location.begin);
    EXPECT_EQ(7u, copied.diagnostic.location.end);
    tlv_codec_detail_t detail{};
    ASSERT_EQ(TLV_OK, tlv_codec_detail_import(&copied.causes, &detail));
    EXPECT_EQ(TLV_CODEC_OP_ENCODE, detail.operation);
    EXPECT_EQ(12345, detail.reported);
    EXPECT_EQ(TLV_CODEC_VIOLATION_RESULT, detail.violation);
    EXPECT_STREQ("object", detail.representation);
    uint64_t restored = 0;
    ASSERT_EQ(TLV_OK, tlv_diagnostic_cause_copy(&copied.causes.frames[1], &foreign, 1, &restored,
                                                sizeof restored));
    EXPECT_EQ(payload, restored);
    const auto before = detail;
    EXPECT_EQ(TLV_ERR_INVALID_ARG, tlv_codec_detail_copy(&copied.causes.frames[1], &detail));
    EXPECT_EQ(before.reported, detail.reported);
}

TEST(Unit_Tlv_CodecDiagnostic, BoundsDelegationAndSaturatesOmissions) {
    tlv_codec_diagnostic_t diagnostic;
    tlv_codec_diagnostic_init(&diagnostic, TLV_CODEC_OP_MEASURE);
    const uint64_t payload = 1;
    for (size_t i = 0; i < TLV_DIAGNOSTIC_CAUSES_MAX; ++i)
        ASSERT_EQ(TLV_OK, tlv_diagnostic_causes_append(&diagnostic.causes, &foreign, 1, &payload,
                                                       sizeof payload));
    tlv_diagnostic_envelope_t envelope;
    tlv_codec_diagnostic_export(&envelope, &diagnostic);
    EXPECT_EQ(2u, envelope.causes.count);
    EXPECT_EQ(1u, envelope.causes.omitted);
    EXPECT_STREQ("opentlv.codec", envelope.causes.frames[0].domain->name);
    diagnostic.causes.omitted = SIZE_MAX;
    tlv_codec_diagnostic_export(&envelope, &diagnostic);
    EXPECT_EQ(SIZE_MAX, envelope.causes.omitted);
    tlv_codec_diagnostic_export(&envelope, nullptr);
    EXPECT_EQ(0u, envelope.causes.count);
    EXPECT_EQ(0u, envelope.causes.omitted);
    tlv_codec_detail_t detail{};
    EXPECT_EQ(TLV_ERR_INVALID_ARG, tlv_codec_detail_import(&envelope.causes, &detail));
    EXPECT_EQ(TLV_ERR_NULL_ARG, tlv_codec_detail_import(nullptr, &detail));
    EXPECT_EQ(TLV_ERR_NULL_ARG, tlv_codec_detail_attach(&envelope.causes, nullptr));
}
