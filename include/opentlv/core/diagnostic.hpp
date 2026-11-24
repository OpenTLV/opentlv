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

/** Owned details of a bounded IO operation. Counts are in bytes. */
struct diagnostic {
    core::result result = core::result::ok;
    core::field field = core::field::none;
    std::size_t offset = 0;
    std::uintmax_t required = 0;
    std::uintmax_t available = 0;
};

[[nodiscard]] constexpr diagnostic from_c(const tlv_diagnostic_t &details) noexcept
{
    return {from_c(details.result), static_cast<field>(details.field),
            details.offset, details.required, details.available};
}

} // namespace opentlv::core

#endif /* OPENTLV_CORE_DIAGNOSTIC_HPP */
