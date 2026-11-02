// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel
#include "tlv++/document/codec.hpp"
#include "tlv++/formats/fixed_format.hpp"

int main() {
    using field = tlv::field<tlv::tag_constant<1>, uint8_t, tlv::uint8_codec>;
    const tlv::byte wire[] = {static_cast<tlv::byte>(1), static_cast<tlv::byte>(1),
                              static_cast<tlv::byte>(42)};
    auto            doc = tlv::document::parse(tlv::bytes(wire, sizeof wire),
                                               tlv::document_format(tlv::fixed_format<1, 1>{}));
    if (!doc) return 1;
    auto value = doc->get<field>();
    return value && *value == 42 ? 0 : 2;
}
