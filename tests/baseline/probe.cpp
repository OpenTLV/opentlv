// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

// Observation only: compile against the reference library, never into it.
#include "tlv/builtins/asn1/ber.h"
#include "tlv/document/document.h"
#include "tlv/reader/reader.h"
#include "tlv/schema/number.h"
#include "tlv/schema/schema.h"
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using Bytes = std::vector<uint8_t>;

std::string hex(const uint8_t* data, size_t size) {
    const char* digits = "0123456789abcdef";
    std::string result;
    for (size_t i = 0; i < size; ++i) {
        result += digits[data[i] >> 4];
        result += digits[data[i] & 15];
    }
    return result;
}

void require(tlv_result_t result) {
    if (result != TLV_OK) throw std::runtime_error(tlv_result_string(result));
}

void common(const tlv_diagnostic_t& d) {
    std::cout << "\"code\":" << int(d.code) << ",\"severity\":" << int(d.severity)
              << ",\"location\":{\"domain\":" << int(d.location.domain)
              << ",\"kind\":" << int(d.location.kind) << ",\"begin\":" << d.location.begin
              << ",\"end\":" << d.location.end << "},\"has_path\":" << d.has_path << ",\"path\":[";
    for (size_t i = 0; d.has_path && i < d.path.length; ++i) {
        if (i) std::cout << ',';
        std::cout << std::quoted(hex(d.path.tags[i].data, d.path.tags[i].size));
    }
    std::cout << "],\"path_omitted\":" << (d.has_path ? d.path.omitted : 0);
}

void reader(const tlv_reader_detail_t& d) {
    std::cout << ",\"reader\":{\"operation\":" << int(d.operation) << ",\"has_tag\":" << d.has_tag
              << ",\"tag\":" << std::quoted(d.has_tag ? hex(d.tag.data, d.tag.size) : "")
              << ",\"has_tag_offset\":" << d.has_tag_offset << ",\"tag_offset\":" << d.tag_offset
              << ",\"has_length_offset\":" << d.has_length_offset
              << ",\"length_offset\":" << d.length_offset
              << ",\"has_value_offset\":" << d.has_value_offset
              << ",\"value_offset\":" << d.value_offset
              << ",\"has_declared_length\":" << d.has_declared_length
              << ",\"declared_length\":" << d.declared_length
              << ",\"has_available\":" << d.has_available << ",\"available\":" << d.available
              << ",\"has_required\":" << d.has_required << ",\"required\":" << d.required
              << ",\"has_enclosing_end\":" << d.has_enclosing_end
              << ",\"enclosing_end\":" << d.enclosing_end
              << ",\"has_raw_length\":" << d.has_raw_length << ",\"raw_length\":"
              << std::quoted(d.has_raw_length ? hex(d.raw_length.data, d.raw_length.size) : "")
              << '}';
}

void schema(const tlv_schema_detail_t& d) {
    std::cout << ",\"schema\":{\"kind\":" << int(d.kind)
              << ",\"field\":" << std::quoted(d.field ? d.field : "")
              << ",\"tag\":" << std::quoted(hex(d.tag.data, d.tag.size))
              << ",\"has_length\":" << d.has_length << ",\"min_length\":" << d.min_length
              << ",\"max_length\":" << d.max_length << ",\"actual_length\":" << d.actual_length
              << ",\"length_multiple\":" << d.length_multiple
              << ",\"length_flags\":" << d.length_flags << ",\"has_occurs\":" << d.has_occurs
              << ",\"min_occurs\":" << d.min_occurs << ",\"max_occurs\":" << d.max_occurs
              << ",\"occurs\":" << d.occurs << ",\"definition_kind\":" << int(d.definition.kind)
              << ",\"definition_index\":" << d.definition.index << '}';
}

void document() {
    const Bytes            input = {0x30, 0x03, 0x04, 0x01, 0xaa, 0x04, 0x01, 0xbb};
    tlv_document_options_t options{};
    require(tlv_document_options_init(&options, &tlv_format_ber));
    options.retain_source_locations = 1;
    tlv_document_t* doc = nullptr;
    require(tlv_document_parse(input.data(), input.size(), &options, &doc, nullptr));
    auto root = tlv_document_first(doc);
    auto leaf = tlv_node_first_child(root);
    auto sibling = tlv_node_next(root);
    std::cout << "\"document\":{\"input\":" << std::quoted(hex(input.data(), input.size()))
              << ",\"nodes\":" << tlv_document_count(doc)
              << ",\"leaf_offset_before\":" << tlv_node_source_location(leaf).offset;
    const uint8_t replacement[] = {1, 2, 3};
    require(tlv_node_set_value(leaf, replacement, sizeof replacement));
    size_t size = 0, written = 0;
    require(tlv_document_encoded_size(doc, &size));
    Bytes output(size);
    require(tlv_document_encode(doc, output.data(), output.size(), &written));
    std::cout << ",\"replacement\":\"010203\",\"encoded\":"
              << std::quoted(hex(output.data(), written))
              << ",\"root_has_offset_after\":" << tlv_node_source_location(root).has_offset
              << ",\"leaf_has_offset_after\":" << tlv_node_source_location(leaf).has_offset
              << ",\"sibling_original_offset_after\":" << tlv_node_source_location(sibling).offset
              << '}';
    tlv_document_free(doc);
}

