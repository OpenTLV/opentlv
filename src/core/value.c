// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#include "opentlv/core/value.h"
#include "internal/validation.h"

tlv_result_t tlv_value_init(const uint8_t *data, tlv_size_t size, tlv_value_t *out)
{
    if (out == NULL) {
        return TLV_ERR_NULL_ARG;
    }
    tlv_result_t result = validate_value_view(data, size);
    if (result == TLV_OK) {
        *out = (tlv_value_t){data, size};
    }
    return result;
}

tlv_result_t tlv_value_validate(const tlv_value_t *value)
{
    if (value == NULL) {
        return TLV_ERR_NULL_ARG;
    }
    return validate_value_view(value->data, value->size);
}
