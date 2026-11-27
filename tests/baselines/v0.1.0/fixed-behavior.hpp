#ifndef OPENTLV_TESTS_V0_1_0_FIXED_BEHAVIOR_HPP
#define OPENTLV_TESTS_V0_1_0_FIXED_BEHAVIOR_HPP

#include <opentlv/core/diagnostic.h>
#include <opentlv/core/endian.h>

#include <vector>

namespace opentlv_test::v0_1_0 {

struct WireRecord {
    tlv_byte_order_t order;
    size_t lengthWidth;
    std::vector<uint8_t> bytes;
};

// Two tag octets, the length field, then three opaque payload octets.
inline const WireRecord records[] = {
    {TLV_BYTE_ORDER_BIG_ENDIAN, 1, {0x00, 0xa7, 3, 0x00, 0xff, 0x80}},
    {TLV_BYTE_ORDER_LITTLE_ENDIAN, 1, {0x00, 0xa7, 3, 0x00, 0xff, 0x80}},
    {TLV_BYTE_ORDER_BIG_ENDIAN, 2, {0x00, 0xa7, 0, 3, 0x00, 0xff, 0x80}},
    {TLV_BYTE_ORDER_LITTLE_ENDIAN, 2, {0x00, 0xa7, 3, 0, 0x00, 0xff, 0x80}},
    {TLV_BYTE_ORDER_BIG_ENDIAN, 3, {0x00, 0xa7, 0, 0, 3, 0x00, 0xff, 0x80}},
    {TLV_BYTE_ORDER_LITTLE_ENDIAN, 3, {0x00, 0xa7, 3, 0, 0, 0x00, 0xff, 0x80}},
    {TLV_BYTE_ORDER_BIG_ENDIAN, 4, {0x00, 0xa7, 0, 0, 0, 3, 0x00, 0xff, 0x80}},
    {TLV_BYTE_ORDER_LITTLE_ENDIAN, 4, {0x00, 0xa7, 3, 0, 0, 0, 0x00, 0xff, 0x80}},
    {TLV_BYTE_ORDER_BIG_ENDIAN, 5, {0x00, 0xa7, 0, 0, 0, 0, 3, 0x00, 0xff, 0x80}},
    {TLV_BYTE_ORDER_LITTLE_ENDIAN, 5, {0x00, 0xa7, 3, 0, 0, 0, 0, 0x00, 0xff, 0x80}},
    {TLV_BYTE_ORDER_BIG_ENDIAN, 6, {0x00, 0xa7, 0, 0, 0, 0, 0, 3, 0x00, 0xff, 0x80}},
    {TLV_BYTE_ORDER_LITTLE_ENDIAN, 6, {0x00, 0xa7, 3, 0, 0, 0, 0, 0, 0x00, 0xff, 0x80}},
    {TLV_BYTE_ORDER_BIG_ENDIAN, 7, {0x00, 0xa7, 0, 0, 0, 0, 0, 0, 3, 0x00, 0xff, 0x80}},
    {TLV_BYTE_ORDER_LITTLE_ENDIAN, 7, {0x00, 0xa7, 3, 0, 0, 0, 0, 0, 0, 0x00, 0xff, 0x80}},
    {TLV_BYTE_ORDER_BIG_ENDIAN, 8, {0x00, 0xa7, 0, 0, 0, 0, 0, 0, 0, 3, 0x00, 0xff, 0x80}},
    {TLV_BYTE_ORDER_LITTLE_ENDIAN, 8, {0x00, 0xa7, 3, 0, 0, 0, 0, 0, 0, 0, 0x00, 0xff, 0x80}},
};

// A complete six-byte record followed by a three-byte tag, two-byte
// big-endian length and three-byte payload. Each cut starts at the second record.
inline constexpr size_t secondRecord = 6;
inline constexpr uint8_t stream[] = {0x00, 0x89, 0x01, 0x00, 0x01, 0xe1, 0xff,
                                     0x00, 0xa0, 0x00, 0x03, 0x7f, 0xa5, 0x00};

struct ReadCut {
    size_t prefix;
    tlv_field_t field;
    size_t offset;
    uintmax_t required;
    uintmax_t available;
};

inline constexpr ReadCut cuts[] = {
    {1, TLV_FIELD_TAG, 0, 3, 1},    {2, TLV_FIELD_TAG, 0, 3, 2},   {3, TLV_FIELD_LENGTH, 3, 2, 0},
    {4, TLV_FIELD_LENGTH, 3, 2, 1}, {5, TLV_FIELD_VALUE, 5, 3, 0}, {6, TLV_FIELD_VALUE, 5, 3, 1},
    {7, TLV_FIELD_VALUE, 5, 3, 2},
};

} // namespace opentlv_test::v0_1_0

#endif
