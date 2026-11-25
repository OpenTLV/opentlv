// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#ifndef OPENTLV_BENCHMARK_FIXTURES_HPP
#define OPENTLV_BENCHMARK_FIXTURES_HPP

#include "adapter.h"

#include <array>
#include <string>
#include <vector>

namespace opentlv_bench {

struct workload {
    bench_operation operation;
    tlv_fixed_length_t field;
    size_t offset;
    size_t payload_size;
    bool complete;
    bool diagnostic;

    std::string id() const;
};

std::vector<workload> workloads();

// The length begins at byte 64 or 65 so that both standalone fields and
// record lengths have the requested alignment. Leave guards on both ends.
struct alignas(64) wire_buffer {
    std::array<uint8_t, 64 + 1 + 8 + 513 + 1> bytes;
};

class fixture {
public:
    explicit fixture(workload entry);
    bench_input input() const;
    size_t data_offset() const;
    static bench_output initial_output();

    // An empty string means every observed result matched the golden fixture.
    std::string validate(decltype(&bench_run_c) run) const;

private:
    workload entry_;
    wire_buffer golden_;
    size_t header_size() const;
    size_t wire_size() const;
    bench_output expected(const bench_input &in) const;
};

} // namespace opentlv_bench

#endif /* OPENTLV_BENCHMARK_FIXTURES_HPP */
