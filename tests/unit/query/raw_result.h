// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel
#ifndef OPENTLV_TEST_QUERY_RAW_RESULT_H
#define OPENTLV_TEST_QUERY_RAW_RESULT_H
#include "tlv/query/program.h"
#ifdef __cplusplus
extern "C" {
#endif
tlv_result_t tlv_test_raw_query(const void*, const tlv_tree_event_t*, const uint8_t*, size_t, void*,
                                size_t, tlv_query_result_t*, tlv_diagnostic_envelope_t*);
#ifdef __cplusplus
}
#endif
#endif
