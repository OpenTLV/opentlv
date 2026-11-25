#include <opentlv/core/field/fixed.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <string>
#include <tuple>

namespace {

namespace core = opentlv::core;
using LengthParameters = std::tuple<size_t, tlv_byte_order_t>;

class FixedLength : public testing::TestWithParam<LengthParameters> {
protected:
    const tlv_fixed_length_t field{std::get<0>(GetParam()), std::get<1>(GetParam())};
    const core::fixed_length cpp_field = core::from_c(field);
    const tlv_size_t maximum = UINT64_MAX >> (8 * (8 - field.size));

    struct Example {
        tlv_size_t value;
        const uint8_t *wire;
    };

    std::array<Example, 3> examples() const
    {
        static constexpr tlv_size_t values[] = {
            0xa1, 0xa102, 0xa102d3, 0xa102d304,
            0xa102d304f5, 0xa102d304f506, 0xa102d304f50697, 0xa102d304f5069708
        };
        static constexpr uint8_t big[]    = {0xa1, 0x02, 0xd3, 0x04, 0xf5, 0x06, 0x97, 0x08};
        static constexpr uint8_t little[] = {0x08, 0x97, 0x06, 0xf5, 0x04, 0xd3, 0x02, 0xa1};
        static constexpr uint8_t zeros[8]{};
        static constexpr uint8_t ones[]   = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
        const uint8_t *pattern = field.byte_order == TLV_BYTE_ORDER_BIG_ENDIAN
            ? big : little + (8 - field.size);
        return {{{values[field.size - 1], pattern}, {0, zeros}, {maximum, ones}}};
    }
};

TEST_P(FixedLength, ReadsExactlyOneField)
{
    for (const Example &example : examples()) {
        SCOPED_TRACE(example.value);
        alignas(tlv_size_t) std::array<uint8_t, 10> input;
        input.fill(0x6b);
        std::copy_n(example.wire, field.size, input.begin() + 1);
        for (size_t available : {field.size, input.size() - 1}) {
            tlv_size_t decoded = ~example.value;
            size_t consumed = 99;
            ASSERT_EQ(TLV_OK, tlv_fixed_length_read(&field, input.data() + 1, available,
                                                    &decoded, &consumed));
            EXPECT_EQ(example.value, decoded);
            EXPECT_EQ(field.size, consumed);
            core::logical_size cpp_decoded = ~example.value;
            size_t cpp_consumed = 99;
            ASSERT_EQ(core::result::ok, core::read_length(
                cpp_field, {input.data() + 1, available}, cpp_decoded, cpp_consumed));
            EXPECT_EQ(decoded, cpp_decoded);
            EXPECT_EQ(consumed, cpp_consumed);
        }
    }
}

TEST_P(FixedLength, MeasuresAndWritesExactlyOneField)
{
    for (const Example &example : examples()) {
        SCOPED_TRACE(example.value);
        size_t written = 99;
        ASSERT_EQ(TLV_OK, tlv_fixed_length_write(&field, example.value, nullptr, 0, &written));
        EXPECT_EQ(field.size, written);
        size_t cpp_written = 99;
        ASSERT_EQ(core::result::ok, core::write_length(cpp_field, example.value, {}, cpp_written));
        EXPECT_EQ(written, cpp_written);

        std::array<uint8_t, 10> expected;
        expected.fill(0x6b);
        std::copy_n(example.wire, field.size, expected.begin() + 1);
        for (size_t capacity : {field.size, expected.size() - 1}) {
            alignas(tlv_size_t) std::array<uint8_t, 10> output;
            output.fill(0x6b);
            alignas(tlv_size_t) auto cpp_output = output;
            written = 99;
            cpp_written = 99;
            ASSERT_EQ(TLV_OK, tlv_fixed_length_write(&field, example.value, output.data() + 1,
                                                     capacity, &written));
            EXPECT_EQ(expected, output);
            EXPECT_EQ(field.size, written);
            ASSERT_EQ(core::result::ok, core::write_length(
                cpp_field, example.value, {cpp_output.data() + 1, capacity}, cpp_written));
            EXPECT_EQ(output, cpp_output);
            EXPECT_EQ(written, cpp_written);
        }
    }
}

TEST_P(FixedLength, TruncatedReadsReportTheAvailablePrefix)
{
    const std::array<uint8_t, 8> input{};
    for (size_t available = 0; available < field.size; ++available) {
        SCOPED_TRACE(available);
        tlv_size_t decoded = 42;
        size_t consumed = 99;
        EXPECT_EQ(TLV_ERR_TRUNCATED, tlv_fixed_length_read(&field, input.data(), available,
                                                           &decoded, &consumed));
        EXPECT_EQ(42u, decoded);
        EXPECT_EQ(available, consumed);
        core::logical_size cpp_decoded = 42;
        size_t cpp_consumed = 99;
        EXPECT_EQ(core::result::truncated, core::read_length(
            cpp_field, {input.data(), available}, cpp_decoded, cpp_consumed));
        EXPECT_EQ(decoded, cpp_decoded);
        EXPECT_EQ(consumed, cpp_consumed);
    }
}

TEST_P(FixedLength, InvalidWritesPreserveTheBufferAndCount)
{
    std::array<uint8_t, 8> output;
    output.fill(0x6b);
    const auto original = output;
    auto cpp_output = original;
    size_t written = 99;
    size_t cpp_written = 99;
    for (size_t capacity = 0; capacity < field.size; ++capacity) {
        SCOPED_TRACE(capacity);
        EXPECT_EQ(TLV_ERR_BUFFER_TOO_SHORT, tlv_fixed_length_write(&field, maximum, output.data(),
                                                                   capacity, &written));
        EXPECT_EQ(original, output);
        EXPECT_EQ(99u, written);
        EXPECT_EQ(core::result::buffer_too_short, core::write_length(
            cpp_field, maximum, {cpp_output.data(), capacity}, cpp_written));
        EXPECT_EQ(output, cpp_output);
        EXPECT_EQ(written, cpp_written);
    }
    if (maximum < UINT64_MAX) {
        EXPECT_EQ(TLV_ERR_INVALID_LENGTH, tlv_fixed_length_write(&field, maximum + 1, output.data(),
                                                                 output.size(), &written));
        EXPECT_EQ(original, output);
        EXPECT_EQ(99u, written);
        EXPECT_EQ(core::result::invalid_length, core::write_length(
            cpp_field, maximum + 1, {cpp_output.data(), cpp_output.size()}, cpp_written));
        EXPECT_EQ(output, cpp_output);
        EXPECT_EQ(written, cpp_written);
        EXPECT_EQ(TLV_ERR_INVALID_LENGTH, tlv_fixed_length_write(&field, maximum + 1, nullptr, 0,
                                                                 &written));
        EXPECT_EQ(99u, written);
        EXPECT_EQ(core::result::invalid_length, core::write_length(cpp_field, maximum + 1, {}, cpp_written));
        EXPECT_EQ(written, cpp_written);
    }
}

INSTANTIATE_TEST_SUITE_P(WireLayouts, FixedLength,
    testing::Combine(testing::Values(1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u),
                     testing::Values(TLV_BYTE_ORDER_BIG_ENDIAN, TLV_BYTE_ORDER_LITTLE_ENDIAN)),
    [](const testing::TestParamInfo<LengthParameters> &info) {
        const std::string order = std::get<1>(info.param) == TLV_BYTE_ORDER_BIG_ENDIAN
            ? "BigEndian" : "LittleEndian";
        return order + std::to_string(std::get<0>(info.param));
    });

TEST(FixedLengthArguments, RejectsInvalidWidthsAndByteOrder)
{
    const tlv_fixed_length_t invalid[] = {
        {0, TLV_BYTE_ORDER_BIG_ENDIAN}, {9, TLV_BYTE_ORDER_LITTLE_ENDIAN},
        {1, TLV_BYTE_ORDER_UNKNOWN}, {1, static_cast<tlv_byte_order_t>(3)}
    };
    for (const auto &field : invalid) {
        SCOPED_TRACE(testing::Message() << "width " << field.size << ", order " << field.byte_order);
        std::array<uint8_t, 9> bytes;
        bytes.fill(0x6b);
        const auto original = bytes;
        tlv_size_t decoded = 42;
        size_t count = 99;
        EXPECT_EQ(TLV_ERR_INVALID_ARG, tlv_fixed_length_read(&field, bytes.data(), bytes.size(),
                                                             &decoded, &count));
        EXPECT_EQ(42u, decoded);
        EXPECT_EQ(99u, count);
        EXPECT_EQ(TLV_ERR_INVALID_ARG, tlv_fixed_length_write(&field, 0, bytes.data(), bytes.size(),
                                                              &count));
        EXPECT_EQ(original, bytes);
        EXPECT_EQ(99u, count);
        EXPECT_EQ(TLV_ERR_INVALID_ARG, tlv_fixed_length_write(&field, 0, nullptr, 0, &count));
        EXPECT_EQ(99u, count);
    }
}

TEST(FixedLengthArguments, ReadsRejectMissingArgumentsAndDistinguishEmptyInput)
{
    const tlv_fixed_length_t field{1, TLV_BYTE_ORDER_BIG_ENDIAN};
    const uint8_t byte = 0xa1;
    tlv_size_t decoded = 42;
    size_t consumed = 99;
    EXPECT_EQ(TLV_ERR_NULL_ARG, tlv_fixed_length_read(nullptr, &byte, 1, &decoded, &consumed));
    EXPECT_EQ(TLV_ERR_NULL_ARG, tlv_fixed_length_read(&field, nullptr, 1, &decoded, &consumed));
    EXPECT_EQ(TLV_ERR_NULL_ARG, tlv_fixed_length_read(&field, &byte, 1, nullptr, &consumed));
    EXPECT_EQ(TLV_ERR_NULL_ARG, tlv_fixed_length_read(&field, &byte, 1, &decoded, nullptr));
    EXPECT_EQ(42u, decoded);
    EXPECT_EQ(99u, consumed);
    EXPECT_EQ(TLV_ERR_TRUNCATED, tlv_fixed_length_read(&field, nullptr, 0, &decoded, &consumed));
    EXPECT_EQ(42u, decoded);
    EXPECT_EQ(0u, consumed);
}

TEST(FixedLengthArguments, WritesRequireConfigCountAndStorageForPositiveCapacity)
{
    const tlv_fixed_length_t field{1, TLV_BYTE_ORDER_BIG_ENDIAN};
    uint8_t byte = 0x6b;
    size_t written = 99;
    EXPECT_EQ(TLV_ERR_NULL_ARG, tlv_fixed_length_write(nullptr, 0, &byte, 1, &written));
    EXPECT_EQ(TLV_ERR_NULL_ARG, tlv_fixed_length_write(&field, 0, nullptr, 1, &written));
    EXPECT_EQ(TLV_ERR_NULL_ARG, tlv_fixed_length_write(&field, 0, &byte, 1, nullptr));
    EXPECT_EQ(0x6b, byte);
    EXPECT_EQ(99u, written);
}

} // namespace
