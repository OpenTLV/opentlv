// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel
#include "tlv++/document/document.hpp"
#include "tlv++/formats/fixed_format.hpp"

int main() {
    const tlv::byte wire[] = {static_cast<tlv::byte>(1), static_cast<tlv::byte>(1),
                              static_cast<tlv::byte>(42)};
    auto            doc = tlv::document::parse(tlv::bytes(wire, sizeof wire),
                                               tlv::document_format(tlv::fixed_format<1, 1>{}));
    return doc && doc->size() == 1 && doc->first().value().size() == 1 ? 0 : 1;
}
