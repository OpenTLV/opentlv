// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#ifndef OPENTLV_BENCHMARK_TIMING_HPP
#define OPENTLV_BENCHMARK_TIMING_HPP

#include "adapter.h"
#include <opentlv/tlv.hpp>

namespace opentlv_bench {

// Inputs and destinations borrow fixture storage. Initialize cursors and collect
// observations outside the timed interval. Only run() belongs in that interval.
class c_timing {
public:
    c_timing(bench_operation op, const bench_input &in, uint8_t *destination,
             const bench_output &initial);
    c_timing(const c_timing &) = delete;
    c_timing &operator=(const c_timing &) = delete;

    tlv_result_t reset_cursor();
    tlv_result_t run() { return step_(&state_); }
    bench_output output(tlv_result_t result) const;

private:
    bench_operation operation_;
    bench_c_step step_;
    bench_c_state state_;
};

class cpp_timing {
public:
    cpp_timing(bench_operation op, const bench_input &in, uint8_t *destination,
               const bench_output &initial);
    cpp_timing(const cpp_timing &) = delete;
    cpp_timing &operator=(const cpp_timing &) = delete;

    tlv_result_t reset_cursor();
    tlv_result_t run() { return opentlv::core::to_c(step_(*this)); }
    bench_output output(tlv_result_t result) const;

private:
    bench_operation operation_;
    opentlv::core::result (*step_)(cpp_timing &);
    opentlv::core::fixed_format format_;
    opentlv::core::element_view source_;
    opentlv::core::byte_view input_;
    opentlv::core::mutable_byte_view destination_;
    tlv_size_t length_;
    bench_output output_;
    opentlv::core::element_view view_;
    opentlv::core::diagnostic diagnostic_;
    opentlv::core::diagnostic *details_;
    opentlv::io::reader reader_;
    opentlv::io::writer writer_;
};

} // namespace opentlv_bench

#endif /* OPENTLV_BENCHMARK_TIMING_HPP */
