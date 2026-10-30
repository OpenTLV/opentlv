// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel
#ifndef OPENTLV_TEST_SCHEMA_DIAGNOSTIC_HELPERS_H
#define OPENTLV_TEST_SCHEMA_DIAGNOSTIC_HELPERS_H
#include "tlv/schema/schema.h"
#include <string.h>
static inline tlv_reader_detail_t schema_test_reader(const tlv_schema_diagnostic_t* diagnostic) {
    tlv_reader_detail_t result;
    memset(&result, 0, sizeof result);
    if (diagnostic->cause == TLV_SCHEMA_CAUSE_READER && diagnostic->detail.causes.count)
        (void)tlv_reader_detail_copy(&diagnostic->detail.causes.frames[0], &result);
    return result;
}
#endif
