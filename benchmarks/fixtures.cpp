// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#include "fixtures.hpp"

#include <algorithm>

namespace opentlv_bench {
namespace {

constexpr uint8_t guard = 0xcc;
constexpr uint8_t tag[] = {0x00, 0xa7};
constexpr uint8_t payload_pattern[] = {0x00, 0xff, 0x80};
constexpr uint8_t field_digits[] = {0xef, 0xcd, 0xab, 0x89, 0x67, 0x45, 0x23, 0x81};

bool is_field(bench_operation op)
{
    return op == BENCH_FIELD_READ || op == BENCH_FIELD_WRITE;
}

bool is_measurement(bench_operation op)
{
    return op == BENCH_HEADER_SIZE || op == BENCH_ELEMENT_SIZE;
}

const char *operation_name(bench_operation op)
{
    switch (op) {
    case BENCH_FIELD_READ: return "field-read";
    case BENCH_FIELD_WRITE: return "field-write";
    case BENCH_HEADER_SIZE: return "header-size";
    case BENCH_ELEMENT_SIZE: return "element-size";
    case BENCH_READ: return "read";
    case BENCH_NEXT: return "next";
    case BENCH_WRITE: return "write";
    case BENCH_APPEND: return "append";
    }
    return "unknown";
}

bool same_view(const tlv_element_t &a, const tlv_element_t &b)
{
    return a.tag.data == b.tag.data && a.tag.size == b.tag.size &&
           a.value.data == b.value.data && a.value.size == b.value.size;
}

bool same_diagnostic(const tlv_diagnostic_t &a, const tlv_diagnostic_t &b)
{
    return a.result == b.result && a.field == b.field && a.offset == b.offset &&
           a.required == b.required && a.available == b.available;
}

} // namespace

std::string workload::id() const
{
    std::string name = std::string("v1/") + operation_name(operation) + "/" +
        (field.byte_order == TLV_BYTE_ORDER_BIG_ENDIAN ? "be" : "le") + std::to_string(field.size);
    if (!is_measurement(operation)) {
        name += "/offset" + std::to_string(offset);
    }
    if (!is_field(operation) && operation != BENCH_HEADER_SIZE) {
        name += "/payload" + std::to_string(payload_size);
    }
    if (complete) {
        name += "/complete";
    } else {
        name += is_measurement(operation) ? "/invalid" : "/short";
    }
    if (!is_measurement(operation) && !is_field(operation)) {
        name += diagnostic ? "/diagnostic" : "/plain";
    }
    return name;
}

std::vector<workload> workloads()
{
    std::vector<workload> cases;
    for (size_t width = 1; width <= 8; ++width) {
        for (auto order : {TLV_BYTE_ORDER_BIG_ENDIAN, TLV_BYTE_ORDER_LITTLE_ENDIAN}) {
            const tlv_fixed_length_t field{width, order};
            for (bool complete : {true, false}) {
                cases.push_back({BENCH_HEADER_SIZE, field, 0, 0, complete, false});
                for (size_t offset : {0u, 1u}) {
                    for (auto op : {BENCH_FIELD_READ, BENCH_FIELD_WRITE}) {
                        cases.push_back({op, field, offset, 0, complete, false});
                    }
                }
                for (size_t payload : {0u, 3u, width == 1 ? 255u : 513u}) {
                    cases.push_back({BENCH_ELEMENT_SIZE, field, 0, payload, complete, false});
                    for (auto op : {BENCH_READ, BENCH_NEXT, BENCH_WRITE, BENCH_APPEND}) {
                        for (size_t offset : {0u, 1u}) {
                            for (bool diagnostic : {false, true}) {
                                cases.push_back({op, field, offset, payload, complete, diagnostic});
                            }
                        }
                    }
                }
            }
        }
    }
    return cases;
}

size_t fixture::data_offset() const
{
    return 64 + entry_.offset - (is_field(entry_.operation) ? 0 : sizeof tag);
}

size_t fixture::header_size() const
{
    return sizeof tag + entry_.field.size;
}

size_t fixture::wire_size() const
{
    return is_field(entry_.operation) ? entry_.field.size : header_size() + entry_.payload_size;
}

fixture::fixture(workload entry) : entry_(entry)
{
    golden_.bytes.fill(guard);
    auto *wire = golden_.bytes.data() + data_offset();
    const size_t width = entry.field.size;
    const bool little = entry.field.byte_order == TLV_BYTE_ORDER_LITTLE_ENDIAN;
    if (is_field(entry.operation)) {
        if (little) {
            std::copy_n(field_digits, width, wire);
        } else {
            std::reverse_copy(field_digits, field_digits + width, wire);
        }
        return;
    }

    std::copy_n(tag, sizeof tag, wire);
    auto *length = wire + sizeof tag;
    std::fill_n(length, width, 0);
    // All catalog payload sizes have at most two significant octets.
    length[little ? 0 : width - 1] = static_cast<uint8_t>(entry.payload_size);
    if (width > 1) {
        length[little ? 1 : width - 2] = static_cast<uint8_t>(entry.payload_size / 256);
    }
    auto *value = wire + header_size();
    for (size_t i = 0; i < entry.payload_size; ++i) {
        value[i] = payload_pattern[i % sizeof payload_pattern];
    }
}

bench_input fixture::input() const
{
    const uint8_t *wire = golden_.bytes.data() + data_offset();
    bench_input in{};
    in.format = {{sizeof tag}, entry_.field};
    in.data = wire;
    in.available = wire_size();
    in.length = UINT64_C(0x8123456789abcdef) & (UINT64_MAX >> (8 * (8 - entry_.field.size)));
    in.diagnostic = entry_.diagnostic;
    if (!is_field(entry_.operation)) {
        in.source = {{tag, sizeof tag}, {wire + header_size(), entry_.payload_size}};
    }
    if (!entry_.complete) {
        switch (entry_.operation) {
        case BENCH_HEADER_SIZE: in.format.identifier.size = 0; break;
        case BENCH_ELEMENT_SIZE: in.source.tag.size = 1; break;
        default: --in.available; break;
        }
    }
    return in;
}

bench_output fixture::initial_output()
{
    return {TLV_END, SIZE_MAX, UINT64_MAX, {{tag, 1}, {payload_pattern, 2}}, SIZE_MAX,
            {TLV_END, TLV_FIELD_TAG, 99, 98, 97}};
}

bench_output fixture::expected(const bench_input &in) const
{
    auto out = initial_output();
    const bool complete = entry_.complete;
    const size_t width = entry_.field.size;
    out.result = TLV_OK;
    switch (entry_.operation) {
    case BENCH_FIELD_READ:
        out.result = complete ? TLV_OK : TLV_ERR_TRUNCATED;
        out.count = in.available;
        if (complete) {
            out.length = in.length;
        }
        break;
    case BENCH_FIELD_WRITE:
        if (complete) {
            out.count = width;
        } else {
            out.result = TLV_ERR_BUFFER_TOO_SHORT;
        }
        break;
    case BENCH_HEADER_SIZE:
    case BENCH_ELEMENT_SIZE:
        if (complete) {
            out.count = entry_.operation == BENCH_HEADER_SIZE ? header_size() : wire_size();
        } else {
            out.result = entry_.operation == BENCH_HEADER_SIZE ? TLV_ERR_INVALID_ARG : TLV_ERR_INVALID_TAG_SIZE;
        }
        break;
    case BENCH_READ:
    case BENCH_NEXT:
        if (complete) {
            out.view = {{in.data, sizeof tag}, {in.data + header_size(), entry_.payload_size}};
            if (entry_.operation == BENCH_READ) {
                out.count = wire_size();
            }
        } else {
            out.result = TLV_ERR_TRUNCATED;
        }
        break;
    case BENCH_WRITE:
    case BENCH_APPEND:
        out.result = complete ? TLV_OK : TLV_ERR_BUFFER_TOO_SHORT;
        if (entry_.operation == BENCH_WRITE) {
            out.count = wire_size();
        }
        break;
    }
    if (entry_.operation == BENCH_NEXT || entry_.operation == BENCH_APPEND) {
        out.position = complete ? wire_size() : 0;
    }
    if (entry_.diagnostic) {
        if (complete) {
            out.diagnostic = {TLV_OK, TLV_FIELD_NONE, 0, 0, 0};
        } else if (entry_.operation == BENCH_WRITE || entry_.operation == BENCH_APPEND) {
            out.diagnostic = {out.result, TLV_FIELD_ELEMENT, 0, wire_size(), in.available};
        } else if (entry_.payload_size == 0) {
            out.diagnostic = {out.result, TLV_FIELD_LENGTH, sizeof tag, width, width - 1};
        } else {
            out.diagnostic = {out.result, TLV_FIELD_VALUE, header_size(), entry_.payload_size, entry_.payload_size - 1};
        }
    }
    return out;
}

std::string fixture::validate(decltype(&bench_run_c) run) const
{
    const bench_input in = input();
    const bench_output want = expected(in);
    bench_output got = initial_output();
    wire_buffer output;
    output.bytes.fill(guard);
    run(entry_.operation, &in, output.bytes.data() + data_offset(), &got);
    if (got.result != want.result) return "result";
    if (got.count != want.count) return "byte count";
    if (got.length != want.length) return "decoded length";
    if (got.position != want.position) return "cursor position";
    if (!same_view(got.view, want.view)) return "borrowed view";
    if (!same_diagnostic(got.diagnostic, want.diagnostic)) return "diagnostic";

    const bool write = entry_.operation == BENCH_FIELD_WRITE || entry_.operation == BENCH_WRITE ||
                       entry_.operation == BENCH_APPEND;
    if (write && entry_.complete) {
        if (output.bytes != golden_.bytes) return "wire bytes or guards";
    } else if (!std::all_of(output.bytes.begin(), output.bytes.end(), [](uint8_t b) { return b == guard; })) {
        return "destination preservation";
    }
    return {};
}

} // namespace opentlv_bench
