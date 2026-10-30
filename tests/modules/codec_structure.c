// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel
#include "tlv/codec/structure.h"
#include "tlv/reader/diagnostic.h"
#include "tlv/formats/fixed.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "line %d: %s\n", __LINE__, #x);                                        \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
static tlv_result_t decode(const void* context, const tlv_format_t* format, const uint8_t* data,
                           size_t size, void* value, size_t capacity,
                           tlv_codec_diagnostic_t* diagnostic) {
    (void)context;
    (void)format;
    (void)diagnostic;
    if (size != 3 || capacity < 1) return TLV_ERR_INVALID_ARG;
    *(uint8_t*)value = data[2];
    return TLV_OK;
}
int main(void) {
    tlv_fixed_format_t config = {
        {1}, {1, TLV_BYTE_ORDER_BIG_ENDIAN}, TLV_ELEMENT_ORDER_TLV, TLV_LENGTH_SCOPE_VALUE};
    tlv_format_t format;
    CHECK(tlv_fixed_format_init(&format, &config) == TLV_OK);
    const uint8_t                tag[] = {1}, wire[] = {1, 1, 42}, malformed[] = {1, 2, 42};
    const tlv_schema_entry_t     field = {tlv_tag(tag, 1), 1, 1, 0, "value", 0};
    const tlv_structure_rule_t   rule = {&field, 1, 1, TLV_SCHEMA_PRIMITIVE, NULL, 0};
    const tlv_structure_schema_t schema = {&rule, 1, 0, NULL, 0, TLV_SCHEMA_ORDER_ANY};
    const tlv_structure_codec_t  codec = {NULL, &format, &schema, 8, 16, decode, NULL};
    uint8_t                      value = 0;
    tlv_codec_diagnostic_t       diagnostic;
    CHECK(tlv_structure_decode(&codec, wire, sizeof wire, &value, 1, &diagnostic) == TLV_OK &&
          value == 42);
    CHECK(tlv_structure_decode(&codec, malformed, sizeof malformed, &value, 1, &diagnostic) !=
          TLV_OK);
    CHECK(diagnostic.causes.count == 1);
    tlv_reader_detail_t reader;
    CHECK(tlv_reader_detail_copy(&diagnostic.causes.frames[0], &reader) == TLV_OK);
    CHECK(reader.declared_length == 2 && reader.available == 1);
    CHECK(tlv_structure_decode(&codec, NULL, 0, &value, 1, &diagnostic) == TLV_ERR_SCHEMA);
    tlv_schema_detail_t detail;
    CHECK(tlv_schema_detail_copy(&diagnostic.causes.frames[0], &detail) == TLV_OK);
    CHECK(detail.kind == TLV_SCHEMA_ISSUE_MISSING && detail.min_occurs == 1);
    return 0;
}
