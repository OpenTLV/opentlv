// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel
#include "tlv++/codec/writer.hpp"
#include "tlv++/formats/fixed_format.hpp"

int main() {
    using field = tlv::field<tlv::tag_constant<1>, uint8_t, tlv::uint8_codec>;
    tlv::byte                            wire[3] = {};
    tlv::writer<tlv::fixed_format<1, 1>> output(wire, sizeof wire);
    auto                                 result = output.write<field>(42);
    return result && wire[0] == static_cast<tlv::byte>(1) && wire[1] == static_cast<tlv::byte>(1) &&
                   wire[2] == static_cast<tlv::byte>(42)
               ? 0
               : 1;
}
