// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#include "tlv/copy.h"
#include <string.h>

tlv_result_t tlv_copy_encoded(const uint8_t* encoded_data, size_t encoded_length, uint8_t* data,
                              size_t capacity, size_t* written) {
    if (!written || (!data && capacity) || (!encoded_data && encoded_length))
        return TLV_ERR_NULL_ARG;
    if (!data) {
        *written = encoded_length;
        return TLV_OK;
    }
    if (capacity < encoded_length) return TLV_ERR_BUFFER_TOO_SHORT;
    if (encoded_length) memmove(data, encoded_data, encoded_length);
    *written = encoded_length;
    return TLV_OK;
}

tlv_result_t tlv_copy_value(const tlv_element_t* element, uint8_t* data, size_t capacity,
                            size_t* written) {
    if (!element) return TLV_ERR_NULL_ARG;
    return tlv_value_copy(element->value, data, capacity, written);
}
