// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#include <opentlv/tlv.h>

#include <algorithm>
#include <array>
#include <cstdlib>

namespace {

bool check_field(tlv_fixed_length_t field, size_t offset)
{
    constexpr uint8_t digits[] = {0xef, 0xcd, 0xab, 0x89, 0x67, 0x45, 0x23, 0x81};
    const tlv_size_t value = UINT64_C(0x8123456789abcdef) & (UINT64_MAX >> (8 * (8 - field.size)));
    alignas(64) std::array<uint8_t, 10> golden, output;
    golden.fill(0xcc);
    output.fill(0xcc);
    if (field.byte_order == TLV_BYTE_ORDER_BIG_ENDIAN) {
        std::reverse_copy(digits, digits + field.size, golden.data() + offset);
    } else {
        std::copy_n(digits, field.size, golden.data() + offset);
    }

    tlv_size_t decoded = 0;
    size_t consumed = 0, written = 0;
    return tlv_fixed_length_read(&field, golden.data() + offset, field.size, &decoded, &consumed) == TLV_OK &&
           decoded == value && consumed == field.size &&
           tlv_fixed_length_write(&field, value, output.data() + offset, field.size, &written) == TLV_OK &&
           written == field.size && output == golden;
}

bool check_record(tlv_fixed_length_t field, size_t payload_size)
{
    constexpr uint8_t tag[] = {0x00, 0xa7};
    constexpr uint8_t pattern[] = {0x00, 0xff, 0x80};
    alignas(64) std::array<uint8_t, 513> payload;
    for (size_t i = 0; i < payload_size; ++i) {
        payload[i] = pattern[i % sizeof pattern];
    }

    // Both record buffers start at offset one. Their two-byte tags put the
    // length fields at offset three, including for the eight-byte layout.
    alignas(64) std::array<uint8_t, 1 + sizeof tag + 8 + 513> golden, output;
    golden.fill(0xcc);
    output.fill(0xcc);
    uint8_t *wire = golden.data() + 1;
    std::copy_n(tag, sizeof tag, wire);
    const size_t header = sizeof tag + field.size;
    std::fill(wire + sizeof tag, wire + header, 0);
    // The payload sizes use at most two significant octets.
    uint8_t *length = wire + sizeof tag;
    const bool little = field.byte_order == TLV_BYTE_ORDER_LITTLE_ENDIAN;
    length[little ? 0 : field.size - 1] = static_cast<uint8_t>(payload_size);
    if (field.size > 1) {
        length[little ? 1 : field.size - 2] = static_cast<uint8_t>(payload_size / 256);
    }
    std::copy_n(payload.data(), payload_size, wire + header);

    const tlv_fixed_format_t format{{sizeof tag}, field};
    const tlv_element_t source{{tag, sizeof tag}, {payload.data(), payload_size}};
    const size_t size = header + payload_size;
    tlv_element_t decoded{};
    size_t consumed = 0, written = 0;
    return tlv_read(wire, size, &format, &decoded, &consumed) == TLV_OK && consumed == size &&
           decoded.tag.data == wire && decoded.tag.size == sizeof tag &&
           decoded.value.data == wire + header && decoded.value.size == payload_size &&
           tlv_write(output.data() + 1, size, &format, &source, &written) == TLV_OK &&
           written == size && output == golden;
}

}  // namespace

int main()
{
    for (size_t width = 1; width <= 8; ++width) {
        for (auto order : {TLV_BYTE_ORDER_BIG_ENDIAN, TLV_BYTE_ORDER_LITTLE_ENDIAN}) {
            const tlv_fixed_length_t field{width, order};
            if (!check_field(field, 0) || !check_field(field, 1)) {
                return EXIT_FAILURE;
            }
            for (size_t size : {0u, 3u, width == 1 ? 255u : 513u}) {
                if (!check_record(field, size)) {
                    return EXIT_FAILURE;
                }
            }
        }
    }
    return EXIT_SUCCESS;
}
