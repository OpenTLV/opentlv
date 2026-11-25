// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#include "fixtures.hpp"
#include "timing.hpp"

#include <valgrind/callgrind.h>

#include <cstdlib>
#include <iostream>
#include <map>
#include <set>
#include <string_view>

namespace {

using namespace opentlv_bench;

std::set<std::string> selected_ids()
{
    std::set<std::string> ids{
        "v1/header-size/be2/complete",
        "v1/header-size/be2/invalid",
        "v1/element-size/le8/payload513/complete",
        "v1/element-size/le8/payload513/invalid",
    };
    for (const std::string operation : {"v1/field-read", "v1/field-write"}) {
        for (const auto layout : {"/be3/offset0", "/le8/offset1"}) {
            ids.insert(operation + layout + "/complete");
            ids.insert(operation + layout + "/short");
        }
    }
    for (const std::string layout :
         {"v1/read/be1/offset0/payload0", "v1/next/le3/offset1/payload3",
          "v1/write/le8/offset1/payload513", "v1/append/be2/offset0/payload3"}) {
        for (const auto outcome : {"/complete", "/short"}) {
            for (const auto mode : {"/plain", "/diagnostic"}) {
                ids.insert(layout + outcome + mode);
            }
        }
    }
    return ids;
}

template<class Adapter>
std::string profile(const workload &entry, const fixture &data, const std::string &id)
{
    auto destination = fixture::initial_destination();
    Adapter adapter(entry.operation, data.input(), destination.bytes.data() + data.data_offset(),
                    fixture::initial_output());
    if (adapter.reset_cursor() != TLV_OK) {
        return "cursor initialization";
    }

    // The runner starts with collection disabled. Only the adapter dispatch,
    // nested calls and the client-request boundary instructions are counted.
    CALLGRIND_ZERO_STATS;
    CALLGRIND_TOGGLE_COLLECT;
    const auto result = adapter.run();
    CALLGRIND_TOGGLE_COLLECT;
    CALLGRIND_DUMP_STATS_AT(id.c_str());

    return data.validate(adapter.output(result), destination);
}

template<class Adapter>
bool check(const workload &entry, const fixture &data, const std::string &id,
           decltype(&bench_run_c) run, bool list)
{
    // Exercise the same code path before profiling so first-call runtime setup
    // is outside the interval. profile() uses fresh descriptors and storage.
    auto mismatch = data.validate(run);
    if (mismatch.empty() && !list) {
        mismatch = profile<Adapter>(entry, data, id);
    }
    if (!mismatch.empty()) {
        std::cerr << id << " failed: " << mismatch << '\n';
        return false;
    }
    return true;
}

} // namespace

int main(int argc, char **argv)
{
    const bool list = argc == 2 && std::string_view(argv[1]) == "--list";
    if (argc != 1 && !list) {
        std::cerr << "Usage: " << argv[0] << " [--list]\n";
        return EXIT_FAILURE;
    }
    if (!list && !RUNNING_ON_VALGRIND) {
        std::cerr << "Run the probe under Callgrind with --collect-atstart=no.\n";
        return EXIT_FAILURE;
    }

    const auto ids = selected_ids();
    constexpr size_t expected_inputs = 8 + 4 + 16; // Field, sizing and IO cases.
    if (ids.size() != expected_inputs) {
        std::cerr << "Expected " << expected_inputs << " v1 Callgrind inputs, found " << ids.size()
                  << '\n';
        return EXIT_FAILURE;
    }
    std::map<std::string, workload> selected;
    for (const auto &entry : workloads()) {
        const auto id = entry.id();
        if (ids.count(id) && !selected.emplace(id, entry).second) {
            std::cerr << "Duplicate catalog workload: " << id << '\n';
            return EXIT_FAILURE;
        }
    }
    for (const auto &id : ids) {
        const auto found = selected.find(id);
        if (found == selected.end()) {
            std::cerr << "Missing catalog workload: " << id << '\n';
            return EXIT_FAILURE;
        }
        const auto &entry = found->second;
        const fixture data(entry);
        if (!check<c_timing>(entry, data, "c99/" + id, bench_run_c, list) ||
            !check<cpp_timing>(entry, data, "cpp17/" + id, bench_run_cpp, list)) {
            return EXIT_FAILURE;
        }
    }
    if (list) {
        for (const auto language : {"c99/", "cpp17/"}) {
            for (const auto &id : ids) {
                std::cout << language << id << '\n';
            }
        }
    }
    return EXIT_SUCCESS;
}
