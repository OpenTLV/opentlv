// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#include "opentlv/builtin/lldp/schema.h"
#include "opentlv/builtin/lldp/lldp.h"
#include "opentlv/io/reader/reader.h"
#include <string.h>

static const uint8_t lldp_schema_identifiers[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 127};
static const tlv_schema_entry_t lldp_schema_fields[] = {
    {{lldp_schema_identifiers + 0, 1}, 0, 0, 0, "End of LLDPDU", 0},
    {{lldp_schema_identifiers + 1, 1}, 2, 256, 0, "Chassis ID", 0},
    {{lldp_schema_identifiers + 2, 1}, 2, 256, 0, "Port ID", 0},
    {{lldp_schema_identifiers + 3, 1}, 2, 2, 0, "Time To Live", 0},
    {{lldp_schema_identifiers + 4, 1}, 0, 255, 0, "Port Description", 0},
    {{lldp_schema_identifiers + 5, 1}, 0, 255, 0, "System Name", 0},
    {{lldp_schema_identifiers + 6, 1}, 0, 255, 0, "System Description", 0},
    {{lldp_schema_identifiers + 7, 1}, 4, 4, 0, "System Capabilities", 0},
    {{lldp_schema_identifiers + 8, 1}, 9, 167, 0, "Management Address", 0},
    {{lldp_schema_identifiers + 9, 1}, 4, 511, 0, "Organisationally Specific", 0}};

static const tlv_structure_rule_t lldp_schema_rules[] = {
    {&lldp_schema_fields[0], 0, 1, TLV_SCHEMA_PRIMITIVE, NULL, 0},
    {&lldp_schema_fields[1], 1, 1, TLV_SCHEMA_PRIMITIVE, NULL, 0},
    {&lldp_schema_fields[2], 1, 1, TLV_SCHEMA_PRIMITIVE, NULL, 0},
    {&lldp_schema_fields[3], 1, 1, TLV_SCHEMA_PRIMITIVE, NULL, 0},
    {&lldp_schema_fields[4], 0, 1, TLV_SCHEMA_PRIMITIVE, NULL, 0},
    {&lldp_schema_fields[5], 0, 1, TLV_SCHEMA_PRIMITIVE, NULL, 0},
    {&lldp_schema_fields[6], 0, 1, TLV_SCHEMA_PRIMITIVE, NULL, 0},
    {&lldp_schema_fields[7], 0, 1, TLV_SCHEMA_PRIMITIVE, NULL, 0},
    {&lldp_schema_fields[8], 0, SIZE_MAX, TLV_SCHEMA_PRIMITIVE, NULL, 0},
    {&lldp_schema_fields[9], 0, SIZE_MAX, TLV_SCHEMA_PRIMITIVE, NULL, 0}};

const tlv_structure_schema_t tlv_lldp_schema = {
    lldp_schema_rules,   sizeof(lldp_schema_rules) / sizeof(lldp_schema_rules[0]), 1, NULL, 0,
    TLV_SCHEMA_ORDER_ANY};

static tlv_result_t lldp_schema_failure(tlv_schema_diagnostic_t* diagnostic, tlv_result_t rc,
                                        size_t offset, const char* expected) {
    if (diagnostic) {
        tlv_diagnostic_init(&diagnostic->diagnostic, rc, TLV_DIAGNOSTIC_SEVERITY_ERROR);
        if (rc != TLV_ERR_NULL_ARG && rc != TLV_ERR_INVALID_ARG) {
            tlv_diagnostic_set_location(&diagnostic->diagnostic, TLV_LOCATION_INPUT,
                                        TLV_LOCATION_POINT, offset, offset);
            diagnostic->diagnostic.location.kind = TLV_LOCATION_POINT;
        }
        if (rc == TLV_ERR_SCHEMA) {
            diagnostic->cause = TLV_SCHEMA_CAUSE_SCHEMA;
            diagnostic->detail.schema.kind = TLV_SCHEMA_ISSUE_UNEXPECTED;
        }
        diagnostic->diagnostic.expected = expected;
        diagnostic->diagnostic.actual = tlv_result_string(rc);
    }
    return rc;
}

tlv_result_t tlv_lldp_validate(const uint8_t* data, size_t size, size_t max_elements,
                               tlv_schema_diagnostic_t* diagnostic) {
    tlv_reader_t reader;
    tlv_element_t element;
    size_t index = 0;
    tlv_result_t rc;
    if (diagnostic) memset(diagnostic, 0, sizeof(*diagnostic));
    if (!data && size)
        return lldp_schema_failure(diagnostic, TLV_ERR_NULL_ARG, 0, "non-NULL TLV region");
    rc = tlv_schema_validate(data, size, &tlv_format_lldp, &tlv_lldp_schema, 0, max_elements,
                             diagnostic);
    if (rc != TLV_OK) return rc;
    rc = tlv_reader_init(&reader, data, size, &tlv_format_lldp);
    if (rc != TLV_OK) return lldp_schema_failure(diagnostic, rc, 0, "readable LLDP region");
    while (!tlv_reader_at_end(&reader)) {
        size_t offset = reader.pos;
        rc = tlv_reader_next(&reader, &element);
        if (rc != TLV_OK) return lldp_schema_failure(diagnostic, rc, offset, "complete LLDP TLV");
        if (index < 3 && element.tag.data[0] != index + 1) {
            rc = lldp_schema_failure(diagnostic, TLV_ERR_SCHEMA, offset,
                                     "Chassis ID, Port ID, TTL prefix");
            if (diagnostic) {
                diagnostic->cause = TLV_SCHEMA_CAUSE_SCHEMA;
                diagnostic->detail.schema.kind = TLV_SCHEMA_ISSUE_ORDER;
            }
            return rc;
        }
        if (element.tag.data[0] == 0 && !tlv_reader_at_end(&reader))
            return lldp_schema_failure(diagnostic, TLV_ERR_SCHEMA, reader.pos,
                                       "end of region after End TLV");
        ++index;
    }
    return TLV_OK;
}
