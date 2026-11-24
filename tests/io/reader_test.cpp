#include <opentlv/io/reader/reader.h>

#include "expect_view.h"
#include <gtest/gtest.h>

#include <string>
#include <tuple>
#include <vector>

namespace {

using opentlv_test::ExpectView;

constexpr tlv_fixed_format_t oneByteFields{{1}, {1, TLV_BYTE_ORDER_BIG_ENDIAN}};
constexpr uint8_t markers[] = {0x99, 0x98, 0x97, 0x96, 0x95};
constexpr tlv_element_t sentinel{{markers, 3}, {markers + 3, 2}};

void ExpectReader(const tlv_reader_t &actual, const tlv_reader_t &expected)
{
    EXPECT_EQ(expected.data, actual.data);
    EXPECT_EQ(expected.size, actual.size);
    EXPECT_EQ(expected.pos, actual.pos);
    EXPECT_EQ(expected.format.identifier.size, actual.format.identifier.size);
    EXPECT_EQ(expected.format.length.size, actual.format.length.size);
    EXPECT_EQ(expected.format.length.byte_order, actual.format.length.byte_order);
}

void ExpectReadFailure(const uint8_t *data, size_t size, const tlv_fixed_format_t *format,
                       tlv_result_t expected)
{
    tlv_element_t out = sentinel;
    size_t consumed = 79;
    EXPECT_EQ(expected, tlv_read(data, size, format, &out, &consumed));
    EXPECT_EQ(79u, consumed);
    ExpectView(sentinel, out);
}

void ExpectStopped(tlv_reader_t &reader, tlv_result_t expected, tlv_element_t previous)
{
    const tlv_reader_t before = reader;
    tlv_element_t out = previous;
    for (int attempt = 0; attempt < 2; ++attempt) {
        ASSERT_EQ(expected, tlv_reader_next(&reader, &out));
        ExpectReader(reader, before);
        ExpectView(previous, out);
    }
}

using Layout = std::tuple<size_t, tlv_byte_order_t>;

class ReaderLayout : public testing::TestWithParam<Layout> {
protected:
    const tlv_fixed_format_t format{{3}, {std::get<0>(GetParam()), std::get<1>(GetParam())}};

