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

/** Groups of results that share recovery behavior. */
enum class category {
    success = TLV_CATEGORY_SUCCESS,
    end = TLV_CATEGORY_END,
    invalid_argument = TLV_CATEGORY_INVALID_ARGUMENT,
    malformed_data = TLV_CATEGORY_MALFORMED_DATA,
    truncated = TLV_CATEGORY_TRUNCATED,
    capacity = TLV_CATEGORY_CAPACITY,
    range = TLV_CATEGORY_RANGE,
    unknown = TLV_CATEGORY_UNKNOWN
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

[[nodiscard]] constexpr tlv_result_category_t to_c(category group) noexcept
{
    return static_cast<tlv_result_category_t>(group);
}

[[nodiscard]] constexpr category from_c(tlv_result_category_t group) noexcept
{
    return static_cast<category>(group);
}

/** Classify a C-representable result code. Unassigned codes yield category::unknown. */
[[nodiscard]] inline category result_category(result code) noexcept
{
    return from_c(tlv_result_category(to_c(code)));
}

/** Describe a C-representable result code using the C library's read-only static string. */
[[nodiscard]] inline const char *result_string(result code) noexcept
{
    return tlv_result_string(to_c(code));
}

} // namespace opentlv::core

#endif /* OPENTLV_CORE_RESULT_HPP */
