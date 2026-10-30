// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel
#ifndef OPENTLV_TLVPP_QUERY_CODEC_HPP
#define OPENTLV_TLVPP_QUERY_CODEC_HPP
#include "tlv++/query/options.hpp"
#include "tlv/query/adapters.h"
/** @file
 * @brief Explicit Codec-backed Query environment composition.
 */
namespace tlv {
/** @brief Add the canonical NUM, BCD and TEXT providers before compilation.
 * Call once per environment. Provider registration may allocate; borrowed
 * programs and executions must not exist while modifying the environment.
 */
inline void configure_query_codecs(query_environment& environment) {
    size_t      count = 0;
    const auto* hooks = tlv_query_builtin_hooks(&count);
    for (size_t i = 0; i < count; ++i) detail::query_options_access::hook(environment, hooks[i]);
}
} // namespace tlv
#endif
