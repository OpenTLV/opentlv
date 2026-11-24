#include <opentlv/tlv.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <random>
#include <string>
#include <vector>

namespace {

using Bytes = std::vector<uint8_t>;

// Each literal is the wire encoding of a three-byte payload length.
struct Layout {
    tlv_byte_order_t order;
    Bytes lengthOfThree;
};

const Layout layouts[] = {
    {TLV_BYTE_ORDER_BIG_ENDIAN, {3}},                   {TLV_BYTE_ORDER_LITTLE_ENDIAN, {3}},
    {TLV_BYTE_ORDER_BIG_ENDIAN, {0,3}},                 {TLV_BYTE_ORDER_LITTLE_ENDIAN, {3,0}},
    {TLV_BYTE_ORDER_BIG_ENDIAN, {0,0,3}},               {TLV_BYTE_ORDER_LITTLE_ENDIAN, {3,0,0}},
    {TLV_BYTE_ORDER_BIG_ENDIAN, {0,0,0,3}},             {TLV_BYTE_ORDER_LITTLE_ENDIAN, {3,0,0,0}},
    {TLV_BYTE_ORDER_BIG_ENDIAN, {0,0,0,0,3}},           {TLV_BYTE_ORDER_LITTLE_ENDIAN, {3,0,0,0,0}},
    {TLV_BYTE_ORDER_BIG_ENDIAN, {0,0,0,0,0,3}},         {TLV_BYTE_ORDER_LITTLE_ENDIAN, {3,0,0,0,0,0}},
    {TLV_BYTE_ORDER_BIG_ENDIAN, {0,0,0,0,0,0,3}},       {TLV_BYTE_ORDER_LITTLE_ENDIAN, {3,0,0,0,0,0,0}},
    {TLV_BYTE_ORDER_BIG_ENDIAN, {0,0,0,0,0,0,0,3}},     {TLV_BYTE_ORDER_LITTLE_ENDIAN, {3,0,0,0,0,0,0,0}},
};

class FixedRoundTrip : public testing::TestWithParam<Layout> {
protected:
    const tlv_fixed_length_t length{GetParam().lengthOfThree.size(), GetParam().order};
    const tlv_fixed_format_t format{{2}, length};

