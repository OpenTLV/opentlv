// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#include "fixtures.hpp"
#include "timing.hpp"

#include <benchmark/benchmark.h>

#include <array>
#include <cstdlib>
#include <optional>
#include <string>

namespace {

using namespace opentlv_bench;

// Amortize the timer switches needed to rewind cursors. Google Benchmark
// counts the operations inside a batch as individual iterations.
constexpr size_t batch_size = 32;

template <typename Adapter>
void measure(benchmark::State &state, const workload &entry, decltype(&bench_run_c) check,
             bool &failed)
{
    const fixture data(entry);
    const auto preflight = data.validate(check);
    if (!preflight.empty()) {
        state.SkipWithError("fixture check failed: " + preflight);
        failed = true;
        return;
    }

    const auto input = data.input();
    std::array<wire_buffer, batch_size> destinations;
    std::array<std::optional<Adapter>, batch_size> adapters;
    std::array<tlv_result_t, batch_size> results{};
    for (size_t i = 0; i < batch_size; ++i) {
        destinations[i] = fixture::initial_destination();
        auto *wire = destinations[i].bytes.data() + data.data_offset();
        benchmark::DoNotOptimize(wire);
        adapters[i].emplace(entry.operation, input, wire, fixture::initial_output());
    }

    const bool cursor = entry.operation == BENCH_NEXT || entry.operation == BENCH_APPEND;
    while (state.KeepRunningBatch(batch_size)) {
        if (cursor) {
            state.PauseTiming();
            for (auto &adapter : adapters) {
                if (adapter->reset_cursor() != TLV_OK) {
                    state.SkipWithError("cursor initialization failed");
                    failed = true;
                    return;
                }
            }
            state.ResumeTiming();
        }
        for (size_t i = 0; i < batch_size; ++i) {
            results[i] = adapters[i]->run();
            benchmark::DoNotOptimize(results[i]);
            benchmark::ClobberMemory();
        }
    }

    for (size_t i = 0; i < batch_size; ++i) {
        const auto mismatch = data.validate(adapters[i]->output(results[i]), destinations[i]);
        if (!mismatch.empty()) {
            state.SkipWithError("timed output failed: " + mismatch);
            failed = true;
            return;
        }
    }
    const bool size_only = entry.operation == BENCH_HEADER_SIZE || entry.operation == BENCH_ELEMENT_SIZE;
    const auto bytes = entry.complete && !size_only ? input.available : 0;
    state.counters["bytes_per_operation"] = static_cast<double>(bytes);
    state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(bytes));
    state.SetItemsProcessed(state.iterations());
}

} // namespace

int main(int argc, char **argv)
{
    benchmark::Initialize(&argc, argv);
    if (benchmark::ReportUnrecognizedArguments(argc, argv)) {
        return EXIT_FAILURE;
    }
    bool failed = false;
    for (const auto &entry : opentlv_bench::workloads()) {
        const auto id = entry.id();
        benchmark::RegisterBenchmark("c99/" + id, [entry, &failed](benchmark::State &state) {
            measure<opentlv_bench::c_timing>(state, entry, bench_run_c, failed);
        })->Unit(benchmark::kNanosecond)->UseRealTime();
        benchmark::RegisterBenchmark("cpp17/" + id, [entry, &failed](benchmark::State &state) {
            measure<opentlv_bench::cpp_timing>(state, entry, bench_run_cpp, failed);
        })->Unit(benchmark::kNanosecond)->UseRealTime();
    }
    const auto matched = benchmark::RunSpecifiedBenchmarks();
    benchmark::Shutdown();
    return matched && !failed ? EXIT_SUCCESS : EXIT_FAILURE;
}
