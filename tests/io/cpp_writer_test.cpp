#include <opentlv/io/writer/writer.hpp>
#include <gtest/gtest.h>
#include <array>
#include <type_traits>

namespace {
namespace core = opentlv::core;
namespace io = opentlv::io;

static_assert(!std::is_copy_constructible_v<io::writer>);
static_assert(!std::is_copy_assignable_v<io::writer>);

TEST(CppWriter, MatchesCAndPreservesCommittedBytesOnShortAppend)
{
    core::fixed_format format{{1}, {2, core::byte_order::little_endian}};
    const std::uint8_t tag = 0xab;
    const core::element_view element{{&tag, 1}, {}};
    const auto config = core::to_c(format);
    const auto source = core::to_c(element);
    std::array<std::uint8_t, 4> expected{}, one_shot{}, stream{};
    std::size_t count{}, written{};
    ASSERT_EQ(TLV_OK, tlv_write(expected.data(), expected.size(), &config, &source, &count));
    core::diagnostic details{};
    ASSERT_EQ(core::result::buffer_too_short, io::write(format, {}, element, written, &details));
    EXPECT_EQ(count, written);
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
} // namespace
