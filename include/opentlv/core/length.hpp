// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#ifndef OPENTLV_CORE_LENGTH_HPP
#define OPENTLV_CORE_LENGTH_HPP

#include "opentlv/core/length.h"
#include <cstddef>
#include <cstdint>

namespace opentlv::core {

/** Encoded length bytes in wire order. Copies borrow the same unchanged storage. */
struct length_view {
    const std::uint8_t *data = nullptr;
    std::size_t size = 0;
};

[[nodiscard]] constexpr tlv_length_t to_c(length_view length) noexcept
{
    return {length.data, length.size};
}

[[nodiscard]] constexpr length_view from_c(tlv_length_t length) noexcept
{
    return {length.data, length.size};
}

} // namespace opentlv::core

#endif /* OPENTLV_CORE_LENGTH_HPP */
