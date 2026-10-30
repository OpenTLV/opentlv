// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#include "tlv++/generator.hpp"
#include "tlv++/formats/fixed_format.hpp"
#include <cstring>

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) return __LINE__;                                                         \
    } while (0)

int main() {
    const auto             candidate = tlv::make_generator_candidate(tlv::tag_bytes<1>(), 0, 16);
    const auto             format = tlv::fixed_format<1, 1, tlv::byte_order::big_endian>::view();
    tlv::generator_options options{42, 7, 8, 0, 16, 128, &candidate, 1};
    tlv::byte              output[128], workspace[128];
    auto                   required = tlv::generator_workspace_size(options);
    CHECK(required && *required == sizeof(workspace));
    auto written =
        tlv::generate(format, options, {output, sizeof(output)}, {workspace, sizeof(workspace)});
    CHECK(written && *written > 0);
    std::vector<tlv::byte> retained;
    {
        tlv::generator generator(format, options);
        auto           owned = generator.generate(7);
        CHECK(owned && owned->size() == *written);
        CHECK(std::memcmp(owned->data(), output, *written) == 0);
        retained = std::move(*owned);
        CHECK(generator.generate(99));
        auto repeated = generator.generate(7);
        CHECK(repeated && *repeated == retained);
    }
    CHECK(retained.size() == *written);
    CHECK(std::memcmp(retained.data(), output, *written) == 0);
    options.max_elements = 0;
    auto invalid =
        tlv::generate(format, options, {output, sizeof(output)}, {workspace, sizeof(workspace)});
    CHECK(!invalid && invalid.error().status() == tlv::errc::invalid_argument);
    return 0;
}