    std::vector<uint8_t> Record() const
    {
        std::vector<uint8_t> wire{0x00, 0xb4, 0x10};
        const size_t lengthStart = wire.size();
        wire.resize(lengthStart + format.length.size, 0);
        const size_t lowByte = format.length.byte_order == TLV_BYTE_ORDER_BIG_ENDIAN
            ? wire.size() - 1 : lengthStart;
        wire[lowByte] = 3;
        wire.insert(wire.end(), {0x21, 0x01, 0xfe});
        return wire;
    }
};

TEST_P(ReaderLayout, BorrowsOnlyTheFirstElement)
{
    auto wire = Record();
    const size_t recordSize = wire.size();
    wire.push_back(0xaa);
    const tlv_element_t expected{{wire.data(), 3}, {wire.data() + recordSize - 3, 3}};
    tlv_element_t out{};
    size_t consumed = 0;
    ASSERT_EQ(TLV_OK, tlv_read(wire.data(), wire.size(), &format, &out, &consumed));
    ExpectView(expected, out);
    EXPECT_EQ(recordSize, consumed);

    tlv_reader_t reader{};
    ASSERT_EQ(TLV_OK, tlv_reader_init(&reader, wire.data(), wire.size(), &format));
    out = sentinel;
    ASSERT_EQ(TLV_OK, tlv_reader_next(&reader, &out));
    ExpectView(expected, out);
    EXPECT_EQ(recordSize, reader.pos);
    ExpectStopped(reader, TLV_ERR_TRUNCATED, out);
}

TEST_P(ReaderLayout, PreservesOutputsAtEveryIncompletePrefix)
{
    const auto wire = Record();
    for (size_t size = 0; size < wire.size(); ++size) {
        SCOPED_TRACE(size);
        const auto expected = size == 0 ? TLV_END : TLV_ERR_TRUNCATED;
        ExpectReadFailure(wire.data(), size, &format, expected);
        tlv_reader_t reader{};
        ASSERT_EQ(TLV_OK, tlv_reader_init(&reader, wire.data(), size, &format));
        ExpectStopped(reader, expected, sentinel);
    }
}

INSTANTIATE_TEST_SUITE_P(WireLayouts, ReaderLayout,
    testing::Combine(testing::Values(1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u),
                     testing::Values(TLV_BYTE_ORDER_BIG_ENDIAN, TLV_BYTE_ORDER_LITTLE_ENDIAN)),
    [](const testing::TestParamInfo<Layout> &info) {
        const std::string order = std::get<1>(info.param) == TLV_BYTE_ORDER_BIG_ENDIAN
            ? "BigEndian" : "LittleEndian";
        return order + std::to_string(std::get<0>(info.param));
    });

TEST(ReaderStream, CopiesTheFormatAndAdvancesIndependentCursors)
{
    const uint8_t wire[] = {0x71, 3, 0x01, 1, 0xf7, 0x22, 2, 0xde, 0xad, 0x08, 0};
    tlv_reader_t reader{};
    {
        tlv_fixed_format_t temporaryFormat = oneByteFields;
        ASSERT_EQ(TLV_OK, tlv_reader_init(&reader, wire, sizeof wire, &temporaryFormat));
    }
    ExpectReader(reader, {wire, sizeof wire, 0, oneByteFields});
    tlv_element_t first{};
    ASSERT_EQ(TLV_OK, tlv_reader_next(&reader, &first));
    ExpectView({{wire, 1}, {wire + 2, 3}}, first);
    ASSERT_EQ(5u, reader.pos);

    tlv_reader_t copy = reader;
    tlv_element_t second{}, last{};
    ASSERT_EQ(TLV_OK, tlv_reader_next(&reader, &second));
    ExpectView({{wire + 5, 1}, {wire + 7, 2}}, second);
    EXPECT_EQ(9u, reader.pos);
    EXPECT_EQ(5u, copy.pos);
    ASSERT_EQ(TLV_OK, tlv_reader_next(&reader, &last));
    ExpectView({{wire + 9, 1}, {wire + sizeof wire, 0}}, last);
    EXPECT_EQ(sizeof wire, reader.pos);
    ExpectStopped(reader, TLV_END, last);

    tlv_element_t copied{};
    ASSERT_EQ(TLV_OK, tlv_reader_next(&copy, &copied));
    ExpectView(second, copied);
    EXPECT_EQ(9u, copy.pos);

    ASSERT_EQ(TLV_OK, tlv_reader_init(&reader, wire + 9, 2, &reader.format));
    ExpectReader(reader, {wire + 9, 2, 0, oneByteFields});
    size_t consumed = 0;
    ASSERT_EQ(TLV_OK, tlv_read(reader.data, reader.size, &reader.format, &copied, &consumed));
    ExpectView(last, copied);
    EXPECT_EQ(2u, consumed);
    ExpectView({{wire, 1}, {wire + 2, 3}}, first);
}

TEST(ReaderBounds, ComparesHugeLengthsBeforeAnyNativeSizeConversion)
{
    const tlv_fixed_format_t format{{1}, {8, TLV_BYTE_ORDER_BIG_ENDIAN}};
    const uint8_t records[][9] = {
        {0x0f, 0, 0, 0, 1, 0, 0, 0, 0}, // 2^32, whose low 32 bits are zero.
        {0x0f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
    };
    for (const auto &wire : records) {
        SCOPED_TRACE(static_cast<unsigned>(wire[1]));
        ExpectReadFailure(wire, sizeof wire, &format, TLV_ERR_TRUNCATED);
        tlv_reader_t reader{};
        ASSERT_EQ(TLV_OK, tlv_reader_init(&reader, wire, sizeof wire, &format));
        ExpectStopped(reader, TLV_ERR_TRUNCATED, sentinel);
    }
}

TEST(ReaderBounds, AcceptsANullEmptyInputAndRejectsAnOutOfRangePosition)
{
    ExpectReadFailure(nullptr, 0, &oneByteFields, TLV_END);
    tlv_reader_t reader{};
    ASSERT_EQ(TLV_OK, tlv_reader_init(&reader, nullptr, 0, &oneByteFields));
    ExpectStopped(reader, TLV_END, sentinel);
    reader.pos = 1;
    ExpectStopped(reader, TLV_ERR_INVALID_ARG, sentinel);
}

TEST(ReaderArguments, RequiresPointersAndPreservesSuppliedOutputs)
{
    const uint8_t wire[] = {0x2a, 0};
    ExpectReadFailure(nullptr, 1, &oneByteFields, TLV_ERR_NULL_ARG);
    ExpectReadFailure(nullptr, 0, nullptr, TLV_ERR_NULL_ARG);
    size_t consumed = 79;
    tlv_element_t out = sentinel;
    EXPECT_EQ(TLV_ERR_NULL_ARG, tlv_read(wire, sizeof wire, &oneByteFields, nullptr, &consumed));
    EXPECT_EQ(79u, consumed);
    EXPECT_EQ(TLV_ERR_NULL_ARG, tlv_read(wire, sizeof wire, &oneByteFields, &out, nullptr));
    ExpectView(sentinel, out);
    EXPECT_EQ(TLV_ERR_NULL_ARG, tlv_reader_next(nullptr, &out));
    ExpectView(sentinel, out);
    EXPECT_EQ(TLV_ERR_NULL_ARG, tlv_reader_init(nullptr, wire, sizeof wire, &oneByteFields));
    tlv_reader_t reader{};
    ASSERT_EQ(TLV_OK, tlv_reader_init(&reader, wire, sizeof wire, &oneByteFields));
    const auto before = reader;
    EXPECT_EQ(TLV_ERR_NULL_ARG, tlv_reader_next(&reader, nullptr));
    ExpectReader(reader, before);
}

TEST(ReaderArguments, InvalidInitializationPreservesTheCurrentStream)
{
    const uint8_t wire[] = {0xb1, 1, 0x11, 0xb2, 0};
    tlv_reader_t reader{};
    tlv_element_t out{};
    ASSERT_EQ(TLV_OK, tlv_reader_init(&reader, wire, sizeof wire, &oneByteFields));
    ASSERT_EQ(TLV_OK, tlv_reader_next(&reader, &out));
    ASSERT_EQ(3u, reader.pos);
    const auto before = reader;
    EXPECT_EQ(TLV_ERR_NULL_ARG, tlv_reader_init(&reader, nullptr, 1, &oneByteFields));
    ExpectReader(reader, before);
    EXPECT_EQ(TLV_ERR_NULL_ARG, tlv_reader_init(&reader, nullptr, 0, nullptr));
    ExpectReader(reader, before);
    const struct {
        tlv_fixed_format_t format;
        tlv_result_t expected;
    } invalid[] = {
        {{{0}, {1, TLV_BYTE_ORDER_BIG_ENDIAN}}, TLV_ERR_INVALID_ARG},
        {{{1}, {0, TLV_BYTE_ORDER_BIG_ENDIAN}}, TLV_ERR_INVALID_ARG},
        {{{1}, {9, TLV_BYTE_ORDER_BIG_ENDIAN}}, TLV_ERR_INVALID_ARG},
        {{{1}, {1, TLV_BYTE_ORDER_UNKNOWN}}, TLV_ERR_INVALID_ARG},
        {{{1}, {1, static_cast<tlv_byte_order_t>(3)}}, TLV_ERR_INVALID_ARG},
        {{{SIZE_MAX}, {1, TLV_BYTE_ORDER_BIG_ENDIAN}}, TLV_ERR_OVERFLOW},
    };
    for (const auto &entry : invalid) {
        SCOPED_TRACE(testing::Message() << entry.format.identifier.size << " tag bytes, "
                     << entry.format.length.size << " length bytes, order " << entry.format.length.byte_order);
        ExpectReadFailure(nullptr, 0, &entry.format, entry.expected);
        EXPECT_EQ(entry.expected, tlv_reader_init(&reader, nullptr, 0, &entry.format));
        ExpectReader(reader, before);
    }
    ASSERT_EQ(TLV_OK, tlv_reader_next(&reader, &out));
    ExpectView({{wire + 3, 1}, {wire + sizeof wire, 0}}, out);
}

}  // namespace
