// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#ifndef OPENTLV_CORE_SIZE_HPP
#define OPENTLV_CORE_SIZE_HPP

#include "opentlv/core/result.hpp"
#include "opentlv/core/size.h"
#include <cstddef>

namespace opentlv::core {

/** Unsigned 64-bit count, independent of the width of a native buffer extent. */
using logical_size = tlv_size_t;

/** Store a representable native count. Check it against the buffer extent before use.
 * A count outside the native range leaves out unchanged.
 */
[[nodiscard]] inline result size_to_native(logical_size count, std::size_t &out) noexcept
{
    return from_c(tlv_size_to_native(count, &out));
}

/** On success, store the sum. Overflow leaves out unchanged. */
[[nodiscard]] inline result size_add(logical_size a, logical_size b, logical_size &out) noexcept
{
    return from_c(tlv_size_add(a, b, &out));
}

} // namespace opentlv::core

#endif /* OPENTLV_CORE_SIZE_HPP */
