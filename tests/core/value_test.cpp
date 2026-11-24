#include <opentlv/core/value.h>

#include <gtest/gtest.h>

TEST(CoreValue, AShallowCopyKeepsBorrowingTheOriginalBytes)
{
    const uint8_t payload[] = {0x00, 0xBA, 0xDC};
    tlv_value_t view{};
    ASSERT_EQ(TLV_OK, tlv_value_init(payload, sizeof payload, &view));
    ASSERT_EQ(payload, view.data);
    ASSERT_EQ(sizeof payload, view.size);
    EXPECT_EQ(TLV_OK, tlv_value_validate(&view));

    const tlv_value_t copy = view;
    ASSERT_EQ(TLV_OK, tlv_value_init(nullptr, 0, &view));
    EXPECT_EQ(nullptr, view.data);
    EXPECT_EQ(0u, view.size);
    EXPECT_EQ(payload, copy.data);
    EXPECT_EQ(sizeof payload, copy.size);
    EXPECT_EQ(0xDC, copy.data[2]);
}

TEST(CoreValue, EmptyViewsPreserveTheSuppliedPointer)
{
    const uint8_t byte = 0;
    const uint8_t *const empty_pointers[] = {nullptr, &byte + 1};
    for (const uint8_t *pointer : empty_pointers) {
        tlv_value_t view{&byte, 1};
        ASSERT_EQ(TLV_OK, tlv_value_init(pointer, 0, &view));
        EXPECT_EQ(pointer, view.data);
        EXPECT_EQ(0u, view.size);
        EXPECT_EQ(TLV_OK, tlv_value_validate(&view));
    }
}

TEST(CoreValue, ChecksTheLargestRepresentableSizeWithoutReadingThePayload)
{
    constexpr tlv_size_t largest = SIZE_MAX < TLV_SIZE_MAX ? SIZE_MAX : TLV_SIZE_MAX;
    const uint8_t byte = 0;
    const tlv_value_t descriptor{&byte, largest};
    EXPECT_EQ(TLV_OK, tlv_value_validate(&descriptor));
    tlv_value_t view{};
    ASSERT_EQ(TLV_OK, tlv_value_init(&byte, largest, &view));
    EXPECT_EQ(&byte, view.data);
    EXPECT_EQ(largest, view.size);
}

TEST(CoreValue, RejectsMissingDataAndPreservesTheDestination)
{
    const tlv_value_t missing{nullptr, 1};
    EXPECT_EQ(TLV_ERR_NULL_ARG, tlv_value_validate(&missing));
    const uint8_t payload[] = {0x01, 0x02};
    tlv_value_t destination{payload, sizeof payload};
    EXPECT_EQ(TLV_ERR_NULL_ARG, tlv_value_init(nullptr, 1, &destination));
    EXPECT_EQ(payload, destination.data);
    EXPECT_EQ(sizeof payload, destination.size);
}

TEST(CoreValue, RequiresAViewPointer)
{
    EXPECT_EQ(TLV_ERR_NULL_ARG, tlv_value_init(nullptr, 0, nullptr));
    EXPECT_EQ(TLV_ERR_NULL_ARG, tlv_value_validate(nullptr));
}

#if SIZE_MAX < TLV_SIZE_MAX
TEST(CoreValue, RejectsANonNativeSizeAndPreservesTheDestination)
{
    constexpr tlv_size_t too_large = static_cast<tlv_size_t>(SIZE_MAX) + 1;
    const uint8_t byte = 0;
    const tlv_value_t descriptor{&byte, too_large};
    EXPECT_EQ(TLV_ERR_NATIVE_SIZE, tlv_value_validate(&descriptor));
    tlv_value_t destination{&byte + 1, 0};
    EXPECT_EQ(TLV_ERR_NATIVE_SIZE, tlv_value_init(&byte, too_large, &destination));
    EXPECT_EQ(&byte + 1, destination.data);
    EXPECT_EQ(0u, destination.size);
}
#endif
