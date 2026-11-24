// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#include <opentlv/tlv.hpp>

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace core = opentlv::core;
namespace io = opentlv::io;

static void print_bytes(const std::uint8_t *data, core::logical_size size)
{
    for (core::logical_size i = 0; i < size; ++i) {
        std::printf(" %02X", static_cast<unsigned>(data[i]));
    }
}

int main()
{
    const std::array<std::uint8_t, 7> wire{{0x12, 0x34, 0x00, 0x03, 0xAA, 0xBB, 0xCC}};
    const core::fixed_format format{{2}, {2, core::byte_order::big_endian}};
    io::reader reader;
    auto result = reader.init(format, {wire.data(), wire.size()});
    if (result != core::result::ok) {
        std::fputs("reader initialization failed\n", stderr);
        return EXIT_FAILURE;
    }

    core::element_view element;
    while ((result = reader.next(element)) == core::result::ok) {
        // Both views borrow bytes from wire.
        std::printf("tag");
        print_bytes(element.tag.data, element.tag.size);
        std::printf(", value");
        print_bytes(element.value.data, element.value.size);
        std::putchar('\n');
    }
    if (result != core::result::end) {
        std::fprintf(stderr, "could not read element at byte %zu\n", reader.position());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
