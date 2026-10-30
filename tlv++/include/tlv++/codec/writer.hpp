// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel
#ifndef OPENTLV_TLVPP_CODEC_WRITER_HPP
#define OPENTLV_TLVPP_CODEC_WRITER_HPP
#include "tlv++/codec/typed.hpp"
#include "tlv++/writer/writer.hpp"
/** @file
 * @brief IO-Codec integration implementing typed writer overloads.
 * Include this header when writing typed fields; generic Value codecs need no IO.
 */
namespace tlv {
/// @cond INTERNAL
namespace detail {
template <typename Field>
expected<void, typed_error> writer_base::write(const typename Field::value_type& value) {
    auto required = Field::codec_type::encode(value, nullptr, 0);
    if (!required) return unexpected<typed_error>(typed_error(required.error()));
    // A bounded stack buffer handles common scalar and short text encodings.
    // Larger owning convenience operations retain their explicit heap behavior.
    byte local[64];
    if (*required <= sizeof local) return write<Field>(value, span<byte>(local, sizeof local));
    std::vector<byte> scratch(*required);
    return write<Field>(value, span<byte>(scratch.data(), scratch.size()));
}
template <typename Field>
expected<void, typed_error> writer_base::write(const typename Field::value_type& value,
                                               span<byte>                        scratch) {
    auto required = Field::codec_type::encode(value, nullptr, 0);
    if (!required) return unexpected<typed_error>(typed_error(required.error()));
    if (*required > scratch.size())
        return unexpected<typed_error>(typed_error(codec_failure(errc::buffer_too_short)));
    auto written = Field::codec_type::encode(value, scratch.data(), scratch.size());
    if (!written) return unexpected<typed_error>(typed_error(written.error()));
    if (*written != *required || *written > scratch.size())
        return unexpected<typed_error>(typed_error(codec_failure(errc::callback)));
    auto result = write(Field::tag(), bytes(scratch.data(), *written));
    if (!result)
        return unexpected<typed_error>(
            typed_error(static_cast<tlv_result_t>(result.error().status())));
    return {};
}
} // namespace detail
/// @endcond
} // namespace tlv
#endif