void schema_cases() {
    const tlv_schema_entry_t     child = {TLV_TAG(0x80), 1, 1, 0, "id", 0};
    const tlv_structure_rule_t   child_rule = {&child, 1, 1, TLV_SCHEMA_PRIMITIVE, nullptr, 0};
    const tlv_structure_schema_t children = {&child_rule, 1, 0, nullptr, 0, TLV_SCHEMA_ORDER_ANY};
    const tlv_schema_entry_t     root = {TLV_TAG(0x30), 0, SIZE_MAX, 0, "record", 0};
    const tlv_structure_rule_t   rule = {&root, 1, 1, TLV_SCHEMA_CONSTRUCTED, &children, 0};
    const tlv_structure_schema_t definition = {&rule, 1, 0, nullptr, 0, TLV_SCHEMA_ORDER_ANY};
    const std::vector<Bytes>     inputs = {
        {0x30, 3, 0x80, 1, 1}, {0x30, 4, 0x80, 2, 1, 2}, {0x30, 3, 0x80, 2, 1}};
    std::cout << ",\"schema_validation\":[";
    for (size_t i = 0; i < inputs.size(); ++i) {
        if (i) std::cout << ',';
        const auto&             wire = inputs[i];
        tlv_schema_diagnostic_t d{};
        const auto              result =
            tlv_schema_validate(wire.data(), wire.size(), &tlv_format_ber, &definition, 8, 100, &d);
        std::cout << "{\"wire\":" << std::quoted(hex(wire.data(), wire.size()))
                  << ",\"result\":" << int(result);
        if (result != TLV_OK) {
            std::cout << ",\"diagnostic\":{";
            common(d.diagnostic);
            std::cout << ",\"cause\":" << int(d.cause);
            if (d.cause == TLV_SCHEMA_CAUSE_READER) reader(d.detail.reader);
            if (d.cause == TLV_SCHEMA_CAUSE_SCHEMA) schema(d.detail.schema);
            std::cout << '}';
        }
        std::cout << '}';
    }
    std::cout << ']';
}

void codec_cases() {
    const tlv_schema_entry_t  field = {TLV_TAG(1), 1, 3, TLV_SCHEMA_LENGTH_ENDPOINTS, "number", 0};
    const tlv_schema_number_t config = {&field, {TLV_NUMBER_BINARY_BE, 0, 0}};
    const auto                codec = tlv_schema_number_codec(&config);
    const uint64_t            number = 256;
    uint8_t                   wire[3]{};
    size_t                    written = 0;
    require(tlv_codec_encode(&codec, &number, sizeof number, wire, sizeof wire, &written, nullptr));
    uint64_t decoded = 99;
    require(tlv_codec_decode(&codec, wire, written, &decoded, sizeof decoded, nullptr));
    std::cout << ",\"codec\":{\"number\":" << number
              << ",\"encoded\":" << std::quoted(hex(wire, written)) << ",\"decoded\":" << decoded;
    tlv_codec_diagnostic_t d{};
    decoded = 99;
    const auto result = tlv_codec_decode(&codec, wire + 1, 2, &decoded, sizeof decoded, &d);
    std::cout << ",\"invalid_wire\":" << std::quoted(hex(wire + 1, 2))
              << ",\"invalid_result\":" << int(result)
              << ",\"destination_after_failure\":" << decoded << ",\"diagnostic\":{";
    common(d.diagnostic);
#ifdef OPENTLV_CODEC_API
    tlv_schema_detail_t detail{};
    tlv_codec_cause_t   cause = TLV_CODEC_CAUSE_NONE;
    if (d.causes.count && tlv_schema_detail_copy(&d.causes.frames[0], &detail) == TLV_OK)
        cause = TLV_CODEC_CAUSE_SCHEMA;
    std::cout << ",\"operation\":" << int(d.codec.operation) << ",\"cause\":" << int(cause)
              << ",\"reason\":" << int(d.codec.reason)
              << ",\"violation\":" << int(d.codec.violation)
              << ",\"reported\":" << int(d.codec.reported);
    if (cause == TLV_CODEC_CAUSE_SCHEMA) schema(detail);
#else
    std::cout << ",\"operation\":" << int(d.codec.operation) << ",\"cause\":" << int(d.codec.cause)
              << ",\"reason\":" << int(d.codec.reason)
              << ",\"violation\":" << int(d.codec.violation)
              << ",\"reported\":" << int(d.codec.reported);
    if (d.codec.cause == TLV_CODEC_CAUSE_SCHEMA) schema(d.codec.detail.schema);
#endif
    std::cout << "}}";
}
} // namespace

int main() try {
    std::cout << '{';
    document();
    schema_cases();
    codec_cases();
    std::cout << "}\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
