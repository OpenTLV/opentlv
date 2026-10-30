// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel
#include "tlv++/schema/schema.hpp"
#include <cstring>

#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) return __LINE__;                                                                 \
    } while (0)

int main() {
    const tlv::schema_failure preflight = tlv::schema_failure::from_result(TLV_ERR_NULL_ARG);
    CHECK(preflight.status() == tlv::errc::null_argument);
    CHECK(preflight.native().diagnostic.code == TLV_ERR_NULL_ARG);
    CHECK(preflight.native().cause == TLV_SCHEMA_CAUSE_NONE);
    CHECK(preflight.schema_kind() == tlv::schema_issue::none);

    const uint8_t           identifier[] = {1};
    tlv_schema_diagnostic_t native{};
    tlv_diagnostic_init(&native.diagnostic, TLV_ERR_INVALID_SCHEMA, TLV_DIAGNOSTIC_SEVERITY_ERROR);
    native.cause = TLV_SCHEMA_CAUSE_SCHEMA;
    native.detail.schema.kind = TLV_SCHEMA_ISSUE_DEFINITION;
    native.detail.schema.tag = tlv_tag(identifier, 1);
    native.detail.schema.definition.kind = TLV_SCHEMA_DEFINITION_RULE;
    native.detail.schema.definition.owner = identifier;
    native.detail.schema.definition.index = 7;
    native.detail.schema.field = "field";
    tlv::schema_failure failure(native);
    native = {};
    CHECK(failure.status() == tlv::errc::invalid_schema);
    CHECK(failure.schema_kind() == tlv::schema_issue::definition);
    CHECK(failure.definition_kind() == tlv::schema_definition_kind::rule);
    CHECK(failure.definition().owner == identifier && failure.definition().index == 7);
    CHECK(std::strcmp(tlv::message(failure.definition_kind()), "rule") == 0);
    CHECK(std::strcmp(failure.native().detail.schema.field, "field") == 0);
    const tlv::error common = tlv::to_error(failure);
    CHECK(common.status() == failure.status() && common.has_tag());
    CHECK(common.tag().size() == 1);

    tlv_reader_detail_t reader{};
    reader.operation = TLV_READER_OP_VALUE;
    reader.has_available = 1;
    reader.available = 2;
    tlv_diagnostic_init(&native.diagnostic, TLV_ERR_TRUNCATED, TLV_DIAGNOSTIC_SEVERITY_ERROR);
    native.cause = TLV_SCHEMA_CAUSE_READER;
    CHECK(tlv_reader_detail_attach(&native.detail.causes, &reader) == TLV_OK);
    const tlv::schema_failure delegated(native);
    native = {};
    reader = {};
    CHECK(tlv_reader_detail_copy(&delegated.native().detail.causes.frames[0], &reader) == TLV_OK);
    CHECK(reader.available == 2 && reader.has_available);
    CHECK(delegated.status() == tlv::errc::truncated);
    CHECK(delegated.schema_kind() == tlv::schema_issue::none);
    return 0;
}
