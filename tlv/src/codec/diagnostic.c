// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#include "tlv/codec/diagnostic.h"
#include <string.h>

void tlv_codec_diagnostic_init(tlv_codec_diagnostic_t* diagnostic,
                               tlv_codec_operation_t operation) {
    if (!diagnostic) return;
    memset(diagnostic, 0, sizeof(*diagnostic));
    diagnostic->diagnostic.severity = TLV_DIAGNOSTIC_SEVERITY_INFO;
    diagnostic->codec.operation = operation;
}
