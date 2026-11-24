#include <opentlv/core/result.h>

#include <gtest/gtest.h>

TEST(CoreResult, ClassifiesSuccess)
{
    EXPECT_EQ(TLV_CATEGORY_SUCCESS, tlv_result_category(TLV_OK));
}

TEST(CoreResult, ClassifiesAndDescribesEveryNamedResult)
{
    const struct {
        tlv_result_t result;
        tlv_result_category_t category;
    } cases[] = {
        {TLV_OK, TLV_CATEGORY_SUCCESS},
        {TLV_END, TLV_CATEGORY_END},
        {TLV_ERR_TRUNCATED, TLV_CATEGORY_TRUNCATED},
        {TLV_ERR_INVALID_TAG_SIZE, TLV_CATEGORY_MALFORMED_DATA},
        {TLV_ERR_INVALID_LENGTH, TLV_CATEGORY_MALFORMED_DATA},
        {TLV_ERR_NULL_ARG, TLV_CATEGORY_INVALID_ARGUMENT},
        {TLV_ERR_INVALID_ARG, TLV_CATEGORY_INVALID_ARGUMENT},
        {TLV_ERR_BUFFER_TOO_SHORT, TLV_CATEGORY_CAPACITY},
        {TLV_ERR_OVERFLOW, TLV_CATEGORY_RANGE},
        {TLV_ERR_NATIVE_SIZE, TLV_CATEGORY_RANGE},
    };
    for (const auto &entry : cases) {
        SCOPED_TRACE(entry.result);
        EXPECT_EQ(entry.category, tlv_result_category(entry.result));
        const char *message = tlv_result_string(entry.result);
        ASSERT_NE(nullptr, message);
        EXPECT_STRNE("", message);
        EXPECT_STRNE("unknown result", message);
    }
}

TEST(CoreResult, ReturnsTheFallbackForUnassignedCodes)
{
    // Both a gap and a code above the last enumerator fit the C++ enum's range.
    const tlv_result_t unassigned[] = {
        static_cast<tlv_result_t>(2), static_cast<tlv_result_t>(18)
    };
    const char *retained = tlv_result_string(TLV_END);
    for (const tlv_result_t code : unassigned) {
        SCOPED_TRACE(code);
        EXPECT_EQ(TLV_CATEGORY_UNKNOWN, tlv_result_category(code));
        EXPECT_STREQ("unknown result", tlv_result_string(code));
    }
    EXPECT_STREQ("end of input", retained);
}
