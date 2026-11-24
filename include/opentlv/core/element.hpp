// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#ifndef OPENTLV_CORE_ELEMENT_HPP
#define OPENTLV_CORE_ELEMENT_HPP

#include "opentlv/core/element.h"
#include "opentlv/core/tag.hpp"
#include "opentlv/core/value.hpp"

namespace opentlv::core {

/** A tag and payload borrowing their backing bytes through the last use of any copy. */
struct element_view {
    tag_view tag{};
    value_view value{};
};

[[nodiscard]] constexpr tlv_element_t to_c(const element_view &element) noexcept
{
    return {to_c(element.tag), to_c(element.value)};
}

[[nodiscard]] constexpr element_view from_c(const tlv_element_t &element) noexcept
{
    return {from_c(element.tag), from_c(element.value)};
}

} // namespace opentlv::core

#endif /* OPENTLV_CORE_ELEMENT_HPP */
