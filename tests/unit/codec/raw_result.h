// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel
#ifndef OPENTLV_TEST_RAW_RESULT_H
#define OPENTLV_TEST_RAW_RESULT_H
#include "tlv/codec/codec.h"
#ifdef __cplusplus
extern "C" {
#endif
tlv_result_t tlv_test_raw_decode(const void*, const uint8_t*, size_t, void*, size_t,
                                 tlv_codec_diagnostic_t*);
tlv_result_t tlv_test_raw_encode(const void*, const void*, size_t, uint8_t*, size_t, size_t*,
                                 tlv_codec_diagnostic_t*);
#ifdef __cplusplus
}
#endif
#endif
