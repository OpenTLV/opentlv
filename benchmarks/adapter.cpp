// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#include "timing.hpp"

namespace opentlv_bench {
namespace core = opentlv::core;

c_timing::c_timing(bench_operation op, const bench_input &in, uint8_t *destination,
                   const bench_output &initial)
    : operation_(op), step_(bench_select_c(op)), state_{in, initial, destination, nullptr, {}, {}}
{
    if (in.diagnostic) {
        state_.details = &state_.output.diagnostic;
    }
}

tlv_result_t c_timing::reset_cursor()
{
    const auto &in = state_.input;
    if (operation_ == BENCH_NEXT) {
        return tlv_reader_init(&state_.reader, in.data, in.available, &in.format);
    }
    if (operation_ == BENCH_APPEND) {
        return tlv_writer_init(&state_.writer, state_.destination, in.available, &in.format);
    }
    return TLV_OK;
}

bench_output c_timing::output(tlv_result_t result) const
{
    auto observed = state_.output;
    observed.result = result;
    if (operation_ == BENCH_NEXT) {
        observed.position = state_.reader.pos;
    } else if (operation_ == BENCH_APPEND) {
        observed.position = state_.writer.pos;
    }
    return observed;
}

cpp_timing::cpp_timing(bench_operation op, const bench_input &in, uint8_t *destination,
                       const bench_output &initial)
    : operation_(op), step_(nullptr), format_(core::from_c(in.format)), source_(core::from_c(in.source)),
      input_{in.data, in.available}, destination_{destination, in.available}, length_(in.length),
      output_(initial), view_(core::from_c(initial.view)), diagnostic_(core::from_c(initial.diagnostic)),
      details_(in.diagnostic ? &diagnostic_ : nullptr)
{
    switch (op) {
    case BENCH_FIELD_READ:
        step_ = [](cpp_timing &s) {
            return core::read_length(s.format_.length, {s.input_.data, s.input_.size},
                                     s.output_.length, s.output_.count);
        };
        break;
    case BENCH_FIELD_WRITE:
        step_ = [](cpp_timing &s) {
            return core::write_length(s.format_.length, s.length_, s.destination_, s.output_.count);
        };
        break;
    case BENCH_HEADER_SIZE:
        step_ = [](cpp_timing &s) { return core::header_size(s.format_, s.output_.count); };
        break;
    case BENCH_ELEMENT_SIZE:
        step_ = [](cpp_timing &s) { return core::element_size(s.format_, s.source_, s.output_.count); };
        break;
    case BENCH_READ:
        step_ = [](cpp_timing &s) {
            return opentlv::io::read(s.format_, s.input_, s.view_, s.output_.count, s.details_);
        };
        break;
    case BENCH_NEXT:
        step_ = [](cpp_timing &s) { return s.reader_.next(s.view_, s.details_); };
        break;
    case BENCH_WRITE:
        step_ = [](cpp_timing &s) {
            return opentlv::io::write(s.format_, s.destination_, s.source_, s.output_.count, s.details_);
        };
        break;
    case BENCH_APPEND:
        step_ = [](cpp_timing &s) { return s.writer_.write(s.source_, s.details_); };
        break;
    }
}

tlv_result_t cpp_timing::reset_cursor()
{
    if (operation_ == BENCH_NEXT) {
        return core::to_c(reader_.init(format_, input_));
    }
    if (operation_ == BENCH_APPEND) {
        return core::to_c(writer_.init(format_, destination_));
    }
    return TLV_OK;
}

bench_output cpp_timing::output(tlv_result_t result) const
{
    auto observed = output_;
    observed.result = result;
    observed.view = core::to_c(view_);
    observed.diagnostic = core::to_c(diagnostic_);
    if (operation_ == BENCH_NEXT) {
        observed.position = reader_.position();
    } else if (operation_ == BENCH_APPEND) {
        observed.position = writer_.position();
    }
    return observed;
}

template<class Timing>
static void run_fixture(bench_operation op, const bench_input &in, uint8_t *destination, bench_output &out)
{
    Timing prepared(op, in, destination, out);
    out.result = prepared.reset_cursor();
    if (out.result == TLV_OK) {
        out = prepared.output(prepared.run());
    }
}

} // namespace opentlv_bench

extern "C" void bench_run_c(bench_operation op, const bench_input *in, uint8_t *destination, bench_output *out)
{
    opentlv_bench::run_fixture<opentlv_bench::c_timing>(op, *in, destination, *out);
}

extern "C" void bench_run_cpp(bench_operation op, const bench_input *in, uint8_t *destination, bench_output *out)
{
    opentlv_bench::run_fixture<opentlv_bench::cpp_timing>(op, *in, destination, *out);
}
