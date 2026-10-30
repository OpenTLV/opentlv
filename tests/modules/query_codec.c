// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel
#include "tlv/query/adapters.h"
#include "tlv/codec/values.h"
#include <string.h>

#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) return __LINE__;                                                                 \
    } while (0)

int main(void) {
    const tlv_query_codec_adapter_t adapter = {&tlv_codec_int64_minimal_be, TLV_QUERY_CODEC_SIGNED};
    const uint8_t                   value[] = {42};
    int64_t                         scratch = 0;
    tlv_query_result_t              result;
    tlv_diagnostic_envelope_t       diagnostic;
    CHECK(tlv_query_codec_decode(&adapter, NULL, value, sizeof value, &scratch, sizeof scratch,
                                 &result, &diagnostic) == TLV_OK);
    CHECK(result.kind == TLV_QUERY_RESULT_INTEGER && result.integer == 42);
    const uint8_t invalid[] = {0, 42};
    CHECK(tlv_query_codec_decode(&adapter, NULL, invalid, sizeof invalid, &scratch, sizeof scratch,
                                 &result, &diagnostic) == TLV_ERR_INVALID_VALUE);
    CHECK(diagnostic.causes.count == 1);
    tlv_codec_detail_t detail;
    CHECK(tlv_codec_detail_import(&diagnostic.causes, &detail) == TLV_OK);
    CHECK(detail.reason == TLV_CODEC_REASON_CANONICAL);
    CHECK(strcmp(diagnostic.causes.frames[0].domain->name, "opentlv.codec") == 0);
    size_t count = 0;
    CHECK(tlv_query_builtin_hooks(&count) != NULL && count == 3);
    return 0;
}
