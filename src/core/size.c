// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#include "opentlv/core/size.h"

tlv_result_t tlv_size_to_native(tlv_size_t count, size_t *out)
{
    if (out == NULL) {
        return TLV_ERR_NULL_ARG;
    }
#if SIZE_MAX < TLV_SIZE_MAX
    if (count > SIZE_MAX) {
        return TLV_ERR_NATIVE_SIZE;
    }
#endif
    *out = (size_t)count;
    return TLV_OK;
}

tlv_result_t tlv_size_add(tlv_size_t a, tlv_size_t b, tlv_size_t *out)
{
    if (out == NULL) {
        return TLV_ERR_NULL_ARG;
    }
    if (b > TLV_SIZE_MAX - a) {
        return TLV_ERR_OVERFLOW;
    }
    *out = a + b;
    return TLV_OK;
}
