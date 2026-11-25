#include <opentlv/io/writer/writer.hpp>
#include <gtest/gtest.h>
#include <array>
#include <type_traits>
#include <utility>

namespace {
namespace core = opentlv::core;
namespace io = opentlv::io;

static_assert(!std::is_copy_constructible_v<io::writer>);
static_assert(!std::is_copy_assignable_v<io::writer>);
static_assert(noexcept(io::write({}, {}, {}, std::declval<std::size_t &>(), nullptr)));
static_assert(noexcept(std::declval<io::writer &>().init({}, {})));
static_assert(noexcept(std::declval<io::writer &>().write({}, nullptr)));

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

TEST(CppWriter, OneShotFailuresMatchCResultsAndDiagnostics)
{
    const core::fixed_format format{{1}, {1, core::byte_order::big_endian}};
    const std::uint8_t tag = 0xa1;
    const core::element_view empty{{&tag, 1}, {}};
    const std::array<std::uint8_t, 256> long_value{};
    const struct {
        core::fixed_format format;
        core::element_view element;
        std::size_t capacity;
        tlv_result_t expected;
    } cases[] = {
        {{}, empty, 3, TLV_ERR_INVALID_ARG},
        {format, {{&tag, 1}, {nullptr, 1}}, 3, TLV_ERR_NULL_ARG},
        {format, {{&tag, 0}, {}}, 3, TLV_ERR_INVALID_TAG_SIZE},
        {format, {{&tag, 1}, {long_value.data(), long_value.size()}}, 3, TLV_ERR_INVALID_LENGTH},
        {format, empty, 1, TLV_ERR_BUFFER_TOO_SHORT},
    };

    const std::array<std::uint8_t, 3> original{0x11, 0x22, 0x33};
    for (const auto &entry : cases) {
        SCOPED_TRACE(entry.expected);
        const auto config = core::to_c(entry.format);
        const auto source = core::to_c(entry.element);
        auto c_bytes = original, bytes = original;
        std::size_t c_written = 91, written = 91;
        tlv_diagnostic_t c_details{};
        core::diagnostic details{core::result::ok, core::field::length, 3, 5, 1};
        const auto c_result = tlv_write_with_diagnostic(c_bytes.data(), entry.capacity, &config,
                                                       &source, &c_written, &c_details);
        ASSERT_EQ(entry.expected, c_result);
        EXPECT_EQ(core::from_c(c_result), io::write(entry.format, {bytes.data(), entry.capacity},
                                                   entry.element, written, &details));
        EXPECT_EQ(core::from_c(c_details.result), details.result);
        EXPECT_EQ(core::from_c(c_details.field), details.field);
        EXPECT_EQ(c_details.offset, details.offset);
        EXPECT_EQ(c_details.required, details.required);
        EXPECT_EQ(c_details.available, details.available);
        EXPECT_EQ(c_written, written);
        EXPECT_EQ(original, bytes);
    }
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
