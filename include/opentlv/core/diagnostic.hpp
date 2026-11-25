// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#ifndef OPENTLV_CORE_DIAGNOSTIC_HPP
#define OPENTLV_CORE_DIAGNOSTIC_HPP

#include "opentlv/core/diagnostic.h"
#include "opentlv/core/result.hpp"
#include <cstddef>
#include <cstdint>

namespace opentlv::core {

enum class field {
    none = TLV_FIELD_NONE,
    tag = TLV_FIELD_TAG,
    length = TLV_FIELD_LENGTH,
    value = TLV_FIELD_VALUE,
    element = TLV_FIELD_ELEMENT
};

[[nodiscard]] constexpr tlv_field_t to_c(field part) noexcept
{
    return static_cast<tlv_field_t>(part);
}

[[nodiscard]] constexpr field from_c(tlv_field_t part) noexcept
{
    return static_cast<field>(part);
}

/** Owned details of a bounded IO operation. Counts are in bytes. */
struct diagnostic {
    core::result result = core::result::ok;
    core::field field = core::field::none;
    std::size_t offset = 0;
    std::uintmax_t required = 0;
    std::uintmax_t available = 0;
};

[[nodiscard]] constexpr tlv_diagnostic_t to_c(const diagnostic &details) noexcept
{
    return {to_c(details.result), to_c(details.field),
            details.offset, details.required, details.available};
}

[[nodiscard]] constexpr diagnostic from_c(const tlv_diagnostic_t &details) noexcept
{
    return {from_c(details.result), from_c(details.field),
            details.offset, details.required, details.available};
}

} // namespace opentlv::core

#endif /* OPENTLV_CORE_DIAGNOSTIC_HPP */
