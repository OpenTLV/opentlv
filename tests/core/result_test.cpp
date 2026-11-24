#include <opentlv/core/result.h>

#include <gtest/gtest.h>

TEST(CoreResult, ClassifiesSuccess)
{
    EXPECT_EQ(TLV_CATEGORY_SUCCESS, tlv_result_category(TLV_OK));
}
