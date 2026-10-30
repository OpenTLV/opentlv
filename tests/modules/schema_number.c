// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel
#include "tlv/schema/number.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "line %d: %s\n", __LINE__, #x);                                        \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
int main(void) {
    const uint8_t             tag[] = {1};
    const tlv_schema_entry_t  field = {tlv_tag(tag, 1), 3, 3, 0, "number", 0};
    const tlv_schema_number_t config = {&field, {TLV_NUMBER_BINARY_BE, 0, 0}};
    const tlv_codec_t         codec = tlv_schema_number_codec(&config);
    uint64_t                  value = 258, decoded = 0;
    uint8_t                   wire[3];
    size_t                    written = 0;
    tlv_codec_diagnostic_t    diagnostic;
    CHECK(tlv_codec_encode(&codec, &value, sizeof value, wire, sizeof wire, &written,
                           &diagnostic) == TLV_OK);
    CHECK(written == 3 && wire[0] == 0 && wire[1] == 1 && wire[2] == 2);
    CHECK(tlv_codec_decode(&codec, wire, 3, &decoded, sizeof decoded, &diagnostic) == TLV_OK &&
          decoded == value);
    CHECK(tlv_codec_decode(&codec, wire, 2, &decoded, sizeof decoded, &diagnostic) ==
          TLV_ERR_SCHEMA);
    CHECK(diagnostic.codec.operation == TLV_CODEC_OP_DECODE && diagnostic.causes.count == 1);
    tlv_schema_detail_t detail;
    CHECK(tlv_schema_detail_copy(&diagnostic.causes.frames[0], &detail) == TLV_OK);
    CHECK(detail.tag.data == tag && detail.field == field.name && detail.min_length == 3 &&
          detail.actual_length == 2);
    tlv_codec_diagnostic_init(&diagnostic, TLV_CODEC_OP_MEASURE);
    CHECK(tlv_schema_number_validate_length(&field, 2, &diagnostic) == TLV_ERR_SCHEMA);
    CHECK(diagnostic.codec.operation == TLV_CODEC_OP_MEASURE && diagnostic.causes.count == 1);
    CHECK(tlv_schema_number_validate_length(NULL, 2, &diagnostic) == TLV_ERR_NULL_ARG);
    return 0;
}
