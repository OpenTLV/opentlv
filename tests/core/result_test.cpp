#include <opentlv/core/result.hpp>

#include <gtest/gtest.h>

namespace core = opentlv::core;

static_assert(noexcept(core::result_category(core::result::ok)));
static_assert(noexcept(core::result_string(core::result::ok)));

TEST(CoreResult, ClassifiesSuccess)
{
    EXPECT_EQ(TLV_CATEGORY_SUCCESS, tlv_result_category(TLV_OK));
}

TEST(CoreResult, ClassifiesAndDescribesEveryNamedResult)
{
    using core::category;
    using core::result;
    const struct {
        tlv_result_t c_code;
        result cpp_code;
        tlv_result_category_t c_category;
        category cpp_category;
    } cases[] = {
        {TLV_OK, result::ok, TLV_CATEGORY_SUCCESS, category::success},
        {TLV_END, result::end, TLV_CATEGORY_END, category::end},
        {TLV_ERR_TRUNCATED, result::truncated, TLV_CATEGORY_TRUNCATED, category::truncated},
        {TLV_ERR_INVALID_TAG_SIZE, result::invalid_tag_size, TLV_CATEGORY_MALFORMED_DATA, category::malformed_data},
        {TLV_ERR_INVALID_LENGTH, result::invalid_length, TLV_CATEGORY_MALFORMED_DATA, category::malformed_data},
        {TLV_ERR_NULL_ARG, result::null_arg, TLV_CATEGORY_INVALID_ARGUMENT, category::invalid_argument},
        {TLV_ERR_INVALID_ARG, result::invalid_arg, TLV_CATEGORY_INVALID_ARGUMENT, category::invalid_argument},
        {TLV_ERR_BUFFER_TOO_SHORT, result::buffer_too_short, TLV_CATEGORY_CAPACITY, category::capacity},
        {TLV_ERR_OVERFLOW, result::overflow, TLV_CATEGORY_RANGE, category::range},
        {TLV_ERR_NATIVE_SIZE, result::native_size, TLV_CATEGORY_RANGE, category::range},
    };
    for (const auto &entry : cases) {
        SCOPED_TRACE(entry.c_code);
        EXPECT_EQ(entry.cpp_code, core::from_c(entry.c_code));
        EXPECT_EQ(entry.c_code, core::to_c(entry.cpp_code));
        EXPECT_EQ(entry.c_category, tlv_result_category(entry.c_code));
        EXPECT_EQ(entry.cpp_category, core::result_category(entry.cpp_code));
        EXPECT_EQ(entry.cpp_category, core::from_c(entry.c_category));
        EXPECT_EQ(entry.c_category, core::to_c(entry.cpp_category));
        const char *message = tlv_result_string(entry.c_code);
        ASSERT_NE(nullptr, message);
        EXPECT_STRNE("", message);
        EXPECT_STRNE("unknown result", message);
        EXPECT_EQ(message, core::result_string(entry.cpp_code));
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
        const auto cpp_code = core::from_c(code);
        EXPECT_EQ(code, core::to_c(cpp_code));
        EXPECT_EQ(core::category::unknown, core::result_category(cpp_code));
        EXPECT_EQ(tlv_result_string(code), core::result_string(cpp_code));
    }
    EXPECT_STREQ("end of input", retained);
}
