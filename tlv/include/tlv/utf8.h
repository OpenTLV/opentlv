// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#ifndef OPENTLV_UTF8_H
#define OPENTLV_UTF8_H

#include "tlv/result.h"
#include "tlv/core/export.h"

#ifdef __cplusplus
extern "C" {
#endif
#include <stddef.h>
#include <stdint.h>

/** @brief Shared, protocol-independent UTF-8 validation. No allocation or recursion.
 * Input may be NULL only with zero length. Rejects malformed/overlong UTF-8,
 * surrogates and code points above U+10FFFF, accepting embedded U+0000.
 * Returns TLV_OK or TLV_ERR_INVALID_VALUE. */
OPENTLV_CORE_API tlv_result_t tlv_utf8_validate(const uint8_t* value, size_t length);

/** @brief Finds the first ill-formed subsequence of input that tlv_utf8_validate()
 * rejects, for diagnostics only: [*begin, *end) is its maximal subpart (Unicode
 * Table 3-7), the longest prefix of a well-formed sequence, or the single
 * offending byte when no sequence can start there. The byte that ends a
 * truncated sequence is not included. Returns nonzero when one exists. */
OPENTLV_CORE_API int tlv_utf8_find_invalid(const uint8_t* value, size_t length, size_t* begin,
                                           size_t* end);

/** @brief Streaming validation for byte spans whose boundaries may split a code
 * point (for example CER segments). Initialize before the first update and
 * finish after the final update. After any error, discard the state.
 * finish() rejects incomplete final code points. Fixed O(1) storage. */
typedef struct tlv_utf8_stream {
    uint8_t pending[4];  /**< Incomplete code point bytes. */
    size_t pending_len;  /**< Bytes collected for the in-flight code point. */
    size_t pending_need; /**< Total expected bytes, zero between code points. */
} tlv_utf8_stream_t;

/** @brief Initialize required streaming state before updates. */
OPENTLV_CORE_API void tlv_utf8_stream_init(tlv_utf8_stream_t* state);
/** @brief Validate the next span; state is required and NULL input requires zero length. Discard
 * state after failure. */
OPENTLV_CORE_API tlv_result_t tlv_utf8_stream_update(tlv_utf8_stream_t* state, const uint8_t* value,
                                                     size_t length);
/** @brief Finish required streaming state; reject an incomplete final code point. */
OPENTLV_CORE_API tlv_result_t tlv_utf8_stream_finish(const tlv_utf8_stream_t* state);

#ifdef __cplusplus
}
#endif

#endif /* OPENTLV_UTF8_H */
