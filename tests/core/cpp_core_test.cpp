#include <opentlv/core/bytes.hpp>
#include <opentlv/core/diagnostic.hpp>
#include <opentlv/core/element.hpp>
#include <opentlv/core/length.hpp>

#include <gtest/gtest.h>

#include <limits>
#include <type_traits>
#include <utility>

namespace {
namespace core = opentlv::core;

static_assert(!std::is_convertible_v<core::byte_view, core::mutable_byte_view>);
static_assert(!std::is_convertible_v<core::tag_view, core::length_view>);
static_assert(!std::is_convertible_v<core::length_view, core::value_view>);
static_assert(!std::is_convertible_v<tlv_element_t, core::element_view>);
static_assert(!std::is_convertible_v<core::element_view, tlv_element_t>);
static_assert(std::is_same_v<decltype(core::byte_view::data), const std::uint8_t *>);
static_assert(std::is_same_v<decltype(core::mutable_byte_view::data), std::uint8_t *>);
static_assert(std::numeric_limits<core::logical_size>::digits == 64);
static_assert(core::to_c(core::result::native_size) == TLV_ERR_NATIVE_SIZE);
static_assert(core::from_c(TLV_END) == core::result::end);
constexpr auto unassigned_result = static_cast<tlv_result_t>(2);
static_assert(core::to_c(core::from_c(unassigned_result)) == unassigned_result);
static_assert(noexcept(core::make_value_view(nullptr, 0, std::declval<core::value_view &>())));
static_assert(noexcept(core::from_c(tlv_diagnostic_t{})));
static_assert(noexcept(core::to_c(core::diagnostic{})));
static_assert(core::to_c(core::field::none) == TLV_FIELD_NONE);
static_assert(core::to_c(core::field::tag) == TLV_FIELD_TAG);
static_assert(core::to_c(core::field::length) == TLV_FIELD_LENGTH);
static_assert(core::to_c(core::field::value) == TLV_FIELD_VALUE);
static_assert(core::to_c(core::field::element) == TLV_FIELD_ELEMENT);

TEST(CppCore, RoundTripsDiagnosticFieldsAndFullWidthCounts)
{
    const tlv_diagnostic_t native{
        TLV_ERR_TRUNCATED, TLV_FIELD_VALUE, std::numeric_limits<std::size_t>::max(),
        std::numeric_limits<std::uintmax_t>::max(), std::numeric_limits<std::uintmax_t>::max() - 1};
    const auto details = core::from_c(native);
    EXPECT_EQ(core::result::truncated, details.result);
    EXPECT_EQ(core::field::value, details.field);
    EXPECT_EQ(native.offset, details.offset);
    EXPECT_EQ(native.required, details.required);
    EXPECT_EQ(native.available, details.available);

    const auto converted = core::to_c(details);
    EXPECT_EQ(native.result, converted.result);
    EXPECT_EQ(native.field, converted.field);
    EXPECT_EQ(native.offset, converted.offset);
    EXPECT_EQ(native.required, converted.required);
    EXPECT_EQ(native.available, converted.available);
}

TEST(CppCore, CopiedDescriptorsKeepBorrowingTheOriginalStorage)
{
    const uint8_t bytes[] = {0xae, 1, 0x65};
    core::element_view element;
    core::length_view length;
    {
        const tlv_element_t native{{bytes, 1}, {bytes + 2, 1}};
        const auto borrowed = core::from_c(native);
        element = borrowed;
        length = core::from_c(tlv_length_t{bytes + 1, 1});
    }
    EXPECT_EQ(bytes, element.tag.data);
    EXPECT_EQ(1u, element.tag.size);
    EXPECT_EQ(bytes + 2, element.value.data);
    EXPECT_EQ(1u, element.value.size);
    EXPECT_EQ(bytes + 1, length.data);
    EXPECT_EQ(1u, length.size);

    const auto native_element = core::to_c(element);
    const auto native_length = core::to_c(length);
    EXPECT_EQ(bytes, native_element.tag.data);
    EXPECT_EQ(1u, native_element.tag.size);
    EXPECT_EQ(bytes + 2, native_element.value.data);
    EXPECT_EQ(1u, native_element.value.size);
    EXPECT_EQ(bytes + 1, native_length.data);
    EXPECT_EQ(1u, native_length.size);
}

TEST(CppCore, ValidatesBorrowedValuesAndPreservesTheDestinationOnFailure)
{
    const uint8_t byte = 0xab;
    const uint8_t previous[] = {0x34, 0x35};
    const tlv_value_t inputs[] = {
        {&byte, 1},
        {nullptr, 0},
        {&byte + 1, 0},
        {nullptr, 1},
#if SIZE_MAX < TLV_SIZE_MAX
        {&byte, static_cast<tlv_size_t>(SIZE_MAX) + 1},
#endif
    };
    for (const auto &input : inputs) {
        core::value_view view{previous, sizeof previous};
        tlv_value_t native{previous, sizeof previous};
        const auto result = tlv_value_init(input.data, input.size, &native);
        EXPECT_EQ(core::from_c(result), core::make_value_view(input.data, input.size, view));
        EXPECT_EQ(native.data, view.data);
        EXPECT_EQ(native.size, view.size);
        EXPECT_EQ(core::from_c(tlv_value_validate(&input)), core::validate(core::from_c(input)));
    }
}

TEST(CppCore, ChecksLogicalAndNativeSizeLimits)
{
    constexpr auto maximum = std::numeric_limits<core::logical_size>::max();
    core::logical_size sum = 0;
    EXPECT_EQ(core::result::ok, core::size_add(maximum - 1, 1, sum));
    EXPECT_EQ(maximum, sum);
    EXPECT_EQ(core::result::overflow, core::size_add(maximum, 1, sum));
    EXPECT_EQ(maximum, sum);

    std::size_t native = 13;
    ASSERT_EQ(core::result::ok, core::size_to_native(0, native));
    EXPECT_EQ(0u, native);
    std::size_t c_size = 0;
    EXPECT_EQ(core::from_c(tlv_size_to_native(maximum, &c_size)),
              core::size_to_native(maximum, native));
    EXPECT_EQ(c_size, native);

    const uint8_t byte = 0;
    const core::value_view logical_view{&byte, maximum};
    const auto descriptor = core::to_c(logical_view);
    EXPECT_EQ(maximum, descriptor.size);
    EXPECT_EQ(maximum, core::from_c(descriptor).size);
    EXPECT_EQ(core::from_c(tlv_value_validate(&descriptor)), core::validate(logical_view));
}

} // namespace
