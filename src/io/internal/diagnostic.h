// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#ifndef OPENTLV_IO_INTERNAL_DIAGNOSTIC_H
#define OPENTLV_IO_INTERNAL_DIAGNOSTIC_H

#include "opentlv/core/diagnostic.h"

static inline void set_field(tlv_diagnostic_t *diagnostic, tlv_result_t result, tlv_field_t field,
                             size_t offset, uintmax_t required, uintmax_t available)
{
    *diagnostic = (tlv_diagnostic_t){result, field, offset, required, available};
}

static inline tlv_result_t report_result(tlv_diagnostic_t *diagnostic, tlv_result_t result)
{
    if (diagnostic) {
        set_field(diagnostic, result, TLV_FIELD_NONE, 0, 0, 0);
    }
    return result;
}

#endif /* OPENTLV_IO_INTERNAL_DIAGNOSTIC_H */
