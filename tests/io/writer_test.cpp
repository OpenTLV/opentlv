#include <opentlv/io/reader/reader.h>
#include <opentlv/io/writer/writer.h>

#include "expect_view.h"
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <numeric>
#include <vector>

namespace {

using opentlv_test::ExpectView;

constexpr tlv_fixed_format_t byteFields{{1}, {1, TLV_BYTE_ORDER_BIG_ENDIAN}};
constexpr uint8_t tag = 0x39;
constexpr tlv_element_t emptyElement{{&tag, 1}, {nullptr, 0}};

void ExpectWriterState(const tlv_writer_t &expected, const tlv_writer_t &actual)
{
    EXPECT_EQ(expected.data, actual.data);
    EXPECT_EQ(expected.capacity, actual.capacity);
    EXPECT_EQ(expected.pos, actual.pos);
    EXPECT_EQ(expected.format.identifier.size, actual.format.identifier.size);
    EXPECT_EQ(expected.format.length.size, actual.format.length.size);
    EXPECT_EQ(expected.format.length.byte_order, actual.format.length.byte_order);
}

TEST(IOWrite, EncodesTheTagLengthAndPayloadForEveryLayout)
{
    constexpr uint8_t identifier[] = {0x00, 0xe1};
    constexpr uint8_t bigEndian[] = {0, 0, 0, 0, 0, 0, 0x01, 0x23};
    constexpr uint8_t littleEndian[] = {0x23, 0x01, 0, 0, 0, 0, 0, 0};
    std::array<uint8_t, 0x123> payload;
    std::iota(payload.begin(), payload.end(), 0);

    for (size_t width = 1; width <= 8; ++width) {
        for (auto order : {TLV_BYTE_ORDER_BIG_ENDIAN, TLV_BYTE_ORDER_LITTLE_ENDIAN}) {
            SCOPED_TRACE(testing::Message() << "width " << width << ", order " << order);
            const tlv_fixed_format_t format{{sizeof identifier}, {width, order}};
            const size_t count = width == 1 ? 0x23 : payload.size();
            const tlv_element_t element{{identifier, sizeof identifier}, {payload.data(), count}};
            const uint8_t *length = order == TLV_BYTE_ORDER_BIG_ENDIAN
                                        ? bigEndian + (sizeof bigEndian - width)
                                        : littleEndian;

            const size_t encoded = sizeof identifier + width + count;
            std::vector<uint8_t> expected(encoded + 2, 0xc7);
            auto lengthStart = std::copy_n(identifier, sizeof identifier, expected.begin() + 1);
            auto payloadStart = std::copy_n(length, width, lengthStart);
            std::copy_n(payload.begin(), count, payloadStart);
            std::vector<uint8_t> output(expected.size(), 0xc7);
            size_t written = 0;
            ASSERT_EQ(TLV_OK,
                      tlv_write(output.data() + 1, output.size() - 1, &format, &element, &written));
            ASSERT_EQ(encoded, written);
            EXPECT_EQ(expected, output);

            tlv_element_t decoded{};
            size_t consumed = 0;
            ASSERT_EQ(TLV_OK, tlv_read(output.data() + 1, written, &format, &decoded, &consumed));
            EXPECT_EQ(encoded, consumed);
            const uint8_t *start = output.data() + 1;
            ExpectView({{start, sizeof identifier}, {start + sizeof identifier + width, count}},
                       decoded);
        }
    }
}

TEST(IOWrite, ReportsRequiredCapacityAndRetriesAtExactFit)
{
    const uint8_t payload[] = {0x71, 0x00, 0x92};
    const tlv_element_t element{{&tag, 1}, {payload, sizeof payload}};
    const std::array<uint8_t, 7> original{1, 2, 3, 4, 5, 6, 7};
    const std::array<uint8_t, 7> expected{1, tag, 3, 0x71, 0x00, 0x92, 7};
    size_t required = 99;
    ASSERT_EQ(TLV_ERR_BUFFER_TOO_SHORT, tlv_write(nullptr, 0, &byteFields, &element, &required));
    ASSERT_EQ(5u, required);
    for (size_t capacity = 0; capacity < required; ++capacity) {
        SCOPED_TRACE(capacity);
        auto output = original;
        size_t written = 99;
        EXPECT_EQ(TLV_ERR_BUFFER_TOO_SHORT,
                  tlv_write(output.data() + 1, capacity, &byteFields, &element, &written));
        ASSERT_EQ(required, written);
        EXPECT_EQ(original, output);
        ASSERT_EQ(TLV_OK, tlv_write(output.data() + 1, written, &byteFields, &element, &written));
        EXPECT_EQ(required, written);
        EXPECT_EQ(expected, output);
    }
}

TEST(IOWriter, RetriesAfterShortAppendAndKeepsItsFormatSnapshot)
{
    const uint8_t payload[] = {0xb1, 0xb2};
    const tlv_element_t large{{&tag, 1}, {payload, sizeof payload}};
    tlv_fixed_format_t format = byteFields;
    std::array<uint8_t, 7> output{0xa0, 1, 2, 3, 4, 5, 0xb0};
    tlv_writer_t writer{};
    ASSERT_EQ(TLV_OK, tlv_writer_init(&writer, output.data() + 1, 5, &format));
    format.length.size = 4;
    ASSERT_EQ(TLV_OK, tlv_writer_write(&writer, &emptyElement));
    const std::array<uint8_t, 7> prefix{0xa0, tag, 0, 3, 4, 5, 0xb0};
    ASSERT_EQ(2u, writer.pos);
    ASSERT_EQ(prefix, output);
    const auto committed = writer;
    for (int attempt = 0; attempt < 2; ++attempt) {
        EXPECT_EQ(TLV_ERR_BUFFER_TOO_SHORT, tlv_writer_write(&writer, &large));
        ExpectWriterState(committed, writer);
        EXPECT_EQ(prefix, output);
    }
    const tlv_element_t smaller{{&tag, 1}, {payload, 1}};
    ASSERT_EQ(TLV_OK, tlv_writer_write(&writer, &smaller));
    const std::array<uint8_t, 7> expected{0xa0, tag, 0, tag, 1, 0xb1, 0xb0};
    EXPECT_EQ(5u, writer.pos);
    EXPECT_EQ(expected, output);
}

TEST(IOWriter, AppendsBorrowedBytesFromADisjointCommittedPrefix)
{
    const tlv_fixed_format_t format{{1}, {2, TLV_BYTE_ORDER_LITTLE_ENDIAN}};
    const uint8_t payload[] = {0xf0, 0x0f};
    const tlv_element_t element{{&tag, 1}, {payload, sizeof payload}};
    std::array<uint8_t, 10> output{};
    tlv_writer_t writer{};
    ASSERT_EQ(TLV_OK, tlv_writer_init(&writer, output.data(), output.size(), &format));
    ASSERT_EQ(TLV_OK, tlv_writer_write(&writer, &element));
    ASSERT_EQ(5u, writer.pos);

    tlv_element_t borrowed{};
    size_t consumed = 0;
    ASSERT_EQ(TLV_OK, tlv_read(output.data(), writer.pos, &format, &borrowed, &consumed));
    ASSERT_EQ(5u, consumed);
    ASSERT_EQ(output.data(), borrowed.tag.data);
    ASSERT_EQ(1u, borrowed.tag.size);
    ASSERT_EQ(output.data() + 3, borrowed.value.data);
    ASSERT_EQ(2u, borrowed.value.size);
    tlv_writer_t copy = writer;
    ASSERT_EQ(TLV_OK, tlv_writer_write(&copy, &borrowed));
    const std::array<uint8_t, 10> expected{tag, 2, 0, 0xf0, 0x0f, tag, 2, 0, 0xf0, 0x0f};
    EXPECT_EQ(5u, writer.pos);
    ASSERT_EQ(output.size(), copy.pos);
    EXPECT_EQ(expected, output);
    EXPECT_EQ(TLV_ERR_BUFFER_TOO_SHORT, tlv_writer_write(&copy, &borrowed));
    EXPECT_EQ(output.size(), copy.pos);
    EXPECT_EQ(expected, output);

    ASSERT_EQ(TLV_OK, tlv_writer_init(&copy, output.data(), output.size(), &copy.format));
    const tlv_writer_t restarted{output.data(), output.size(), 0, format};
    ExpectWriterState(restarted, copy);
    EXPECT_EQ(expected, output);
}

TEST(IOWriter, Rejects256BytesAndRetriesWithTheOneByteMaximum)
{
    std::array<uint8_t, 256> payload;
    std::iota(payload.begin(), payload.end(), 0);
    tlv_element_t element{{&tag, 1}, {payload.data(), payload.size()}};
    std::array<uint8_t, 260> output;
    output.fill(0xcc);
    auto expected = output;
    expected[0] = tag;
    expected[1] = 0;
    tlv_writer_t writer{};
    ASSERT_EQ(TLV_OK, tlv_writer_init(&writer, output.data(), output.size(), &byteFields));
    ASSERT_EQ(TLV_OK, tlv_writer_write(&writer, &emptyElement));
    ASSERT_EQ(2u, writer.pos);
    EXPECT_EQ(TLV_ERR_INVALID_LENGTH, tlv_writer_write(&writer, &element));
    EXPECT_EQ(2u, writer.pos);
    EXPECT_EQ(expected, output);

    element.value.size = 255;
    ASSERT_EQ(TLV_OK, tlv_writer_write(&writer, &element));
    expected[2] = tag;
    expected[3] = 0xff;
    std::copy_n(payload.begin(), 255, expected.begin() + 4);
    ASSERT_EQ(259u, writer.pos);
    EXPECT_EQ(expected, output);

    tlv_reader_t reader{};
    tlv_element_t decoded{};
    ASSERT_EQ(TLV_OK, tlv_reader_init(&reader, output.data(), writer.pos, &byteFields));
    ASSERT_EQ(TLV_OK, tlv_reader_next(&reader, &decoded));
    EXPECT_EQ(2u, reader.pos);
    ExpectView({{output.data(), 1}, {output.data() + 2, 0}}, decoded);
    ASSERT_EQ(TLV_OK, tlv_reader_next(&reader, &decoded));
    EXPECT_EQ(writer.pos, reader.pos);
    ExpectView({{output.data() + 2, 1}, {output.data() + 4, 255}}, decoded);
    EXPECT_EQ(TLV_END, tlv_reader_next(&reader, &decoded));
}

TEST(IOWriter, InvalidElementsPreserveEveryByteAndOutput)
{
    const tlv_element_t wrongWidth{{&tag, 0}, {nullptr, 0}};
    const tlv_element_t missingTag{{nullptr, 1}, {nullptr, 0}};
    const tlv_element_t missingPayload{{&tag, 1}, {nullptr, 1}};
    const struct {
        const tlv_element_t *element;
        tlv_result_t error;
    } cases[] = {{nullptr, TLV_ERR_NULL_ARG},
                 {&wrongWidth, TLV_ERR_INVALID_TAG_SIZE},
                 {&missingTag, TLV_ERR_NULL_ARG},
                 {&missingPayload, TLV_ERR_NULL_ARG}};
    std::array<uint8_t, 8> output{9, 8, 7, 6, 5, 4, 3, 2};
    tlv_writer_t writer{};
    ASSERT_EQ(TLV_OK, tlv_writer_init(&writer, output.data(), output.size(), &byteFields));
    ASSERT_EQ(TLV_OK, tlv_writer_write(&writer, &emptyElement));
    const auto committed = writer;
    const auto bytes = output;
    for (const auto &entry : cases) {
        size_t written = 99;
        EXPECT_EQ(entry.error,
                  tlv_write(output.data(), output.size(), &byteFields, entry.element, &written));
        EXPECT_EQ(99u, written);
        EXPECT_EQ(bytes, output);
        EXPECT_EQ(entry.error, tlv_writer_write(&writer, entry.element));
        ExpectWriterState(committed, writer);
        EXPECT_EQ(bytes, output);
    }
}

TEST(IOWriter, RejectsMissingPointersInvalidFormatsAndOutOfRangePositions)
{
    const tlv_fixed_format_t invalid{{1}, {0, TLV_BYTE_ORDER_BIG_ENDIAN}};
    const tlv_fixed_format_t overflowing{{SIZE_MAX}, {1, TLV_BYTE_ORDER_BIG_ENDIAN}};
    std::array<uint8_t, 4> output{9, 8, 7, 6};
    tlv_writer_t writer{};
    ASSERT_EQ(TLV_OK, tlv_writer_init(&writer, output.data(), output.size(), &byteFields));
    ASSERT_EQ(TLV_OK, tlv_writer_write(&writer, &emptyElement));
    const auto committed = writer;
    const auto bytes = output;
    size_t written = 99;
    EXPECT_EQ(TLV_ERR_NULL_ARG, tlv_write(nullptr, 1, &byteFields, &emptyElement, &written));
    EXPECT_EQ(TLV_ERR_NULL_ARG,
              tlv_write(output.data(), output.size(), nullptr, &emptyElement, &written));
    EXPECT_EQ(TLV_ERR_NULL_ARG,
              tlv_write(output.data(), output.size(), &byteFields, &emptyElement, nullptr));
    EXPECT_EQ(TLV_ERR_INVALID_ARG,
              tlv_write(output.data(), output.size(), &invalid, &emptyElement, &written));
    EXPECT_EQ(99u, written);
    EXPECT_EQ(bytes, output);

    EXPECT_EQ(TLV_ERR_NULL_ARG,
              tlv_writer_init(nullptr, output.data(), output.size(), &byteFields));
    EXPECT_EQ(TLV_ERR_NULL_ARG, tlv_writer_init(&writer, nullptr, 1, &byteFields));
    EXPECT_EQ(TLV_ERR_NULL_ARG, tlv_writer_init(&writer, nullptr, 0, nullptr));
    EXPECT_EQ(TLV_ERR_INVALID_ARG, tlv_writer_init(&writer, nullptr, 0, &invalid));
    EXPECT_EQ(TLV_ERR_OVERFLOW, tlv_writer_init(&writer, nullptr, 0, &overflowing));
    ExpectWriterState(committed, writer);
    EXPECT_EQ(bytes, output);

    EXPECT_EQ(TLV_ERR_NULL_ARG, tlv_writer_write(nullptr, &emptyElement));
    writer.pos = writer.capacity + 1;
    const auto invalidPosition = writer;
    EXPECT_EQ(TLV_ERR_INVALID_ARG, tlv_writer_write(&writer, &emptyElement));
    ExpectWriterState(invalidPosition, writer);
    EXPECT_EQ(bytes, output);
    ASSERT_EQ(TLV_OK, tlv_writer_init(&writer, nullptr, 0, &byteFields));
    EXPECT_EQ(TLV_ERR_BUFFER_TOO_SHORT, tlv_writer_write(&writer, &emptyElement));
    EXPECT_EQ(0u, writer.pos);
}

TEST(IOWrite, SizingQueriesValidateWireAndNativeLimits)
{
    const tlv_fixed_format_t wide{{1}, {8, TLV_BYTE_ORDER_BIG_ENDIAN}};
    const tlv_fixed_format_t overflowing{{SIZE_MAX}, {1, TLV_BYTE_ORDER_BIG_ENDIAN}};
    const tlv_element_t hugeHeader{{&tag, SIZE_MAX}, {nullptr, 0}};
    tlv_element_t element{{&tag, 1}, {&tag, 256}};
    size_t written = 99;
    EXPECT_EQ(TLV_ERR_INVALID_LENGTH, tlv_write(nullptr, 0, &byteFields, &element, &written));
    EXPECT_EQ(TLV_ERR_OVERFLOW, tlv_write(nullptr, 0, &overflowing, &hugeHeader, &written));
#if SIZE_MAX <= UINT64_MAX
    element.value.size = SIZE_MAX;
    EXPECT_EQ(TLV_ERR_OVERFLOW, tlv_write(nullptr, 0, &wide, &element, &written));
#endif
#if SIZE_MAX < UINT64_MAX
    element.value.size = static_cast<tlv_size_t>(SIZE_MAX) + 1;
    EXPECT_EQ(TLV_ERR_NATIVE_SIZE, tlv_write(nullptr, 0, &wide, &element, &written));
#endif
    EXPECT_EQ(99u, written);
}

} // namespace
