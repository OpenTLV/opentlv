// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#ifndef OPENTLV_CORE_RESULT_HPP
#define OPENTLV_CORE_RESULT_HPP

#include "opentlv/core/result.h"

namespace opentlv::core {

enum class result {
    ok = TLV_OK,
    end = TLV_END,
    truncated = TLV_ERR_TRUNCATED,
    invalid_tag_size = TLV_ERR_INVALID_TAG_SIZE,
    invalid_length = TLV_ERR_INVALID_LENGTH,
    null_arg = TLV_ERR_NULL_ARG,
    invalid_arg = TLV_ERR_INVALID_ARG,
    buffer_too_short = TLV_ERR_BUFFER_TOO_SHORT,
    overflow = TLV_ERR_OVERFLOW,
    native_size = TLV_ERR_NATIVE_SIZE
};

/** Convert a result code representable by tlv_result_t. */
[[nodiscard]] constexpr tlv_result_t to_c(result code) noexcept
{
    return static_cast<tlv_result_t>(code);
}

[[nodiscard]] constexpr result from_c(tlv_result_t code) noexcept
{
    return static_cast<result>(code);
}

} // namespace opentlv::core

#endif /* OPENTLV_CORE_RESULT_HPP */
