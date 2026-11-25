#include <opentlv/io/reader/reader.h>
#include <opentlv/io/writer/writer.h>

#include "expect_view.h"
#include <gtest/gtest.h>

#include <array>

namespace {

using opentlv_test::ExpectView;

constexpr tlv_fixed_format_t one_byte_fields{{1}, {1, TLV_BYTE_ORDER_BIG_ENDIAN}};
constexpr tlv_diagnostic_t stale{TLV_ERR_INVALID_TAG_SIZE, TLV_FIELD_TAG, 19, 27, 8};
constexpr uint8_t saved_bytes[] = {0x42, 0x93};
constexpr tlv_element_t saved_view{{saved_bytes, 1}, {saved_bytes + 1, 1}};

void ExpectDiagnostic(const tlv_diagnostic_t &diagnostic, tlv_result_t result,
                      tlv_field_t field = TLV_FIELD_NONE, size_t offset = 0,
                      uintmax_t required = 0, uintmax_t available = 0)
{
    EXPECT_EQ(result, diagnostic.result);
    EXPECT_EQ(field, diagnostic.field);
    EXPECT_EQ(offset, diagnostic.offset);
    EXPECT_EQ(required, diagnostic.required);
    EXPECT_EQ(available, diagnostic.available);
}

TEST(ReadDiagnostic, LocatesEachIncompleteFieldFromTheStartOfTheSuppliedBuffer)
{
    const tlv_fixed_format_t format{{3}, {2, TLV_BYTE_ORDER_BIG_ENDIAN}};
    const uint8_t wire[] = {
        0x00, 0x89, 0x01, 0x00, 0x01, 0xe1,
        0xff, 0x00, 0xa0, 0x00, 0x03, 0x7f, 0xa5, 0x00
    };
    constexpr size_t second = 6;
    const tlv_element_t first{{wire, 3}, {wire + 5, 1}};
    const struct {
        size_t prefix;
        tlv_field_t field;
        size_t offset;
        uintmax_t required;
        uintmax_t available;
    } cuts[] = {
        {1, TLV_FIELD_TAG,    0, 3, 1}, {2, TLV_FIELD_TAG,    0, 3, 2},
        {3, TLV_FIELD_LENGTH, 3, 2, 0}, {4, TLV_FIELD_LENGTH, 3, 2, 1},
        {5, TLV_FIELD_VALUE,  5, 3, 0}, {6, TLV_FIELD_VALUE,  5, 3, 1},
        {7, TLV_FIELD_VALUE,  5, 3, 2}
    };
    for (const auto &cut : cuts) {
        SCOPED_TRACE(cut.prefix);
        tlv_element_t view = saved_view;
        size_t consumed = 91;
        tlv_diagnostic_t diagnostic = stale;
        EXPECT_EQ(TLV_ERR_TRUNCATED, tlv_read_with_diagnostic(
            wire + second, cut.prefix, &format, &view, &consumed, &diagnostic));
        ExpectDiagnostic(diagnostic, TLV_ERR_TRUNCATED, cut.field, cut.offset, cut.required, cut.available);
        ExpectView(saved_view, view);
        EXPECT_EQ(91u, consumed);
        EXPECT_EQ(TLV_ERR_TRUNCATED, tlv_read_with_diagnostic(
            wire + second, cut.prefix, &format, &view, &consumed, nullptr));
        ExpectView(saved_view, view);
        EXPECT_EQ(91u, consumed);

        tlv_reader_t reader{};
        ASSERT_EQ(TLV_OK, tlv_reader_init(&reader, wire, second + cut.prefix, &format));
        ASSERT_EQ(TLV_OK, tlv_reader_next_with_diagnostic(&reader, &view, &diagnostic));
        ExpectDiagnostic(diagnostic, TLV_OK);
        ExpectView(first, view);
        ASSERT_EQ(second, reader.pos);
        EXPECT_EQ(TLV_ERR_TRUNCATED, tlv_reader_next_with_diagnostic(&reader, &view, &diagnostic));
        ExpectDiagnostic(diagnostic, TLV_ERR_TRUNCATED, cut.field, second + cut.offset,
                         cut.required, cut.available);
        ExpectView(first, view);
        EXPECT_EQ(second, reader.pos);
        EXPECT_EQ(TLV_ERR_TRUNCATED, tlv_reader_next_with_diagnostic(&reader, &view, nullptr));
        ExpectView(first, view);
        EXPECT_EQ(second, reader.pos);
    }
}

TEST(ReadDiagnostic, AcceptsEverySingleByteTagAndLengthAndClearsFieldDetails)
{
    for (unsigned n = 0; n <= 255; ++n) {
        SCOPED_TRACE(n);
        std::array<uint8_t, 257> wire{};
        wire[0] = static_cast<uint8_t>(255 - n);
        wire[1] = static_cast<uint8_t>(n);
        tlv_diagnostic_t diagnostic = stale;
        tlv_element_t view = saved_view;
        size_t consumed = 0;
        ASSERT_EQ(TLV_OK, tlv_read_with_diagnostic(
            wire.data(), 2 + n, &one_byte_fields, &view, &consumed, &diagnostic));
        ExpectDiagnostic(diagnostic, TLV_OK);
        ExpectView({{wire.data(), 1}, {wire.data() + 2, n}}, view);
        EXPECT_EQ(2 + n, consumed);
    }
}

TEST(ReadDiagnostic, ReportsTheFullWireLengthWhenItExceedsTheRemainingInput)
{
    const struct {
        tlv_byte_order_t order;
        std::array<uint8_t, 10> wire;
        tlv_size_t declared;
    } cases[] = {
        {TLV_BYTE_ORDER_BIG_ENDIAN, {0x11, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x51},
         UINT64_MAX},
        {TLV_BYTE_ORDER_LITTLE_ENDIAN, {0x11, 1, 0, 0, 0, 1, 0, 0, 0, 0x51}, UINT64_C(0x100000001)}
    };
    for (const auto &entry : cases) {
        SCOPED_TRACE(entry.declared);
        const tlv_fixed_format_t format{{1}, {8, entry.order}};
        tlv_diagnostic_t diagnostic = stale;
        tlv_element_t view = saved_view;
        size_t consumed = 91;
        EXPECT_EQ(TLV_ERR_TRUNCATED, tlv_read_with_diagnostic(
            entry.wire.data(), entry.wire.size(), &format, &view, &consumed, &diagnostic));
        ExpectDiagnostic(diagnostic, TLV_ERR_TRUNCATED, TLV_FIELD_VALUE, 9, entry.declared, 1);
        ExpectView(saved_view, view);
        EXPECT_EQ(91u, consumed);
        EXPECT_EQ(TLV_ERR_TRUNCATED, tlv_read(
            entry.wire.data(), entry.wire.size(), &format, &view, &consumed));
        ExpectView(saved_view, view);
        EXPECT_EQ(91u, consumed);
    }
}

TEST(ReadDiagnostic, ClearsStaleDetailsOnEndAndInvalidArguments)
{
    tlv_element_t view = saved_view;
    size_t consumed = 91;
    tlv_diagnostic_t diagnostic = stale;
    EXPECT_EQ(TLV_END, tlv_read_with_diagnostic(nullptr, 0, &one_byte_fields, &view, &consumed, &diagnostic));
    ExpectDiagnostic(diagnostic, TLV_END);
    ExpectView(saved_view, view);
    EXPECT_EQ(91u, consumed);
    diagnostic = stale;
    EXPECT_EQ(TLV_ERR_NULL_ARG, tlv_read_with_diagnostic(
        nullptr, 1, &one_byte_fields, &view, &consumed, &diagnostic));
    ExpectDiagnostic(diagnostic, TLV_ERR_NULL_ARG);
    ExpectView(saved_view, view);
    EXPECT_EQ(91u, consumed);

    const tlv_fixed_format_t oversized_header{{SIZE_MAX}, {1, TLV_BYTE_ORDER_BIG_ENDIAN}};
    diagnostic = stale;
    EXPECT_EQ(TLV_ERR_OVERFLOW, tlv_read_with_diagnostic(
        nullptr, 0, &oversized_header, &view, &consumed, &diagnostic));
    ExpectDiagnostic(diagnostic, TLV_ERR_OVERFLOW);
    ExpectView(saved_view, view);
    EXPECT_EQ(91u, consumed);

    const uint8_t wire[] = {0xc3, 0x00};
    const tlv_element_t empty{{wire, 1}, {wire + 2, 0}};
    ASSERT_EQ(TLV_OK, tlv_read_with_diagnostic(wire, sizeof wire, &one_byte_fields, &view, &consumed, nullptr));
    ExpectView(empty, view);
    EXPECT_EQ(sizeof wire, consumed);
    tlv_reader_t reader{};
    ASSERT_EQ(TLV_OK, tlv_reader_init(&reader, wire, sizeof wire, &one_byte_fields));
    view = saved_view;
    ASSERT_EQ(TLV_OK, tlv_reader_next_with_diagnostic(&reader, &view, nullptr));
    ExpectView(empty, view);
    diagnostic = stale;
    EXPECT_EQ(TLV_END, tlv_reader_next_with_diagnostic(&reader, &view, &diagnostic));
    ExpectDiagnostic(diagnostic, TLV_END);
    ExpectView(empty, view);
    EXPECT_EQ(sizeof wire, reader.pos);
    reader.pos = sizeof wire + 1;
    diagnostic = stale;
    EXPECT_EQ(TLV_ERR_INVALID_ARG, tlv_reader_next_with_diagnostic(&reader, &view, &diagnostic));
    ExpectDiagnostic(diagnostic, TLV_ERR_INVALID_ARG);
    ExpectView(empty, view);
    EXPECT_EQ(sizeof wire + 1, reader.pos);
}

TEST(WriteDiagnostic, ReportsTheWholeElementAndTheCapacityOfTheSuppliedSlice)
{
    const uint8_t tag = 0xd7;
    const uint8_t payload[] = {0x00, 0xff};
    const tlv_element_t record{{&tag, 1}, {payload, 2}};
    const std::array<uint8_t, 7> initial{0x31, 0x52, 0xea, 0xfb, 0x76, 0x55, 0x13};
    const std::array<uint8_t, 7> encoded{0x31, 0xd7, 0x02, 0x00, 0xff, 0x55, 0x13};
    auto output = initial;
    size_t written = 91;
    tlv_diagnostic_t diagnostic = stale;
    EXPECT_EQ(TLV_ERR_BUFFER_TOO_SHORT, tlv_write_with_diagnostic(
        nullptr, 0, &one_byte_fields, &record, &written, &diagnostic));
    ExpectDiagnostic(diagnostic, TLV_ERR_BUFFER_TOO_SHORT, TLV_FIELD_ELEMENT, 0, 4, 0);
    EXPECT_EQ(4u, written);
    written = 91;
    EXPECT_EQ(TLV_ERR_BUFFER_TOO_SHORT, tlv_write_with_diagnostic(
        output.data() + 1, 3, &one_byte_fields, &record, &written, &diagnostic));
    ExpectDiagnostic(diagnostic, TLV_ERR_BUFFER_TOO_SHORT, TLV_FIELD_ELEMENT, 0, 4, 3);
    EXPECT_EQ(4u, written);
    EXPECT_EQ(initial, output);
    written = 91;
    EXPECT_EQ(TLV_ERR_BUFFER_TOO_SHORT, tlv_write_with_diagnostic(
        output.data() + 1, 3, &one_byte_fields, &record, &written, nullptr));
    EXPECT_EQ(4u, written);
    EXPECT_EQ(initial, output);

    ASSERT_EQ(TLV_OK, tlv_write_with_diagnostic(output.data() + 1, 4, &one_byte_fields,
                                                &record, &written, &diagnostic));
    ExpectDiagnostic(diagnostic, TLV_OK);
    EXPECT_EQ(4u, written);
    EXPECT_EQ(encoded, output);
    written = 91;
    output = initial;
    ASSERT_EQ(TLV_OK, tlv_write_with_diagnostic(output.data() + 1, 4, &one_byte_fields,
                                                &record, &written, nullptr));
    EXPECT_EQ(4u, written);
    EXPECT_EQ(encoded, output);
}

TEST(WriteDiagnostic, ReportsCommittedOffsetAndRemainingSpaceForAShortAppend)
{
    const uint8_t tag = 0xd7;
    const uint8_t payload[] = {0x00, 0xff};
    const tlv_element_t empty{{&tag, 1}, {nullptr, 0}};
    const tlv_element_t next{{&tag, 1}, {payload, 2}};
    std::array<uint8_t, 7> bytes{0x98, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0x51};
    const std::array<uint8_t, 7> committed{0x98, 0xd7, 0x00, 0xcc, 0xcc, 0xcc, 0x51};
    tlv_writer_t writer{};
    ASSERT_EQ(TLV_OK, tlv_writer_init(&writer, bytes.data() + 1, 5, &one_byte_fields));
    ASSERT_EQ(TLV_OK, tlv_writer_write_with_diagnostic(&writer, &empty, nullptr));
    ASSERT_EQ(2u, writer.pos);
    EXPECT_EQ(committed, bytes);
    tlv_diagnostic_t diagnostic = stale;
    EXPECT_EQ(TLV_ERR_BUFFER_TOO_SHORT, tlv_writer_write_with_diagnostic(&writer, &next, &diagnostic));
    ExpectDiagnostic(diagnostic, TLV_ERR_BUFFER_TOO_SHORT, TLV_FIELD_ELEMENT, 2, 4, 3);
    EXPECT_EQ(2u, writer.pos);
    EXPECT_EQ(committed, bytes);
    EXPECT_EQ(TLV_ERR_BUFFER_TOO_SHORT, tlv_writer_write_with_diagnostic(&writer, &next, nullptr));
    EXPECT_EQ(2u, writer.pos);
    EXPECT_EQ(committed, bytes);
    EXPECT_EQ(TLV_ERR_NULL_ARG, tlv_writer_write_with_diagnostic(&writer, nullptr, &diagnostic));
    ExpectDiagnostic(diagnostic, TLV_ERR_NULL_ARG);
    EXPECT_EQ(2u, writer.pos);
    EXPECT_EQ(committed, bytes);

    const tlv_element_t fits{{&tag, 1}, {payload, 1}};
    const std::array<uint8_t, 7> full{0x98, 0xd7, 0x00, 0xd7, 0x01, 0x00, 0x51};
    diagnostic = stale;
    ASSERT_EQ(TLV_OK, tlv_writer_write_with_diagnostic(&writer, &fits, &diagnostic));
    ExpectDiagnostic(diagnostic, TLV_OK);
    EXPECT_EQ(5u, writer.pos);
    EXPECT_EQ(full, bytes);
    writer.pos = writer.capacity + 1;
    diagnostic = stale;
    EXPECT_EQ(TLV_ERR_INVALID_ARG, tlv_writer_write_with_diagnostic(&writer, &empty, &diagnostic));
    ExpectDiagnostic(diagnostic, TLV_ERR_INVALID_ARG);
    EXPECT_EQ(6u, writer.pos);
    EXPECT_EQ(full, bytes);
}

TEST(WriteDiagnostic, ClearsFieldDetailsForDescriptorAndSizeErrors)
{
    const tlv_fixed_format_t bad_format{{1}, {0, TLV_BYTE_ORDER_BIG_ENDIAN}};
    const tlv_fixed_format_t wide{{1}, {8, TLV_BYTE_ORDER_BIG_ENDIAN}};
    const uint8_t tag = 0x82;
    const std::array<uint8_t, 256> payload{};
    const struct {
        tlv_fixed_format_t format;
        tlv_element_t element;
        tlv_result_t result;
    } cases[] = {
        {one_byte_fields, {{&tag, 0}, {nullptr, 0}}, TLV_ERR_INVALID_TAG_SIZE},
        {one_byte_fields, {{&tag, 1}, {payload.data(), payload.size()}}, TLV_ERR_INVALID_LENGTH},
        {bad_format,      {{&tag, 1}, {nullptr, 0}}, TLV_ERR_INVALID_ARG},
#if SIZE_MAX <= UINT64_MAX
        {wide, {{&tag, 1}, {payload.data(), SIZE_MAX}}, TLV_ERR_OVERFLOW},
#endif
#if SIZE_MAX < UINT64_MAX
        {wide, {{&tag, 1}, {payload.data(), static_cast<tlv_size_t>(SIZE_MAX) + 1}}, TLV_ERR_NATIVE_SIZE},
#endif
    };
    for (const auto &entry : cases) {
        SCOPED_TRACE(entry.result);
        tlv_diagnostic_t diagnostic = stale;
        diagnostic.result = TLV_OK;
        size_t written = 91;
        EXPECT_EQ(entry.result, tlv_write_with_diagnostic(
            nullptr, 0, &entry.format, &entry.element, &written, &diagnostic));
        ExpectDiagnostic(diagnostic, entry.result);
        EXPECT_EQ(91u, written);
        EXPECT_EQ(entry.result, tlv_write_with_diagnostic(
            nullptr, 0, &entry.format, &entry.element, &written, nullptr));
        EXPECT_EQ(91u, written);
    }
}

}  // namespace
