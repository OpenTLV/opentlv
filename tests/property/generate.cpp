// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel
#include "tlv/formats/fixed.h"
#include "tlv/generator.h"
#include "tlv/builtins/asn1/ber.h"
#include "tlv/builtins/asn1/der.h"
#include "tlv/builtins/emv/format.h"
#include "tlv/builtins/bluetooth/bluetooth_ltv.h"
#include "tlv/builtins/nfc/type2.h"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
uint64_t number(const char* text) {
    if (!*text || std::strspn(text, "0123456789") != std::strlen(text))
        throw std::invalid_argument("expected an unsigned decimal integer");
    return std::stoull(text);
}
size_t size_argument(const char* text) {
    const auto value = number(text);
    if (value > SIZE_MAX) throw std::out_of_range("size does not fit this platform");
    return static_cast<size_t>(value);
}
} // namespace

int main(int argc, char** argv) try {
    // Test-only adapter, not a CLI command:
    // format directory seed count depth elements value-size case-size tag-width length-width
    // byte-order
    if (argc != 12) return 2;
    const uint64_t seed = number(argv[3]), count = number(argv[4]);
    const size_t   depth = size_argument(argv[5]), elements = size_argument(argv[6]);
    size_t         value_limit = size_argument(argv[7]);
    const size_t   case_size = size_argument(argv[8]);
    if (!count || depth > 64 || !elements || !case_size ||
        case_size > static_cast<uint64_t>(std::numeric_limits<std::streamsize>::max()))
        return 2;
    if (std::strcmp(argv[11], "big") && std::strcmp(argv[11], "little")) return 2;
    const tlv_fixed_format_t fixed = {{size_argument(argv[9])},
                                      {size_argument(argv[10]), !std::strcmp(argv[11], "little")
                                                                    ? TLV_BYTE_ORDER_LITTLE_ENDIAN
                                                                    : TLV_BYTE_ORDER_BIG_ENDIAN},
                                      TLV_ELEMENT_ORDER_TLV,
                                      TLV_LENGTH_SCOPE_VALUE};
    tlv_format_t             descriptor{};
    const tlv_format_t*      format = nullptr;
    std::vector<std::vector<uint8_t>> tags;
    bool                              nested = false;
    if (!std::strcmp(argv[1], "fixed")) {
        if (tlv_fixed_format_init(&descriptor, &fixed) != TLV_OK) return 2;
        format = &descriptor;
        tags.emplace_back(fixed.identifier.size, uint8_t(1));
        if (fixed.length.size < sizeof(size_t))
            value_limit = std::min(value_limit, (size_t(1) << (fixed.length.size * 8)) - 1);
    }
    if (!std::strcmp(argv[1], "ber")) format = &tlv_format_ber;
    if (!std::strcmp(argv[1], "der")) format = &tlv_format_der;
    if (!std::strcmp(argv[1], "emv")) format = &tlv_format_emv;
    if (!std::strcmp(argv[1], "ber") || !std::strcmp(argv[1], "der") ||
        !std::strcmp(argv[1], "emv")) {
        tags = {{0x04}, {0x80}, {0x30}, {0xA0}};
        nested = true;
    }
    if (!std::strcmp(argv[1], "bluetooth-ltv")) {
        format = &tlv_format_bluetooth_ltv;
        tags = {{0x09}, {0xFF}};
        value_limit = std::min(value_limit, size_t(254));
    }
    if (!std::strcmp(argv[1], "nfc-type2")) {
        format = &tlv_format_nfc_type2;
        tags = {{0x03}, {0xFD}};
        value_limit = std::min(value_limit, size_t(65534));
    }
    if (!format) return 2;
    // Stable test-owned domains preserve the former CLI-generated corpus.
    // Generation and PRNG behavior are exclusively implemented by Core.
    std::vector<tlv_generator_candidate_t> candidates;
    for (const auto& tag : tags) candidates.push_back({{tag.data(), tag.size()}, 0, value_limit});
    tlv_generator_options_t options{seed,        0,         elements,          nested ? depth : 0,
                                    value_limit, case_size, candidates.data(), candidates.size()};
    size_t                  workspace_size = 0;
    const auto              checked = tlv_generator_workspace_size(&options, &workspace_size);
    if (checked != TLV_OK) {
        std::cerr << "Generator configuration error " << static_cast<int>(checked) << '\n';
        return 1;
    }
    std::vector<uint8_t> workspace(workspace_size), output(case_size);
    for (uint64_t index = 0; index < count; ++index) {
        options.case_index = index;
        size_t     written = 0;
        const auto result = tlv_generate(format, &options, output.data(), output.size(),
                                         workspace.data(), workspace.size(), &written);
        if (result != TLV_OK) {
            std::cerr << "Generator error " << static_cast<int>(result) << " at case " << index
                      << '\n';
            return 1;
        }
        std::ostringstream path;
        path << argv[2] << '/' << std::setw(6) << std::setfill('0') << index << ".bin";
        std::ofstream destination(path.str(), std::ios::binary);
        destination.write(reinterpret_cast<const char*>(output.data()),
                          static_cast<std::streamsize>(written));
        destination.close();
        if (!destination) {
            std::cerr << "Cannot write case " << index << '\n';
            return 2;
        }
    }
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 2;
}
