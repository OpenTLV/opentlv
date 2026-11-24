#include <opentlv/core/formats/fixed.hpp>

#include <gtest/gtest.h>

#include <array>
#include <limits>
#include <utility>

namespace {
namespace core = opentlv::core;

static_assert(noexcept(core::element_size({}, {}, std::declval<std::size_t &>())));
static_assert(noexcept(core::read_length({}, {}, std::declval<core::logical_size &>(),
                                         std::declval<std::size_t &>())));

TEST(CppFixed, ConvertsConfigurationByValueAndValidatesHeaderSize)
{
    const tlv_fixed_format_t formats[] = {
        {{3}, {2, TLV_BYTE_ORDER_LITTLE_ENDIAN}},
        {{0}, {2, TLV_BYTE_ORDER_BIG_ENDIAN}},
        {{1}, {0, TLV_BYTE_ORDER_BIG_ENDIAN}},
        {{1}, {9, TLV_BYTE_ORDER_LITTLE_ENDIAN}},
        {{1}, {2, TLV_BYTE_ORDER_UNKNOWN}},
        {{std::numeric_limits<std::size_t>::max()}, {1, TLV_BYTE_ORDER_BIG_ENDIAN}},
    };
    for (const auto &format : formats) {
        const auto converted = core::from_c(format);
        const auto native = core::to_c(converted);
        EXPECT_EQ(format.identifier.size, native.identifier.size);
        EXPECT_EQ(format.length.size, native.length.size);
        EXPECT_EQ(format.length.byte_order, native.length.byte_order);

        std::size_t c_size = 71, cpp_size = 71;
        const auto result = tlv_fixed_header_size(&format, &c_size);
        EXPECT_EQ(core::from_c(result), core::header_size(converted, cpp_size));
        EXPECT_EQ(c_size, cpp_size);
    }
    const core::fixed_format unsupported_order{{1}, {2, static_cast<core::byte_order>(97)}};
    std::size_t size = 71;
    EXPECT_EQ(core::result::invalid_arg, core::header_size(unsupported_order, size));
    EXPECT_EQ(71u, size);
}

TEST(CppFixed, MeasuresElementsAndPreservesSizeOnError)
{
    const core::fixed_format format{{2}, {1, core::byte_order::big_endian}};
    const uint8_t tag[] = {0, 4};
    const uint8_t payload[] = {8, 9};
    const core::element_view elements[] = {
        {{tag, 2}, {payload, 2}}, {{tag, 2}, {nullptr, 0}},
        {{tag, 1}, {payload, 2}}, {{tag, 2}, {nullptr, 2}},
        {{tag, 2}, {payload, 256}},  // Measurement only inspects the descriptor.
    };
    const auto c_format = core::to_c(format);
    for (const auto &element : elements) {
        const auto c_element = core::to_c(element);
        std::size_t c_size = 75, cpp_size = 75;
        const auto result = tlv_fixed_element_size(&c_format, &c_element, &c_size);
        EXPECT_EQ(core::from_c(result), core::element_size(format, element, cpp_size));
        EXPECT_EQ(c_size, cpp_size);
    }
}

TEST(CppFixed, ReadsAndWritesExactlyTheSelectedFieldInBothOrders)
{
    const core::byte_order orders[] = {core::byte_order::little_endian, core::byte_order::big_endian};
    for (const auto order : orders) {
        const core::fixed_length field{2, order};
        const auto c_field = core::to_c(field);
        std::array<uint8_t, 3> cpp_wire{0xcc, 0xcc, 0xcc}, c_wire = cpp_wire;
        std::size_t c_written = 0, cpp_written = 0;
        ASSERT_EQ(TLV_OK, tlv_fixed_length_write(&c_field, 0x1234, c_wire.data(),
                                                 c_wire.size(), &c_written));
        const core::mutable_byte_view output{cpp_wire.data(), cpp_wire.size()};
        ASSERT_EQ(core::result::ok, core::write_length(field, 0x1234, output, cpp_written));
        EXPECT_EQ(c_wire, cpp_wire);
        EXPECT_EQ(c_written, cpp_written);
        EXPECT_EQ(0xcc, cpp_wire.back());

        core::logical_size cpp_decoded = 0;
        tlv_size_t c_decoded = 0;
        std::size_t c_consumed = 0, cpp_consumed = 0;
        ASSERT_EQ(TLV_OK, tlv_fixed_length_read(&c_field, c_wire.data(), c_wire.size(),
                                                &c_decoded, &c_consumed));
        ASSERT_EQ(core::result::ok, core::read_length(field, {cpp_wire.data(), cpp_wire.size()},
                                                      cpp_decoded, cpp_consumed));
        EXPECT_EQ(c_decoded, cpp_decoded);
        EXPECT_EQ(0x1234u, cpp_decoded);
        EXPECT_EQ(c_consumed, cpp_consumed);
        EXPECT_EQ(2u, cpp_consumed);
    }
}

TEST(CppFixed, PreservesFieldOutputsAndReportsTruncation)
{
    const core::fixed_length field{2, core::byte_order::big_endian};
    uint8_t octet = 0xe1;
    core::logical_size decoded = 213;
    std::size_t count = 81;
    EXPECT_EQ(core::result::truncated, core::read_length(field, {&octet, 1}, decoded, count));
    EXPECT_EQ(213u, decoded);
    EXPECT_EQ(1u, count);
    const core::fixed_length invalid{0, field.order};
    EXPECT_EQ(core::result::invalid_arg, core::read_length(invalid, {}, decoded, count));
    EXPECT_EQ(213u, decoded);
    EXPECT_EQ(1u, count);

    ASSERT_EQ(core::result::ok, core::write_length(field, 25, {}, count));
    EXPECT_EQ(2u, count);
    count = 81;
    EXPECT_EQ(core::result::buffer_too_short, core::write_length(field, 25, {&octet, 0}, count));
    EXPECT_EQ(81u, count);
    EXPECT_EQ(0xe1, octet);
    EXPECT_EQ(core::result::buffer_too_short, core::write_length(field, 25, {&octet, 1}, count));
    EXPECT_EQ(81u, count);
    EXPECT_EQ(0xe1, octet);
    EXPECT_EQ(core::result::invalid_length, core::write_length(field, 65536, {&octet, 1}, count));
    EXPECT_EQ(81u, count);
    EXPECT_EQ(0xe1, octet);
}

}  // namespace
