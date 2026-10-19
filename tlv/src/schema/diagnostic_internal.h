// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#ifndef OPENTLV_SCHEMA_DIAGNOSTIC_INTERNAL_H
#define OPENTLV_SCHEMA_DIAGNOSTIC_INTERNAL_H
#include "tlv/schema/schema.h"

/* Violation detail shared by tlv_schema_validate() and the report API, so both
 * describe one violation identically. Each helper fills only its kind's fields. */

static void schema_issue_rule_occurs(tlv_schema_diagnostic_t* diagnostic,
                                     const tlv_structure_rule_t* rule, size_t occurs) {
    diagnostic->detail.schema.field = rule->entry->name;
    diagnostic->detail.schema.is_group = 0;
    diagnostic->detail.schema.has_occurs = 1;
    diagnostic->detail.schema.min_occurs = rule->min_occurs;
    diagnostic->detail.schema.max_occurs = rule->max_occurs;
    diagnostic->detail.schema.occurs = occurs;
}

static void schema_issue_group_occurs(tlv_schema_diagnostic_t* diagnostic,
                                      const tlv_structure_group_t* group, size_t occurs) {
    diagnostic->detail.schema.field = group->name;
    diagnostic->detail.schema.is_group = 1;
    diagnostic->detail.schema.has_occurs = 1;
    diagnostic->detail.schema.min_occurs = group->min_occurs;
    diagnostic->detail.schema.max_occurs = group->max_occurs;
    diagnostic->detail.schema.occurs = occurs;
}

static void schema_issue_length(tlv_schema_diagnostic_t* diagnostic,
                                const tlv_structure_rule_t* rule, size_t actual_length) {
    diagnostic->detail.schema.field = rule->entry->name;
    diagnostic->detail.schema.has_length = 1;
    diagnostic->detail.schema.min_length = rule->entry->min_length;
    diagnostic->detail.schema.max_length = rule->entry->max_length;
    diagnostic->detail.schema.actual_length = actual_length;
    diagnostic->detail.schema.length_multiple = rule->entry->length_multiple;
    diagnostic->detail.schema.length_flags = rule->entry->flags;
}

static void schema_issue_form(tlv_schema_diagnostic_t* diagnostic, const tlv_structure_rule_t* rule,
                              int actual_constructed) {
    diagnostic->detail.schema.field = rule->entry->name;
    diagnostic->detail.schema.has_form = 1;
    diagnostic->detail.schema.expected_form = rule->kind;
    diagnostic->detail.schema.actual_constructed = actual_constructed;
}
#endif
