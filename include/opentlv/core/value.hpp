// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#ifndef OPENTLV_CORE_VALUE_HPP
#define OPENTLV_CORE_VALUE_HPP

#include "opentlv/core/size.hpp"
#include "opentlv/core/value.h"
#include <cstdint>

namespace opentlv::core {

/** Payload bytes and their logical count. Copies borrow the same unchanged storage. */
struct value_view {
    const std::uint8_t *data = nullptr;
    logical_size size = 0;
};

[[nodiscard]] constexpr tlv_value_t to_c(value_view value) noexcept
{
    return {value.data, value.size};
}

[[nodiscard]] constexpr value_view from_c(tlv_value_t value) noexcept
{
    return {value.data, value.size};
}

/** Check the pointer and native representability. The caller guarantees the readable extent. */
[[nodiscard]] inline result validate(const value_view &value) noexcept
{
    const auto native = to_c(value);
    return from_c(tlv_value_validate(&native));
}

/** Set out to a checked borrowed view on success. Empty views may have a null pointer. */
[[nodiscard]] inline result make_value_view(const std::uint8_t *data, logical_size size,
                                            value_view &out) noexcept
{
    tlv_value_t native{};
    const auto code = tlv_value_init(data, size, &native);
    if (code == TLV_OK) {
        out = from_c(native);
    }
    return from_c(code);
}

} // namespace opentlv::core

#endif /* OPENTLV_CORE_VALUE_HPP */
