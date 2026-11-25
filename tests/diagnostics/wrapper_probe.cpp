// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#include <opentlv/tlv.hpp>

#include <array>
#include <cstdlib>

namespace core = opentlv::core;
namespace io = opentlv::io;

extern "C" {
decltype(tlv_result_category) c_category;
decltype(tlv_value_init) c_value;
decltype(tlv_fixed_element_size) c_measure;
decltype(tlv_read) c_read;
decltype(tlv_reader_next) c_next;
decltype(tlv_write) c_write;
decltype(tlv_writer_write) c_append;

// Retain the caller in the binary, with runtime arguments and inlined wrappers.
[[gnu::noipa]] core::category cpp_category(core::result code)
{
    return core::result_category(code);
}

[[gnu::noipa]] core::result cpp_value(const uint8_t *data, tlv_size_t size, core::value_view &out)
{
    return core::make_value_view(data, size, out);
}

[[gnu::noipa]] core::result cpp_measure(const core::fixed_format &format,
                                        const core::element_view &source, size_t &size)
{
    return core::element_size(format, source, size);
}

[[gnu::noipa]] core::result cpp_read(const uint8_t *data, size_t size,
                                     const core::fixed_format &format, core::element_view &out,
                                     size_t &count)
{
    return io::read(format, {data, size}, out, count);
}

[[gnu::noipa]] core::result cpp_next(io::reader &cursor, core::element_view &out)
{
    return cursor.next(out);
}

[[gnu::noipa]] core::result cpp_write(uint8_t *data, size_t capacity,
                                      const core::fixed_format &format,
                                      const core::element_view &source, size_t &count)
{
    return io::write(format, {data, capacity}, source, count);
}

[[gnu::noipa]] core::result cpp_append(io::writer &cursor, const core::element_view &source)
{
    return cursor.write(source);
}
}

namespace {

constexpr std::array<uint8_t, 7> wire{0x12, 0x34, 0x00, 0x03, 0xaa, 0xbb, 0xcc};
constexpr core::fixed_format format{{2}, {2, core::byte_order::big_endian}};
constexpr core::element_view source{{wire.data(), 2}, {wire.data() + 4, 3}};
constexpr auto native_format = core::to_c(format);
constexpr auto native_source = core::to_c(source);

template<typename Element> bool matches(const Element &element, const core::element_view &expected)
{
    return element.tag.data == expected.tag.data && element.tag.size == expected.tag.size &&
           element.value.data == expected.value.data && element.value.size == expected.value.size;
}

bool check_values(bool valid)
{
    const uint8_t *data = valid ? wire.data() : nullptr;
    const auto code = valid ? TLV_OK : TLV_ERR_NULL_ARG;
    const auto category = valid ? TLV_CATEGORY_SUCCESS : TLV_CATEGORY_INVALID_ARGUMENT;
    tlv_value_t native = native_source.value;
    core::value_view typed = source.value;
    const auto expected = valid ? core::value_view{data, wire.size()} : source.value;
    return c_category(code) == category &&
           core::to_c(cpp_category(core::from_c(code))) == category &&
           c_value(data, wire.size(), &native) == code &&
           core::to_c(cpp_value(data, wire.size(), typed)) == code &&
           native.data == expected.data && native.size == expected.size &&
           typed.data == expected.data && typed.size == expected.size;
}

bool check_record(size_t size)
{
    const bool complete = size == wire.size();
    const auto read_code = complete ? TLV_OK : TLV_ERR_TRUNCATED;
    const auto write_code = complete ? TLV_OK : TLV_ERR_BUFFER_TOO_SHORT;
    // Distinct nonempty sentinels expose partial updates on a short read.
    constexpr core::element_view sentinel{{wire.data() + 1, 1}, {wire.data() + 2, 2}};
    const auto &expected = complete ? source : sentinel;
    auto native = core::to_c(sentinel);
    auto typed = sentinel;
    size_t c_count = 42, cpp_count = 42;
    if (c_read(wire.data(), size, &native_format, &native, &c_count) != read_code ||
        core::to_c(cpp_read(wire.data(), size, format, typed, cpp_count)) != read_code ||
        c_count != (complete ? wire.size() : 42) || cpp_count != c_count ||
        !matches(native, expected) || !matches(typed, expected)) {
        return false;
    }

    tlv_reader_t c_reader;
    io::reader cpp_reader;
    native = core::to_c(sentinel);
    typed = sentinel;
    if (tlv_reader_init(&c_reader, wire.data(), size, &native_format) != TLV_OK ||
        cpp_reader.init(format, {wire.data(), size}) != core::result::ok ||
        c_next(&c_reader, &native) != read_code ||
        core::to_c(cpp_next(cpp_reader, typed)) != read_code ||
        c_reader.pos != (complete ? size : 0) || cpp_reader.position() != c_reader.pos ||
        !matches(native, expected) || !matches(typed, expected)) {
        return false;
    }

    std::array<uint8_t, wire.size()> c_output{}, cpp_output{};
    const auto expected_output = complete ? wire : c_output;
    c_count = cpp_count = 42;
    if (c_write(c_output.data(), size, &native_format, &native_source, &c_count) != write_code ||
        core::to_c(cpp_write(cpp_output.data(), size, format, source, cpp_count)) != write_code ||
        c_count != wire.size() || cpp_count != c_count || c_output != expected_output ||
        cpp_output != c_output) {
        return false;
    }

    tlv_writer_t c_writer;
    io::writer cpp_writer;
    c_output.fill(0);
    cpp_output.fill(0);
    return tlv_writer_init(&c_writer, c_output.data(), size, &native_format) == TLV_OK &&
           cpp_writer.init(format, {cpp_output.data(), size}) == core::result::ok &&
           c_append(&c_writer, &native_source) == write_code &&
           core::to_c(cpp_append(cpp_writer, source)) == write_code &&
           c_writer.pos == (complete ? size : 0) && cpp_writer.position() == c_writer.pos &&
           c_output == expected_output && cpp_output == c_output;
}

} // namespace

int main()
{
    size_t c_size = 0, cpp_size = 0;
    return check_values(true) && check_values(false) &&
                   c_measure(&native_format, &native_source, &c_size) == TLV_OK &&
                   cpp_measure(format, source, cpp_size) == core::result::ok &&
                   c_size == wire.size() && cpp_size == c_size && check_record(wire.size()) &&
                   check_record(3)
               ? EXIT_SUCCESS
               : EXIT_FAILURE;
}
