#include <opentlv/core/size.h>

#include <gtest/gtest.h>

TEST(CoreSize, ConvertsRepresentableCountsToNative)
{
    static_assert(TLV_SIZE_MAX == UINT64_MAX, "TLV byte counts span 64 bits");
    constexpr tlv_size_t largest = SIZE_MAX < TLV_SIZE_MAX ? SIZE_MAX : TLV_SIZE_MAX;
    const tlv_size_t counts[] = {0, 511, largest};
    for (const tlv_size_t count : counts) {
        SCOPED_TRACE(count);
        size_t native = 42;
        ASSERT_EQ(TLV_OK, tlv_size_to_native(count, &native));
        EXPECT_EQ(count, native);
    }
}

#if SIZE_MAX < TLV_SIZE_MAX
TEST(CoreSize, RejectsTheFirstUnrepresentableNativeCountAndPreservesOutput)
{
    constexpr tlv_size_t too_large = static_cast<tlv_size_t>(SIZE_MAX) + 1;
    size_t native = 42;
    EXPECT_EQ(TLV_ERR_NATIVE_SIZE, tlv_size_to_native(too_large, &native));
    EXPECT_EQ(42u, native);
}
#endif

#if defined(_M_IX86) || defined(__i386__)
TEST(CoreSize, RejectsUint64MaxOnX86)
{
    static_assert(sizeof(size_t) == 4, "The x86 test requires a four-byte size_t");
    size_t native = 42;
    EXPECT_EQ(TLV_ERR_NATIVE_SIZE, tlv_size_to_native(UINT64_MAX, &native));
    EXPECT_EQ(42u, native);
}
#elif defined(_M_X64) || (defined(__x86_64__) && !defined(__ILP32__))
TEST(CoreSize, ConvertsCountsAboveUint32MaxOnX64)
{
    static_assert(sizeof(size_t) == 8, "The x64 test requires an eight-byte size_t");
    const tlv_size_t counts[] = {UINT64_C(0x100000000), UINT64_MAX};
    for (const tlv_size_t count : counts) {
        SCOPED_TRACE(count);
        size_t native = 0;
        ASSERT_EQ(TLV_OK, tlv_size_to_native(count, &native));
        EXPECT_EQ(count, native);
    }
}
#endif

TEST(CoreSize, AddsCountsThroughTheLogicalLimit)
{
    const struct {
        tlv_size_t left;
        tlv_size_t right;
        tlv_size_t expected;
    } sums[] = {
        {0, 0, 0},
        {UINT32_MAX, 1, UINT64_C(0x100000000)},
        {TLV_SIZE_MAX - 1, 1, TLV_SIZE_MAX},
        {TLV_SIZE_MAX, 0, TLV_SIZE_MAX},
    };
    for (const auto &entry : sums) {
        SCOPED_TRACE(testing::Message() << entry.left << " + " << entry.right);
        tlv_size_t sum = 42;
        ASSERT_EQ(TLV_OK, tlv_size_add(entry.left, entry.right, &sum));
        EXPECT_EQ(entry.expected, sum);
    }
}

TEST(CoreSize, ReportsOverflowAndPreservesOutput)
{
    tlv_size_t sum = 42;
    EXPECT_EQ(TLV_ERR_OVERFLOW, tlv_size_add(TLV_SIZE_MAX, 1, &sum));
    EXPECT_EQ(42u, sum);
    EXPECT_EQ(TLV_ERR_OVERFLOW, tlv_size_add(2, TLV_SIZE_MAX - 1, &sum));
    EXPECT_EQ(42u, sum);
}

TEST(CoreSize, RequiresAnOutputPointer)
{
    EXPECT_EQ(TLV_ERR_NULL_ARG, tlv_size_to_native(0, nullptr));
    EXPECT_EQ(TLV_ERR_NULL_ARG, tlv_size_add(0, 0, nullptr));
}
