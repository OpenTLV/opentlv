// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#ifndef OPENTLV_CORE_INTERNAL_VALIDATION_H
#define OPENTLV_CORE_INTERNAL_VALIDATION_H

#include "opentlv/core/size.h"

static inline tlv_result_t validate_value_view(const uint8_t *data, tlv_size_t size)
{
    if (!data && size != 0) {
        return TLV_ERR_NULL_ARG;
    }
#if SIZE_MAX < TLV_SIZE_MAX
    if (size > SIZE_MAX) {
        return TLV_ERR_NATIVE_SIZE;
    }
#endif
    return TLV_OK;
}

#endif /* OPENTLV_CORE_INTERNAL_VALIDATION_H */
