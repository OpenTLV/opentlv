// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#include <opentlv/tlv.hpp>

#include <array>
#include <cstdlib>

namespace core = opentlv::core;
namespace io = opentlv::io;

// Give GDB stable entry points. GCC still optimizes each public wrapper inside
// its caller, but cannot specialize the caller for this fixture's arguments.
extern "C" {
[[gnu::noipa]] core::result probe_read(const core::fixed_format &format, core::byte_view input,
                                       core::element_view &view, std::size_t &count,
                                       core::diagnostic *d)
{
    return io::read(format, input, view, count, d);
}

[[gnu::noipa]] core::result probe_next(io::reader &reader, core::element_view &view,
                                       core::diagnostic *d)
{
    return reader.next(view, d);
}

[[gnu::noipa]] core::result probe_write(const core::fixed_format &format,
                                        core::mutable_byte_view output,
                                        const core::element_view &source, std::size_t &count,
                                        core::diagnostic *d)
{
    return io::write(format, output, source, count, d);
}

[[gnu::noipa]] core::result probe_append(io::writer &writer, const core::element_view &source,
                                         core::diagnostic *d)
{
    return writer.write(source, d);
}
}

int main()
{
    constexpr core::fixed_format format{{2}, {2, core::byte_order::big_endian}};
    constexpr std::array<std::uint8_t, 7> wire{0x12, 0x34, 0x00, 0x03, 0xaa, 0xbb, 0xcc};
    const core::element_view source{{wire.data(), 2}, {wire.data() + 4, 3}};
    for (bool enabled : {false, true}) {
        core::diagnostic details;
        auto *diagnostic = enabled ? &details : nullptr;
        for (std::size_t size : {wire.size(), std::size_t{3}}) {
            const bool complete = size == wire.size();
            const auto read_result = complete ? core::result::ok : core::result::truncated;
            const auto write_result = complete ? core::result::ok : core::result::buffer_too_short;
            std::array<std::uint8_t, wire.size()> output{};
            const auto expected_output = complete ? wire : output;
            const core::byte_view input{wire.data(), size};
            const core::mutable_byte_view destination{output.data(), size};
            io::reader reader;
            io::writer writer;
            if (reader.init(format, input) != core::result::ok ||
                writer.init(format, destination) != core::result::ok) {
                return EXIT_FAILURE;
            }
            core::element_view view{};
            std::size_t count = 0;
            if (probe_read(format, input, view, count, diagnostic) != read_result ||
                count != (complete ? wire.size() : 0) ||
                view.value.data != (complete ? source.value.data : nullptr)) {
                return EXIT_FAILURE;
            }
            view = {};
            if (probe_next(reader, view, diagnostic) != read_result || reader.position() != count ||
                view.value.data != (complete ? source.value.data : nullptr)) {
                return EXIT_FAILURE;
            }
            count = 0;
            if (probe_write(format, destination, source, count, diagnostic) != write_result ||
                count != wire.size() || output != expected_output) {
                return EXIT_FAILURE;
            }
            output.fill(0);
            if (probe_append(writer, source, diagnostic) != write_result ||
                output != expected_output || writer.position() != (complete ? wire.size() : 0)) {
                return EXIT_FAILURE;
            }
        }
    }
    return EXIT_SUCCESS;
}
