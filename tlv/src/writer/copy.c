// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#include "tlv/writer/copy.h"
#include "tlv/writer/writer.h"
#include "tlv/size.h"

tlv_result_t tlv_copy_element(const tlv_element_t* element, const tlv_format_t* format,
                              uint8_t* data, size_t capacity, size_t* written) {
    size_t length, local_written;
    tlv_result_t rc;
    if (!element || !written || (!data && capacity) ||
        (!element->value.data && element->value.size))
        return TLV_ERR_NULL_ARG;
    rc = tlv_size_to_native(element->value.size, &length);
    if (rc != TLV_OK) return rc;
    if (!data) return tlv_element_encoded_size(element, format, written);
    rc = tlv_write_element(data, capacity, format, element, &local_written);
    if (rc == TLV_OK) *written = local_written;
    return rc;
}
