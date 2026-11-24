#include <opentlv/core/formats/fixed.h>

#include <gtest/gtest.h>

#include <algorithm>

namespace {

constexpr uint8_t byte = 0xa5;
constexpr tlv_fixed_format_t oneByteFields{{1}, {1, TLV_BYTE_ORDER_BIG_ENDIAN}};
constexpr tlv_element_t emptyElement{{&byte, 1}, {nullptr, 0}};

void ExpectHeaderError(tlv_result_t expected, const tlv_fixed_format_t *format)
{
    size_t size = 37;
    EXPECT_EQ(expected, tlv_fixed_header_size(format, &size));
    EXPECT_EQ(37u, size);
}

void ExpectElementError(tlv_result_t expected, const tlv_fixed_format_t *format,
                        const tlv_element_t *element)
{
    size_t size = 37;
    EXPECT_EQ(expected, tlv_fixed_element_size(format, element, &size));
    EXPECT_EQ(37u, size);
}

TEST(FixedMeasurement, CountsEveryOctetOfAnOpaqueTag)
{
    const uint8_t tag[] = {0x00, 0x00, 0xff, 0x10, 0x80, 0x00, 0x92, 0x40, 0x01, 0x00};
    const uint8_t payload[] = {0x01, 0x00, 0xff};
    for (const auto order : {TLV_BYTE_ORDER_BIG_ENDIAN, TLV_BYTE_ORDER_LITTLE_ENDIAN}) {
        SCOPED_TRACE(order);
        const tlv_fixed_format_t format{{sizeof tag}, {2, order}};
        tlv_element_t element{{tag, sizeof tag}, {payload, sizeof payload}};
        size_t size = 0;
        ASSERT_EQ(TLV_OK, tlv_fixed_header_size(&format, &size));
        EXPECT_EQ(12u, size);
        ASSERT_EQ(TLV_OK, tlv_fixed_element_size(&format, &element, &size));
        EXPECT_EQ(15u, size);

        element.value = {nullptr, 0};
        ASSERT_EQ(TLV_OK, tlv_fixed_element_size(&format, &element, &size));
        EXPECT_EQ(12u, size);
    }
}

TEST(FixedMeasurement, RequiresFormatElementAndOutputPointers)
{
    ExpectHeaderError(TLV_ERR_NULL_ARG, nullptr);
    ExpectElementError(TLV_ERR_NULL_ARG, nullptr, &emptyElement);
    ExpectElementError(TLV_ERR_NULL_ARG, &oneByteFields, nullptr);
    EXPECT_EQ(TLV_ERR_NULL_ARG, tlv_fixed_header_size(&oneByteFields, nullptr));
    EXPECT_EQ(TLV_ERR_NULL_ARG, tlv_fixed_element_size(&oneByteFields, &emptyElement, nullptr));
}

TEST(FixedMeasurement, RejectsInvalidFieldDefinitions)
{
    const tlv_fixed_format_t invalid[] = {
        {{0}, {1, TLV_BYTE_ORDER_BIG_ENDIAN}},
        {{1}, {0, TLV_BYTE_ORDER_LITTLE_ENDIAN}},
        {{1}, {9, TLV_BYTE_ORDER_BIG_ENDIAN}},
        {{1}, {1, TLV_BYTE_ORDER_UNKNOWN}},
    };
    for (const auto &format : invalid) {
        SCOPED_TRACE(::testing::Message() << "tag width " << format.identifier.size
                     << ", length width " << format.length.size
                     << ", order " << format.length.byte_order);
        const tlv_element_t element{{&byte, format.identifier.size}, {nullptr, 0}};
        ExpectHeaderError(TLV_ERR_INVALID_ARG, &format);
        ExpectElementError(TLV_ERR_INVALID_ARG, &format, &element);
    }
}

TEST(FixedMeasurement, MeasuresAnExactNativeHeaderAndRejectsTheNextWidth)
{
    // Measurement uses the descriptors alone, even when the header spans SIZE_MAX octets.
    tlv_fixed_format_t format{{SIZE_MAX - 8}, {8, TLV_BYTE_ORDER_LITTLE_ENDIAN}};
    tlv_element_t element{{&byte, format.identifier.size}, {nullptr, 0}};
    size_t size = 0;
    ASSERT_EQ(TLV_OK, tlv_fixed_header_size(&format, &size));
    EXPECT_EQ(SIZE_MAX, size);
    size = 0;
    ASSERT_EQ(TLV_OK, tlv_fixed_element_size(&format, &element, &size));
    EXPECT_EQ(SIZE_MAX, size);

    ++format.identifier.size;
    element.tag.size = format.identifier.size;
    ExpectHeaderError(TLV_ERR_OVERFLOW, &format);
    ExpectElementError(TLV_ERR_OVERFLOW, &format, &element);
}

TEST(FixedElementSize, RejectsMismatchedTagWidths)
{
    const uint8_t bytes[] = {0x00, 0xa5};
    const tlv_tag_t tags[] = {{nullptr, 0}, {bytes, 0}, {bytes, sizeof bytes}};
    for (const auto &tag : tags) {
        SCOPED_TRACE(tag.size);
        const tlv_element_t element{tag, {nullptr, 0}};
        ExpectElementError(TLV_ERR_INVALID_TAG_SIZE, &oneByteFields, &element);
    }
}

TEST(FixedElementSize, RequiresDataPointersForNonemptyFields)
{
    const tlv_element_t missingTag{{nullptr, 1}, {nullptr, 0}};
    const tlv_element_t missingPayload{{&byte, 1}, {nullptr, 1}};
    ExpectElementError(TLV_ERR_NULL_ARG, &oneByteFields, &missingTag);
    ExpectElementError(TLV_ERR_NULL_ARG, &oneByteFields, &missingPayload);
}

TEST(FixedElementSize, EnforcesWireAndNativePayloadLimitsForEachWidth)
{
    constexpr tlv_size_t wireMaxima[] = {
        0xff, 0xffff, 0xffffff, UINT32_MAX, UINT64_C(0xffffffffff),
        UINT64_C(0xffffffffffff), UINT64_C(0xffffffffffffff), UINT64_MAX,
    };
    for (size_t width = 1; width <= 8; ++width) {
        SCOPED_TRACE(width);
        const tlv_fixed_format_t format{{1}, {width, TLV_BYTE_ORDER_BIG_ENDIAN}};
        const size_t header = 1 + width;
        const tlv_size_t wireMax = wireMaxima[width - 1];
        const tlv_size_t maxPayload = std::min<uintmax_t>(wireMax, SIZE_MAX - header);
        // Only the declared count contributes to measurement. One byte of storage is enough.
        tlv_element_t element{{&byte, 1}, {&byte, maxPayload}};
        size_t size = 0;
        ASSERT_EQ(TLV_OK, tlv_fixed_element_size(&format, &element, &size));
        EXPECT_EQ(header + static_cast<size_t>(maxPayload), size);

        if (maxPayload < TLV_SIZE_MAX) {
            ++element.value.size;
            const auto error = maxPayload == wireMax ? TLV_ERR_INVALID_LENGTH : TLV_ERR_OVERFLOW;
            ExpectElementError(error, &format, &element);
        }
    }
}

#if SIZE_MAX < UINT64_MAX
TEST(FixedElementSize, RejectsAWireLengthAboveTheNativeSizeRange)
{
    const tlv_fixed_format_t format{{1}, {8, TLV_BYTE_ORDER_BIG_ENDIAN}};
    const tlv_element_t element{{&byte, 1}, {&byte, static_cast<tlv_size_t>(SIZE_MAX) + 1}};
    ExpectElementError(TLV_ERR_NATIVE_SIZE, &format, &element);
}
#endif

}  // namespace
