// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#include "tlv/utf8.h"

void tlv_utf8_stream_init(tlv_utf8_stream_t* state) {
    state->pending_len = 0;
    state->pending_need = 0;
}

/* Validates one completed code point's leading byte range/overlong/surrogate
 * rules from its collected bytes (state->pending[0..pending_need-1]). */
static tlv_result_t utf8_stream_finish_codepoint(const tlv_utf8_stream_t* state) {
    uint8_t b0 = state->pending[0];
    uint32_t cp, min_cp;
    size_t k;
    if ((b0 & 0xE0) == 0xC0) {
        cp = (uint32_t)(b0 & 0x1F);
        min_cp = 0x80;
    } else if ((b0 & 0xF0) == 0xE0) {
        cp = (uint32_t)(b0 & 0x0F);
        min_cp = 0x800;
    } else {
        cp = (uint32_t)(b0 & 0x07);
        min_cp = 0x10000;
    }
    for (k = 1; k < state->pending_need; ++k) cp = (cp << 6) | (uint32_t)(state->pending[k] & 0x3F);
    if (cp < min_cp || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
        return TLV_ERR_INVALID_VALUE;
    return TLV_OK;
}

tlv_result_t tlv_utf8_stream_update(tlv_utf8_stream_t* state, const uint8_t* value, size_t length) {
    size_t i = 0;
    while (i < length) {
        if (state->pending_need == 0) {
            uint8_t b0 = value[i];
            size_t extra;
            if (b0 < 0x80) {
                ++i;
                continue;
            }
            if ((b0 & 0xE0) == 0xC0)
                extra = 1;
            else if ((b0 & 0xF0) == 0xE0)
                extra = 2;
            else if ((b0 & 0xF8) == 0xF0)
                extra = 3;
            else
                return TLV_ERR_INVALID_VALUE;
            state->pending[0] = b0;
            state->pending_len = 1;
            state->pending_need = 1 + extra;
            ++i;
        } else {
            while (state->pending_len < state->pending_need && i < length) {
                uint8_t b = value[i];
                if ((b & 0xC0) != 0x80) return TLV_ERR_INVALID_VALUE;
                state->pending[state->pending_len++] = b;
                ++i;
            }
            if (state->pending_len == state->pending_need) {
                tlv_result_t rc = utf8_stream_finish_codepoint(state);
                if (rc != TLV_OK) return rc;
                state->pending_len = 0;
                state->pending_need = 0;
            }
        }
    }
    return TLV_OK;
}

tlv_result_t tlv_utf8_stream_finish(const tlv_utf8_stream_t* state) {
    return state->pending_need == 0 ? TLV_OK : TLV_ERR_INVALID_VALUE;
}

/* Rejects overlong encodings, surrogate code points, out-of-range code
 * points, and truncated or malformed continuation bytes. */
tlv_result_t tlv_utf8_validate(const uint8_t* value, size_t length) {
    tlv_utf8_stream_t state;
    tlv_result_t rc;
    tlv_utf8_stream_init(&state);
    rc = tlv_utf8_stream_update(&state, value, length);
    if (rc != TLV_OK) return rc;
    return tlv_utf8_stream_finish(&state);
}

/* Well-formed sequences per Unicode Table 3-7: the lead byte selects the
 * continuation count and the narrower range of the first continuation byte,
 * which excludes overlong forms, surrogates and code points above U+10FFFF. */
int tlv_utf8_find_invalid(const uint8_t* value, size_t length, size_t* begin, size_t* end) {
    size_t i = 0;
    while (i < length) {
        uint8_t lead = value[i], low = 0x80, high = 0xBF;
        size_t need, j;
        if (lead < 0x80) {
            ++i;
            continue;
        }
        if (lead >= 0xC2 && lead <= 0xDF)
            need = 1;
        else if (lead >= 0xE0 && lead <= 0xEF)
            need = 2;
        else if (lead >= 0xF0 && lead <= 0xF4)
            need = 3;
        else {
            *begin = i;
            *end = i + 1;
            return 1;
        }
        if (lead == 0xE0) low = 0xA0;
        if (lead == 0xED) high = 0x9F;
        if (lead == 0xF0) low = 0x90;
        if (lead == 0xF4) high = 0x8F;
        for (j = 1; j <= need; ++j, low = 0x80, high = 0xBF)
            if (i + j >= length || value[i + j] < low || value[i + j] > high) {
                *begin = i;
                *end = i + j;
                return 1;
            }
        i += need + 1;
    }
    return 0;
}
