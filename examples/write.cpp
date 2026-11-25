// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#include <opentlv/tlv.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace core = opentlv::core;
namespace io = opentlv::io;

int main()
{
    const std::array<std::uint8_t, 2> tag{{0x12, 0x34}};
    const std::array<std::uint8_t, 3> value{{0xAA, 0xBB, 0xCC}};
    const core::fixed_format format{{tag.size()}, {2, core::byte_order::big_endian}};
    const core::element_view element{{tag.data(), tag.size()}, {value.data(), value.size()}};

    std::array<std::uint8_t, 16> wire{};
    io::writer writer;
    auto result = writer.init(format, {wire.data(), wire.size()});
    if (result != core::result::ok) {
        std::fprintf(stderr, "writer initialization failed (%s)\n", core::result_string(result));
        return EXIT_FAILURE;
    }
    result = writer.write(element);
    if (result != core::result::ok) {
        std::fprintf(stderr, "could not write element (%s)\n", core::result_string(result));
        return EXIT_FAILURE;
    }

    const std::size_t written = writer.position();
    for (std::size_t i = 0; i < written; ++i) {
        std::printf("%02X%c", static_cast<unsigned>(wire[i]), i + 1 == written ? '\n' : ' ');
    }
    return EXIT_SUCCESS;
}
