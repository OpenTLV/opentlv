// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

// Unmodified header-only facades call opentlv.c, compiled separately as C.
#include <iostream>

#include "opentlv/tlv.hpp"
#include "opentlv/builtin/lldp/codec.hpp"
#include "opentlv/builtin/lldp/lldp.hpp"

int main() {
    const uint8_t    ber[] = {0x04, 0x03, 'a', 'b', 'c'};
    const uint8_t    lldp[] = {2, 2, 7, 'c', 4, 2, 7, 'p', 6, 2, 0, 120, 0, 0};
    const tlv::bytes ber_input(reinterpret_cast<const tlv::byte*>(ber), sizeof ber);
    const tlv::bytes lldp_input(reinterpret_cast<const tlv::byte*>(lldp), sizeof lldp);
    if (!tlv::lldp::validate(lldp_input, 16)) return 1;
    size_t count = 0;
    bool   ttl_seen = false;
    try {
        for (auto element : tlv::ber::parse(ber_input)) {
            if (element.value().size() != 3) return 2;
            ++count;
        }
        if (count != 1) return 3;
        for (auto element : tlv::lldp::parse(lldp_input)) {
            if (element.tag() == tlv::tag_bytes<3>()) {
                auto seconds = element.decode<tlv::lldp::ttl_field>();
                if (!seconds || *seconds != 120) return 4;
                ttl_seen = true;
            }
            ++count;
        }
    } catch (const tlv::parse_error&) {
        return 5;
    }
    if (count != 5 || !ttl_seen) return 6;
    std::cout << "OpenTLV " << tlv::version() << " C++ consumer passed\n";
    return 0;
}
