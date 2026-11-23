// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#include "opentlv/core/value.h"

tlv_result_t tlv_value_init(const uint8_t *data, tlv_size_t size, tlv_value_t *out)
{
    const tlv_value_t view = {data, size};
    tlv_result_t result;

    if (out == NULL) {
        return TLV_ERR_NULL_ARG;
    }
    result = tlv_value_validate(&view);
    if (result == TLV_OK) {
        *out = view;
    }
    return result;
}

tlv_result_t tlv_value_validate(const tlv_value_t *value)
{
    size_t native_size;

    if (value == NULL || (value->data == NULL && value->size != 0)) {
        return TLV_ERR_NULL_ARG;
    }
    return tlv_size_to_native(value->size, &native_size);
}
