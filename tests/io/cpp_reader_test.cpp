#include <opentlv/io/reader/reader.hpp>

#include "expect_view.h"
#include <gtest/gtest.h>

#include <limits>
#include <type_traits>
#include <utility>

namespace {
namespace core = opentlv::core;
namespace io = opentlv::io;
using opentlv_test::ExpectView;

constexpr core::fixed_format one_byte_fields{{1}, {1, core::byte_order::big_endian}};
constexpr std::uint8_t marker[] = {0x31, 0x76};
constexpr core::element_view saved{{marker, 1}, {marker + 1, 1}};
constexpr core::diagnostic stale{core::result::invalid_arg, core::field::element, 77, 89, 63};

constexpr auto converted_view = core::from_c(core::to_c(saved));
static_assert(converted_view.tag.data == marker && converted_view.tag.size == 1);
static_assert(converted_view.value.data == marker + 1 && converted_view.value.size == 1);
static_assert(noexcept(core::from_c(core::to_c(saved))));
static_assert(noexcept(core::from_c(core::to_c(one_byte_fields))));
static_assert(std::is_nothrow_default_constructible_v<io::reader>);
static_assert(std::is_nothrow_copy_constructible_v<io::reader>);
static_assert(std::is_nothrow_copy_assignable_v<io::reader>);
static_assert(noexcept(io::read({}, {}, std::declval<core::element_view &>(),
                                std::declval<std::size_t &>())));
static_assert(noexcept(std::declval<io::reader &>().init({}, {})));
static_assert(noexcept(std::declval<io::reader &>().next(std::declval<core::element_view &>())));
static_assert(noexcept(std::declval<const io::reader &>().position()));
static_assert(noexcept(std::declval<const io::reader &>().format()));

void ExpectDiagnostic(const tlv_diagnostic_t &expected, const core::diagnostic &actual)
{
    EXPECT_EQ(core::from_c(expected.result), actual.result);
    EXPECT_EQ(static_cast<int>(expected.field), static_cast<int>(actual.field));
    EXPECT_EQ(expected.offset, actual.offset);
    EXPECT_EQ(expected.required, actual.required);
    EXPECT_EQ(expected.available, actual.available);
}

TEST(CppReader, OneShotMatchesCAtEveryPrefix)
{
    const core::fixed_format format{{2}, {2, core::byte_order::little_endian}};
    const auto c_format = core::to_c(format);
    const std::uint8_t wire[] = {0x00, 0xa1, 0x02, 0x00, 0xcc, 0x00, 0x81};
    for (std::size_t size = 0; size <= sizeof wire; ++size) {
        SCOPED_TRACE(size);
        auto c_view = core::to_c(saved);
        auto view = saved;
        std::size_t c_count = 87, count = 87;
        tlv_diagnostic_t c_details{};
        auto details = stale;
        const auto result =
            tlv_read_with_diagnostic(wire, size, &c_format, &c_view, &c_count, &c_details);
        ASSERT_EQ(core::from_c(result), io::read(format, {wire, size}, view, count, &details));
        ExpectView(c_view, core::to_c(view));
        EXPECT_EQ(c_count, count);
        ExpectDiagnostic(c_details, details);

        view = saved;
        count = 87;
        ASSERT_EQ(core::from_c(result), io::read(format, {wire, size}, view, count));
        ExpectView(c_view, core::to_c(view));
        EXPECT_EQ(c_count, count);
    }
    auto view = saved;
    std::size_t count = 87;
    ASSERT_EQ(core::result::ok, io::read(format, {wire, sizeof wire}, view, count));
    EXPECT_EQ(6u, count);
    ExpectView({{wire, 2}, {wire + 4, 2}}, core::to_c(view));
}

TEST(CppReader, CopiesCursorsAndBorrowsTheInput)
{
    const std::uint8_t wire[] = {0xa1, 1, 0, 0x8f, 0x00, 0, 0};
    const std::uint8_t other_wire[] = {0xb5, 2, 0, 0xab, 0xcd};
    core::element_view first{}, second{};
    {
        io::reader reader;
        {
            core::fixed_format format{{1}, {2, core::byte_order::little_endian}};
            ASSERT_EQ(core::result::ok, reader.init(format, {wire, sizeof wire}));
            format.identifier.size = 3;
            ASSERT_EQ(core::result::ok, reader.next(first));
            ASSERT_EQ(4u, reader.position());
        }
        auto copy = reader;
        ASSERT_EQ(core::result::ok, reader.next(second));
        EXPECT_EQ(sizeof wire, reader.position());
        EXPECT_EQ(4u, copy.position());
        auto details = stale;
        EXPECT_EQ(core::result::end, reader.next(second, &details));
        ExpectDiagnostic({TLV_END, TLV_FIELD_NONE, 0, 0, 0}, details);
        EXPECT_EQ(sizeof wire, reader.position());
        core::element_view from_copy{};
        ASSERT_EQ(core::result::ok, copy.next(from_copy));
        ExpectView(core::to_c(second), core::to_c(from_copy));

        ASSERT_EQ(core::result::ok, reader.init(reader.format(), {other_wire, sizeof other_wire}));
        EXPECT_EQ(0u, reader.position());
        core::element_view other{};
        ASSERT_EQ(core::result::ok, reader.next(other));
        ExpectView({{other_wire, 1}, {other_wire + 3, 2}}, core::to_c(other));
    }
    ExpectView({{wire, 1}, {wire + 3, 1}}, core::to_c(first));
    ExpectView({{wire + 4, 1}, {wire + sizeof wire, 0}}, core::to_c(second));
}

TEST(CppReader, CopiedCursorsKeepTheirStreamWhenTheOriginalIsReinitialized)
{
    const std::uint8_t wire[] = {0x71, 0, 0x82, 1, 0xda};
    const std::uint8_t other[] = {0xab, 0xcd, 1, 0, 0xef};
    const core::fixed_format other_format{{2}, {2, core::byte_order::little_endian}};
    const auto c_format = core::to_c(one_byte_fields);
    const auto c_other_format = core::to_c(other_format);
    tlv_reader_t c_reader{};
    io::reader reader;
    ASSERT_EQ(TLV_OK, tlv_reader_init(&c_reader, wire, sizeof wire, &c_format));
    ASSERT_EQ(core::result::ok, reader.init(one_byte_fields, {wire, sizeof wire}));
    tlv_element_t c_view{};
    core::element_view view{};
    ASSERT_EQ(TLV_OK, tlv_reader_next(&c_reader, &c_view));
    ASSERT_EQ(core::result::ok, reader.next(view));
    auto c_copy = c_reader;
    auto copy = reader;

    ASSERT_EQ(TLV_OK, tlv_reader_init(&c_reader, other, sizeof other, &c_other_format));
    ASSERT_EQ(core::result::ok, reader.init(other_format, {other, sizeof other}));
    ASSERT_EQ(TLV_OK, tlv_reader_next(&c_reader, &c_view));
    ASSERT_EQ(core::result::ok, reader.next(view));
    ExpectView({{other, 2}, {other + 4, 1}}, c_view);
    ExpectView(c_view, core::to_c(view));

    ASSERT_EQ(TLV_OK, tlv_reader_next(&c_copy, &c_view));
    ASSERT_EQ(core::result::ok, copy.next(view));
    ExpectView({{wire + 2, 1}, {wire + 4, 1}}, c_view);
    ExpectView(c_view, core::to_c(view));
}

TEST(CppReader, ReportsStreamOffsetsAndPreservesStateOnFailure)
{
    const core::fixed_format format{{1}, {8, core::byte_order::big_endian}};
    const auto c_format = core::to_c(format);
    const std::uint8_t wire[] = {0x41, 0,    0,    0,    0,    0,    0,    0,    0,   0x42,
                                 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x72};
    tlv_reader_t c_reader{};
    io::reader reader;
    ASSERT_EQ(TLV_OK, tlv_reader_init(&c_reader, wire, sizeof wire, &c_format));
    ASSERT_EQ(core::result::ok, reader.init(format, {wire, sizeof wire}));
    auto plain_reader = reader;
    tlv_element_t c_view{};
    core::element_view view{}, plain_view{};
    for (const auto expected : {TLV_OK, TLV_ERR_TRUNCATED, TLV_ERR_TRUNCATED}) {
        tlv_diagnostic_t c_details{};
        auto details = stale;
        ASSERT_EQ(expected, tlv_reader_next_with_diagnostic(&c_reader, &c_view, &c_details));
        ASSERT_EQ(core::from_c(expected), reader.next(view, &details));
        ExpectView(c_view, core::to_c(view));
        EXPECT_EQ(c_reader.pos, reader.position());
        ExpectDiagnostic(c_details, details);
        ASSERT_EQ(core::from_c(expected), plain_reader.next(plain_view));
        ExpectView(c_view, core::to_c(plain_view));
        EXPECT_EQ(c_reader.pos, plain_reader.position());
    }
}

TEST(CppReader, PreservesTheCursorOnInvalidInitialization)
{
    const std::uint8_t wire[] = {0xa3, 0, 0xb2, 0};
    io::reader reader;
    ASSERT_EQ(core::result::ok, reader.init(one_byte_fields, {wire, sizeof wire}));
    core::element_view first{};
    ASSERT_EQ(core::result::ok, reader.next(first));
    const core::fixed_format oversized{{std::numeric_limits<std::size_t>::max()},
                                       one_byte_fields.length};
    const struct {
        core::fixed_format format;
        core::byte_view input;
        core::result result;
    } invalid[] = {
        {one_byte_fields, {nullptr, 1}, core::result::null_arg},
        {{}, {}, core::result::invalid_arg},
        {oversized, {}, core::result::overflow},
    };
    for (const auto &entry : invalid) {
        EXPECT_EQ(entry.result, reader.init(entry.format, entry.input));
        EXPECT_EQ(2u, reader.position());
        auto view = saved;
        auto details = stale;
        std::size_t count = 57;
        EXPECT_EQ(entry.result, io::read(entry.format, entry.input, view, count, &details));
        ExpectView(core::to_c(saved), core::to_c(view));
        EXPECT_EQ(57u, count);
        ExpectDiagnostic({core::to_c(entry.result), TLV_FIELD_NONE, 0, 0, 0}, details);
    }
    core::element_view second{};
    ASSERT_EQ(core::result::ok, reader.next(second));
    ExpectView({{wire + 2, 1}, {wire + sizeof wire, 0}}, core::to_c(second));
    ASSERT_EQ(core::result::ok, reader.init(one_byte_fields, {}));
    EXPECT_EQ(core::result::end, reader.next(second));
    EXPECT_EQ(0u, reader.position());
    ExpectView({{wire + 2, 1}, {wire + sizeof wire, 0}}, core::to_c(second));
}

} // namespace
