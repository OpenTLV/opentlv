// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#include "fixtures.hpp"

#include <cstdlib>
#include <iostream>
#include <set>
#include <string_view>

int main(int argc, char **argv)
{
    const bool list = argc == 2 && std::string_view(argv[1]) == "--list";
    if (argc != 1 && !list) {
        std::cerr << "Usage: " << argv[0] << " [--list]\n";
        return EXIT_FAILURE;
    }
    const struct {
        const char *language;
        decltype(&bench_run_c) run;
    } adapters[] = {{"c99", bench_run_c}, {"cpp17", bench_run_cpp}};

    const auto cases = opentlv_bench::workloads();
    // Each v1 layout has 16 Core and 96 IO workloads before language pairing.
    constexpr size_t v1_size = 16 * (16 + 96);
    if (cases.size() != v1_size) {
        std::cerr << "Expected " << v1_size << " v1 workloads, found " << cases.size() << '\n';
        return EXIT_FAILURE;
    }
    std::set<std::string> ids;
    for (const auto &entry : cases) {
        const opentlv_bench::fixture fixture(entry);
        for (const auto &adapter : adapters) {
            const auto id = std::string(adapter.language) + "/" + entry.id();
            if (!ids.insert(id).second) {
                std::cerr << id << " is duplicated\n";
                return EXIT_FAILURE;
            }
            const auto mismatch = fixture.validate(adapter.run);
            if (!mismatch.empty()) {
                std::cerr << id << " failed: " << mismatch << '\n';
                return EXIT_FAILURE;
            }
        }
    }
    if (list) {
        for (const auto &id : ids) {
            std::cout << id << '\n';
        }
    } else {
        std::cout << "Validated " << ids.size() << " C99 and C++17 workloads\n";
    }
    return EXIT_SUCCESS;
}
