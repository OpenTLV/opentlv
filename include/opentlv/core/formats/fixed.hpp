// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#ifndef OPENTLV_CORE_FORMATS_FIXED_HPP
#define OPENTLV_CORE_FORMATS_FIXED_HPP

#include "opentlv/core/element.hpp"
#include "opentlv/core/field/fixed.hpp"
#include "opentlv/core/formats/fixed.h"
#include "opentlv/core/result.hpp"
#include <cstddef>

namespace opentlv::core {

/** Fixed TLV wire layout. The format owns both field configurations. */
struct fixed_format {
    fixed_identifier identifier{};
    fixed_length length{};
};

[[nodiscard]] constexpr tlv_fixed_format_t to_c(const fixed_format &format) noexcept
{
    return {to_c(format.identifier), to_c(format.length)};
}

[[nodiscard]] constexpr fixed_format from_c(const tlv_fixed_format_t &format) noexcept
{
    return {from_c(format.identifier), from_c(format.length)};
}

/** Validate the format and measure the header. Failure preserves size. */
[[nodiscard]] inline result header_size(const fixed_format &format, std::size_t &size) noexcept
{
    const auto config = to_c(format);
    return from_c(tlv_fixed_header_size(&config, &size));
}

/** Measure the complete element without reading its bytes. Failure preserves size. */
[[nodiscard]] inline result element_size(const fixed_format &format, const element_view &element,
                                         std::size_t &size) noexcept
{
    const auto config = to_c(format);
    const auto source = to_c(element);
    return from_c(tlv_fixed_element_size(&config, &source, &size));
}

} // namespace opentlv::core

#endif /* OPENTLV_CORE_FORMATS_FIXED_HPP */
