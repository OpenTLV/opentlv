// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel
#include "tlv/schema/schema.h"
#include "tlv/schema/constraint.h"
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
int main(void) {
    tlv_fixed_format_t config = {
        {1}, {1, TLV_BYTE_ORDER_BIG_ENDIAN}, TLV_ELEMENT_ORDER_TLV, TLV_LENGTH_SCOPE_VALUE};
    tlv_format_t format;
    CHECK(tlv_fixed_format_init(&format, &config) == TLV_OK);
    const uint8_t                tag[] = {1}, wire[] = {1, 1, 42}, short_wire[] = {1, 2, 42};
    const tlv_schema_entry_t     field = {tlv_tag(tag, 1), 1, 2, 0, "value", 1};
    const tlv_structure_rule_t   rule = {&field, 1, 1, TLV_SCHEMA_PRIMITIVE, NULL, 0};
    const tlv_structure_schema_t schema = {&rule, 1, 0, NULL, 0, TLV_SCHEMA_ORDER_ANY};
    tlv_schema_checked_t         checked;
    tlv_schema_diagnostic_t      diagnostic;
    CHECK(tlv_schema_prepare(&checked, &schema, &diagnostic) == TLV_OK);
    CHECK(tlv_schema_validate_checked(&checked, wire, sizeof wire, &format, 8, 16, &diagnostic) ==
          TLV_OK);
    CHECK(tlv_schema_validate(NULL, 0, &format, &schema, 8, 16, &diagnostic) == TLV_ERR_SCHEMA);
    CHECK(diagnostic.cause == TLV_SCHEMA_CAUSE_SCHEMA);
    CHECK(diagnostic.detail.schema.kind == TLV_SCHEMA_ISSUE_MISSING);
    CHECK(diagnostic.detail.schema.min_occurs == 1 && diagnostic.detail.schema.occurs == 0);
    CHECK(diagnostic.diagnostic.location.kind == TLV_LOCATION_SCOPE_END);
    CHECK(tlv_schema_validate(short_wire, sizeof short_wire, &format, &schema, 8, 16,
                              &diagnostic) != TLV_OK);
    CHECK(diagnostic.cause == TLV_SCHEMA_CAUSE_READER && diagnostic.detail.causes.count == 1);
    tlv_reader_detail_t reader;
    CHECK(tlv_reader_detail_copy(&diagnostic.detail.causes.frames[0], &reader) == TLV_OK);
    CHECK(reader.has_declared_length && reader.declared_length == 2);
    CHECK(reader.has_available && reader.available == 1);
    tlv_schema_diagnostic_t copy = diagnostic;
    memset(&diagnostic, 0, sizeof diagnostic);
    CHECK(tlv_reader_detail_copy(&copy.detail.causes.frames[0], &reader) == TLV_OK);
    tlv_schema_diagnostic_report_t report = {&diagnostic, 1, 0};
    CHECK(tlv_schema_validate_all_checked(&checked, NULL, 0, &format, 8, 16,
                                          TLV_SCHEMA_UNKNOWN_BY_SCHEMA, &report,
                                          NULL) == TLV_ERR_SCHEMA);
    CHECK(report.count == 1 && diagnostic.detail.schema.kind == TLV_SCHEMA_ISSUE_MISSING);
    tlv_diagnostic_causes_t causes = {0};
    CHECK(tlv_schema_detail_attach(&causes, &diagnostic.detail.schema) == TLV_OK);
    CHECK(causes.frames[0].domain->id == 5);
    CHECK(strcmp(causes.frames[0].domain->code_name(1, TLV_SCHEMA_ISSUE_MISSING), "missing") == 0);
    CHECK(strcmp(causes.frames[0].domain->code_name(2, TLV_SCHEMA_DEFINITION_RULE), "rule") == 0);
    return 0;
}
