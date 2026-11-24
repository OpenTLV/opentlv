// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#ifndef OPENTLV_CORE_ENDIAN_HPP
#define OPENTLV_CORE_ENDIAN_HPP

#include "opentlv/core/endian.h"

namespace opentlv::core {

enum class byte_order {
    unknown = TLV_BYTE_ORDER_UNKNOWN,
    big_endian = TLV_BYTE_ORDER_BIG_ENDIAN,
    little_endian = TLV_BYTE_ORDER_LITTLE_ENDIAN
};

/** Pass unsupported orders to the C format validator as unknown. */
[[nodiscard]] constexpr tlv_byte_order_t to_c(byte_order order) noexcept
{
    switch (order) {
    case byte_order::big_endian:
        return TLV_BYTE_ORDER_BIG_ENDIAN;
    case byte_order::little_endian:
        return TLV_BYTE_ORDER_LITTLE_ENDIAN;
    default:
        return TLV_BYTE_ORDER_UNKNOWN;
    }
}

[[nodiscard]] constexpr byte_order from_c(tlv_byte_order_t order) noexcept
{
    return static_cast<byte_order>(order);
}

} // namespace opentlv::core

#endif /* OPENTLV_CORE_ENDIAN_HPP */
