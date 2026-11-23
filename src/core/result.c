// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#include "opentlv/core/result.h"

struct result_info {
    tlv_result_category_t category;
    const char *message;
};

static struct result_info describe(tlv_result_t result)
{
    switch (result) {
    case TLV_OK:
        return (struct result_info){TLV_CATEGORY_SUCCESS, "success"};
    case TLV_END:
        return (struct result_info){TLV_CATEGORY_END, "end of input"};
    case TLV_ERR_TRUNCATED:
        return (struct result_info){TLV_CATEGORY_TRUNCATED, "truncated input"};
    case TLV_ERR_INVALID_TAG_SIZE:
        return (struct result_info){TLV_CATEGORY_MALFORMED_DATA, "invalid tag size"};
    case TLV_ERR_INVALID_LENGTH:
        return (struct result_info){TLV_CATEGORY_MALFORMED_DATA, "invalid length"};
    case TLV_ERR_NULL_ARG:
        return (struct result_info){TLV_CATEGORY_INVALID_ARGUMENT, "null argument"};
    case TLV_ERR_INVALID_ARG:
        return (struct result_info){TLV_CATEGORY_INVALID_ARGUMENT, "invalid argument"};
    case TLV_ERR_BUFFER_TOO_SHORT:
        return (struct result_info){TLV_CATEGORY_CAPACITY, "output buffer too short"};
    case TLV_ERR_OVERFLOW:
        return (struct result_info){TLV_CATEGORY_RANGE, "size overflow"};
    case TLV_ERR_NATIVE_SIZE:
        return (struct result_info){TLV_CATEGORY_RANGE, "size exceeds SIZE_MAX"};
    default:
        return (struct result_info){TLV_CATEGORY_UNKNOWN, "unknown result"};
    }
}

tlv_result_category_t tlv_result_category(tlv_result_t result)
{
    return describe(result).category;
}

const char *tlv_result_string(tlv_result_t result)
{
    return describe(result).message;
}
