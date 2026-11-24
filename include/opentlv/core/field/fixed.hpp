// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#ifndef OPENTLV_CORE_FIELD_FIXED_HPP
#define OPENTLV_CORE_FIELD_FIXED_HPP

#include "opentlv/core/bytes.hpp"
#include "opentlv/core/endian.hpp"
#include "opentlv/core/field/fixed.h"
#include "opentlv/core/length.hpp"
#include "opentlv/core/result.hpp"
#include "opentlv/core/size.hpp"
#include <cstddef>

namespace opentlv::core {

/** Width of an opaque tag in octets. A valid identifier has positive width. */
struct fixed_identifier {
    std::size_t size = 0;
};

/** Width and wire order of an unsigned length field. Valid widths are 1 through 8. */
struct fixed_length {
    std::size_t size = 0;
    byte_order order = byte_order::unknown;
};

[[nodiscard]] constexpr tlv_fixed_identifier_t to_c(fixed_identifier field) noexcept
{
    return {field.size};
}

[[nodiscard]] constexpr fixed_identifier from_c(tlv_fixed_identifier_t field) noexcept
{
    return {field.size};
}

[[nodiscard]] constexpr tlv_fixed_length_t to_c(fixed_length field) noexcept
{
    return {field.size, to_c(field.order)};
}

[[nodiscard]] constexpr fixed_length from_c(tlv_fixed_length_t field) noexcept
{
    return {field.size, from_c(field.byte_order)};
}

/**
 * Decode one field into length and report its width in consumed. Supply distinct objects
 * for those outputs. Truncation sets consumed to the input size and preserves length.
 * An invalid field or a null input with nonzero size preserves both outputs.
 */
[[nodiscard]] inline result read_length(const fixed_length &field, length_view input,
                                        logical_size &length, std::size_t &consumed) noexcept
{
    const auto config = to_c(field);
    return from_c(tlv_fixed_length_read(&config, input.data, input.size, &length, &consumed));
}

/**
 * Encode one field and report its width in written. The output bytes and written must
 * occupy separate storage. A null, zero-size view measures the width. A nonnull,
 * zero-size destination has insufficient capacity for any valid field. All errors
 * preserve the output bytes and written.
 */
[[nodiscard]] inline result write_length(const fixed_length &field, logical_size length,
                                         mutable_byte_view output, std::size_t &written) noexcept
{
    const auto config = to_c(field);
    return from_c(tlv_fixed_length_write(&config, length, output.data, output.size, &written));
}

} // namespace opentlv::core

#endif /* OPENTLV_CORE_FIELD_FIXED_HPP */
