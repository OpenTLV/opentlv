// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#ifndef OPENTLV_TLVPP_DOCUMENT_CODEC_HPP
#define OPENTLV_TLVPP_DOCUMENT_CODEC_HPP

/** @file
 * @brief Typed decoding of Document nodes; link OpenTLV::DocumentCodec.
 */
#include "tlv++/document/document.hpp"
#include "tlv++/codec/typed.hpp"

namespace tlv {
template <typename Field>
inline expected<typename Field::value_type, typed_error> node::decode() const {
    if (!*this) return unexpected<typed_error>(typed_error(typed_errc::invalid_node));
    if (tag() != Field::tag())
        return unexpected<typed_error>(typed_error(typed_errc::tag_mismatch));
    if (is_constructed())
        return unexpected<typed_error>(typed_error(typed_errc::constructed_value));
    return element_view(tag(), value()).template decode<Field>();
}

template <typename Field>
inline expected<typename Field::value_type, typed_error> node::get() const {
    if (!*this) return unexpected<typed_error>(typed_error(typed_errc::invalid_node));
    auto child = find(Field::tag());
    if (!child) return unexpected<typed_error>(typed_error(typed_errc::missing_field));
    return child.template decode<Field>();
}

template <typename Field>
inline expected<typename Field::value_type, typed_error> const_node::decode() const {
    return value_.template decode<Field>();
}

template <typename Field>
inline expected<typename Field::value_type, typed_error> const_node::get() const {
    return value_.template get<Field>();
}

template <typename Field>
inline expected<typename Field::value_type, typed_error> document::get() const {
    auto found = find(Field::tag());
    if (!found) return unexpected<typed_error>(typed_error(typed_errc::missing_field));
    return found.template decode<Field>();
}
} // namespace tlv
#endif
