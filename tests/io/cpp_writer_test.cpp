#include <opentlv/io/writer/writer.hpp>
#include <gtest/gtest.h>
#include <array>
#include <type_traits>

namespace {
namespace core = opentlv::core;
namespace io = opentlv::io;

static_assert(!std::is_copy_constructible_v<io::writer>);
static_assert(!std::is_copy_assignable_v<io::writer>);

TEST(CppWriter, RejectsWritesUntilInitializationSucceeds)
{
    const core::fixed_format format{{1}, {1, core::byte_order::big_endian}};
    const std::uint8_t tag = 0x9f;
    const core::element_view valid{{&tag, 1}, {}};
    const core::element_view missing_value{{&tag, 1}, {nullptr, 1}};
    io::writer cursor;
    core::diagnostic details{core::result::truncated, core::field::value, 7, 9, 3};
    EXPECT_EQ(core::result::invalid_arg, cursor.write(missing_value, &details));
    EXPECT_EQ(core::result::invalid_arg, details.result);
    EXPECT_EQ(core::field::none, details.field);
    EXPECT_EQ(0u, details.offset);
    EXPECT_EQ(0u, details.required);
    EXPECT_EQ(0u, details.available);
    EXPECT_EQ(0u, cursor.position());
    ASSERT_EQ(core::result::invalid_arg, cursor.init({}, {}));
    EXPECT_EQ(core::result::invalid_arg, cursor.write(valid));
    EXPECT_EQ(0u, cursor.position());
    ASSERT_EQ(core::result::ok, cursor.init(format, {}));
    EXPECT_EQ(core::result::buffer_too_short, cursor.write(valid, &details));
    EXPECT_EQ(core::field::element, details.field);
    EXPECT_EQ(2u, details.required);
    EXPECT_EQ(0u, details.available);
    EXPECT_EQ(0u, cursor.position());
}

TEST(CppWriter, MatchesCAndPreservesCommittedBytesOnShortAppend)
{
    core::fixed_format format{{1}, {2, core::byte_order::little_endian}};
    const std::uint8_t tag = 0xab;
    const std::uint8_t value[] = {0x52, 0x00};
    const core::element_view element{{&tag, 1}, {value, sizeof value}};
    const auto config = core::to_c(format);
    const auto source = core::to_c(element);
    std::array<std::uint8_t, 6> expected{}, one_shot{}, stream{};
    expected.fill(0xcc);
    one_shot.fill(0xcc);
    stream.fill(0xcc);
    std::size_t count{}, written{};
    ASSERT_EQ(TLV_OK, tlv_write(expected.data(), expected.size(), &config, &source, &count));
    core::diagnostic details{};
    ASSERT_EQ(core::result::buffer_too_short, io::write(format, {}, element, written, &details));
    EXPECT_EQ(count, written);
    const auto untouched = one_shot;
    ASSERT_EQ(core::result::buffer_too_short, io::write(format, {one_shot.data(), count - 1},
                                                        element, written, &details));
    EXPECT_EQ(count, written);
    EXPECT_EQ(untouched, one_shot);
    ASSERT_EQ(core::result::ok, io::write(format, {one_shot.data(), one_shot.size()}, element, written, &details));
    EXPECT_EQ(core::result::ok, details.result);
    EXPECT_EQ(count, written);
    EXPECT_EQ(expected, one_shot);
    {
        io::writer cursor;
        ASSERT_EQ(core::result::ok, cursor.init(format, {stream.data(), stream.size()}));
        format = {};
        ASSERT_EQ(core::result::ok, cursor.write(element));
        EXPECT_EQ(core::result::buffer_too_short, cursor.write(element, &details));
        EXPECT_EQ(count, cursor.position());
        EXPECT_EQ(count, details.offset);
        EXPECT_EQ(count, details.required);
        EXPECT_EQ(stream.size() - count, details.available);
    }
    EXPECT_EQ(expected, stream);
}

TEST(CppWriter, InvalidInputsPreserveOneShotBufferAndCount)
{
    const core::fixed_format format{{1}, {1, core::byte_order::big_endian}};
    const std::uint8_t tag = 0xa1;
    const core::element_view empty{{&tag, 1}, {}};
    const core::element_view missing_value{{&tag, 1}, {nullptr, 1}};
    std::array<std::uint8_t, 3> bytes{0x11, 0x22, 0x33};
    const auto original = bytes;
    const core::mutable_byte_view output{bytes.data(), bytes.size()};
    std::size_t written = 91;
    core::diagnostic details{};

    EXPECT_EQ(core::result::invalid_arg, io::write({}, output, empty, written, &details));
    EXPECT_EQ(core::result::invalid_arg, details.result);
    EXPECT_EQ(91u, written);
    EXPECT_EQ(original, bytes);
    EXPECT_EQ(core::result::null_arg, io::write(format, output, missing_value, written, &details));
    EXPECT_EQ(core::result::null_arg, details.result);
    EXPECT_EQ(91u, written);
    EXPECT_EQ(original, bytes);
}

TEST(CppWriter, AppendsEmptyAndNonEmptyValuesAcrossAnInvalidElement)
{
    const core::fixed_format format{{1}, {1, core::byte_order::big_endian}};
    const std::uint8_t tag = 0xa1, other_tag = 0xb2;
    const std::uint8_t payload[] = {0x07, 0x00};
    const core::element_view empty{{&tag, 1}, {nullptr, 0}};
    const core::element_view nonempty{{&other_tag, 1}, {payload, sizeof payload}};
    std::array<std::uint8_t, 6> output{};
    output.fill(0xdd);
    io::writer cursor;
    ASSERT_EQ(core::result::ok, cursor.init(format, {output.data(), output.size()}));
    ASSERT_EQ(core::result::ok, cursor.write(empty));
    EXPECT_EQ(2u, cursor.position());
    const auto committed = output;
    EXPECT_EQ(core::result::invalid_tag_size, cursor.write({}));
    EXPECT_EQ(2u, cursor.position());
    EXPECT_EQ(committed, output);
    ASSERT_EQ(core::result::ok, cursor.write(nonempty));
    EXPECT_EQ(output.size(), cursor.position());
    const std::array<std::uint8_t, 6> expected{0xa1, 0x00, 0xb2, 0x02, 0x07, 0x00};
    EXPECT_EQ(expected, output);
}

TEST(CppWriter, ReinitializationSwitchesBufferAndFormatOnlyOnSuccess)
{
    const core::fixed_format first_format{{1}, {1, core::byte_order::little_endian}};
    const core::fixed_format second_format{{1}, {2, core::byte_order::big_endian}};
    const std::uint8_t tag = 0xa5, value = 0xf0;
    const core::element_view element{{&tag, 1}, {&value, 1}};
    std::array<std::uint8_t, 6> first{};
    std::array<std::uint8_t, 4> second{1, 2, 3, 4};
    const auto untouched = second;
    io::writer cursor;
    ASSERT_EQ(core::result::ok, cursor.init(first_format, {first.data(), first.size()}));
    ASSERT_EQ(core::result::ok, cursor.write(element));
    EXPECT_EQ(core::result::invalid_arg, cursor.init({}, {second.data(), second.size()}));
    EXPECT_EQ(3u, cursor.position());
    ASSERT_EQ(core::result::ok, cursor.write(element));
    const std::array<std::uint8_t, 6> expected_first{0xa5, 1, 0xf0, 0xa5, 1, 0xf0};
    EXPECT_EQ(expected_first, first);
    ASSERT_EQ(core::result::ok, cursor.init(second_format, {second.data(), second.size()}));
    EXPECT_EQ(untouched, second);
    EXPECT_EQ(0u, cursor.position());
    ASSERT_EQ(core::result::ok, cursor.write(element));
    EXPECT_EQ(second.size(), cursor.position());
    const std::array<std::uint8_t, 4> expected_second{0xa5, 0, 1, 0xf0};
    EXPECT_EQ(expected_second, second);
    EXPECT_EQ(expected_first, first);
}
} // namespace
