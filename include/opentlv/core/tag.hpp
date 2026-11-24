// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#ifndef OPENTLV_CORE_TAG_HPP
#define OPENTLV_CORE_TAG_HPP

#include "opentlv/core/tag.h"
#include <cstddef>
#include <cstdint>

namespace opentlv::core {

/** Tag bytes in wire order. Copies borrow the same unchanged storage. */
struct tag_view {
    const std::uint8_t *data = nullptr;
    std::size_t size = 0;
};

[[nodiscard]] constexpr tlv_tag_t to_c(tag_view tag) noexcept
{
    return {tag.data, tag.size};
}

[[nodiscard]] constexpr tag_view from_c(tlv_tag_t tag) noexcept
{
    return {tag.data, tag.size};
}

} // namespace opentlv::core

#endif /* OPENTLV_CORE_TAG_HPP */