    Bytes GoldenRecord() const
    {
        const Bytes &field = GetParam().lengthOfThree;
        Bytes wire{0x00, 0xa7};
        wire.reserve(2 + field.size() + 3);
        wire.insert(wire.end(), field.begin(), field.end());
        wire.insert(wire.end(), {0x00, 0xff, 0x80});
        return wire;
    }
};

TEST_P(FixedRoundTrip, ReadsAndWritesGoldenBytes)
{
    const Bytes golden = GoldenRecord();
    const uint8_t tag[] = {0x00, 0xa7};
    const uint8_t value[] = {0x00, 0xff, 0x80};
    const tlv_element_t source{{tag, sizeof tag}, {value, sizeof value}};
    Bytes encoded(golden.size(), 0xcc);
    size_t written = 0;
    ASSERT_EQ(TLV_OK, tlv_write(encoded.data(), encoded.size(), &format, &source, &written));
    EXPECT_EQ(golden.size(), written);
    EXPECT_EQ(golden, encoded);

    tlv_element_t decoded{};
    size_t consumed = 0;
    ASSERT_EQ(TLV_OK, tlv_read(golden.data(), golden.size(), &format, &decoded, &consumed));
    EXPECT_EQ(golden.size(), consumed);
    ASSERT_EQ(golden.data(), decoded.tag.data);
    ASSERT_EQ(2u, decoded.tag.size);
    ASSERT_EQ(golden.data() + golden.size() - 3, decoded.value.data);
    ASSERT_EQ(3u, decoded.value.size);

}

TEST_P(FixedRoundTrip, RoundTripsBoundedBinaryValuesAtLengthBoundaries)
{
    std::mt19937 random(0x26f1u);
    for (const size_t tagWidth : {1u, 2u, 9u}) {
        const tlv_fixed_format_t layout{{tagWidth}, length};
        for (const size_t count : {0u, 1u, 127u, 254u, 255u, 256u, 257u, 513u}) {
            SCOPED_TRACE(testing::Message() << "tag bytes " << tagWidth << ", value bytes " << count);
            Bytes tag(tagWidth), value(count);
            const auto nextByte = [&random] { return static_cast<uint8_t>(random()); };
            std::generate(tag.begin(), tag.end(), nextByte);
            std::generate(value.begin(), value.end(), nextByte);
            const tlv_element_t source{{tag.data(), tag.size()}, {value.data(), count}};
            const size_t header = tagWidth + length.size;
            Bytes output(header + count, 0xcc);
            size_t written = 93;
            if (length.size == 1 && count > 255) {
                const Bytes before = output;
                EXPECT_EQ(TLV_ERR_INVALID_LENGTH,
                          tlv_write(output.data(), output.size(), &layout, &source, &written));
                EXPECT_EQ(93u, written);
                EXPECT_EQ(before, output);
                continue;
            }

            // The bounded corpus uses at most two significant length octets.
            Bytes field(length.size, 0);
            field.front() = static_cast<uint8_t>(count % 256);
            if (field.size() > 1) {
                field[1] = static_cast<uint8_t>(count / 256);
            }
            if (length.byte_order == TLV_BYTE_ORDER_BIG_ENDIAN) {
                std::reverse(field.begin(), field.end());
            }
            size_t measured = 0;
            ASSERT_EQ(TLV_OK, tlv_fixed_element_size(&layout, &source, &measured));
            EXPECT_EQ(output.size(), measured);
            ASSERT_EQ(TLV_OK, tlv_write(output.data(), output.size(), &layout, &source, &written));
            ASSERT_EQ(output.size(), written);
            EXPECT_TRUE(std::equal(tag.begin(), tag.end(), output.begin()));
            EXPECT_TRUE(std::equal(field.begin(), field.end(), output.begin() + tagWidth));
            EXPECT_TRUE(std::equal(value.begin(), value.end(), output.begin() + header));

            tlv_element_t decoded{};
            size_t consumed = 0;
            ASSERT_EQ(TLV_OK, tlv_read(output.data(), written, &layout, &decoded, &consumed));
            EXPECT_EQ(written, consumed);
            EXPECT_EQ(output.data(), decoded.tag.data);
            EXPECT_EQ(tagWidth, decoded.tag.size);
            EXPECT_EQ(output.data() + header, decoded.value.data);
            EXPECT_EQ(count, decoded.value.size);
        }
    }
}

TEST_P(FixedRoundTrip, CommitsOnlyCompleteRecordsInAnAdjacentStream)
{
    const uint8_t firstTag[] = {0xfe, 0x00};
    const tlv_element_t empty{{firstTag, 2}, {nullptr, 0}};
    Bytes golden(2 + length.size, 0);
    golden[0] = 0xfe;
    const size_t boundary = golden.size();
    const Bytes second = GoldenRecord();
    golden.insert(golden.end(), second.begin(), second.end());
    const tlv_element_t binary{{second.data(), 2}, {second.data() + second.size() - 3, 3}};

    Bytes stream(golden.size(), 0xcc);
    tlv_writer_t writer{};
    ASSERT_EQ(TLV_OK, tlv_writer_init(&writer, stream.data(), stream.size(), &format));
    ASSERT_EQ(TLV_OK, tlv_writer_write(&writer, &empty));
    ASSERT_EQ(boundary, writer.pos);
    ASSERT_EQ(TLV_OK, tlv_writer_write(&writer, &binary));
    ASSERT_EQ(golden.size(), writer.pos);
    ASSERT_EQ(golden, stream);

    for (size_t suffix = 0; suffix <= second.size(); ++suffix) {
        SCOPED_TRACE(suffix);
        tlv_reader_t reader{};
        tlv_element_t view{};
        ASSERT_EQ(TLV_OK, tlv_reader_init(&reader, stream.data(), boundary + suffix, &format));
        ASSERT_EQ(TLV_OK, tlv_reader_next(&reader, &view));
        EXPECT_EQ(boundary, reader.pos);
        EXPECT_EQ(stream.data(), view.tag.data);
        EXPECT_EQ(2u, view.tag.size);
        EXPECT_EQ(stream.data() + boundary, view.value.data);
        EXPECT_EQ(0u, view.value.size);

        if (suffix == second.size()) {
            ASSERT_EQ(TLV_OK, tlv_reader_next(&reader, &view));
            EXPECT_EQ(golden.size(), reader.pos);
            EXPECT_EQ(stream.data() + boundary, view.tag.data);
            EXPECT_EQ(2u, view.tag.size);
            EXPECT_EQ(stream.data() + stream.size() - 3, view.value.data);
            EXPECT_EQ(3u, view.value.size);
            EXPECT_EQ(TLV_END, tlv_reader_next(&reader, &view));
        } else {
            EXPECT_EQ(suffix == 0 ? TLV_END : TLV_ERR_TRUNCATED, tlv_reader_next(&reader, &view));
            EXPECT_EQ(boundary, reader.pos);
            EXPECT_EQ(stream.data(), view.tag.data);
            EXPECT_EQ(2u, view.tag.size);
            EXPECT_EQ(stream.data() + boundary, view.value.data);
            EXPECT_EQ(0u, view.value.size);
        }
    }
}

INSTANTIATE_TEST_SUITE_P(WireLayouts, FixedRoundTrip, testing::ValuesIn(layouts),
    [](const testing::TestParamInfo<Layout> &info) {
        const std::string order = info.param.order == TLV_BYTE_ORDER_BIG_ENDIAN ? "BigEndian" : "LittleEndian";
        return order + std::to_string(info.param.lengthOfThree.size());
    });

}  // namespace
