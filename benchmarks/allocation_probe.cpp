// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#include "fixtures.hpp"
#include "timing.hpp"

#include <cstdlib>
#include <iostream>
#include <new>

// The debugger marks an allocator entry only while a measured call is active.
extern "C" {
volatile int opentlv_allocation_active = 0;
volatile int opentlv_allocation_seen = 0;
}

namespace {
using namespace opentlv_bench;

template<class Call> bool allocated(Call call)
{
    opentlv_allocation_seen = 0;
    opentlv_allocation_active = 1;
    call();
    opentlv_allocation_active = 0;
    return opentlv_allocation_seen != 0;
}

bool check_controls()
{
    // Volatile function pointers preserve deliberate allocator calls at -O3.
    auto *volatile allocate = &std::malloc;
    auto *volatile zero_allocate = &std::calloc;
    auto *volatile resize = &std::realloc;
    using new_function = void *(*)(std::size_t);
    volatile new_function scalar_new = &::operator new;
    volatile new_function array_new = &::operator new[];
    using aligned_new_function = void *(*)(std::size_t, std::align_val_t);
    volatile aligned_new_function aligned_new = &::operator new;

    void *memory = allocate(8);
    const bool empty = allocated([] {});
    std::free(memory);
    const bool malloc_seen = allocated([&] { memory = allocate(8); });
    const bool realloc_seen = allocated([&] { memory = resize(memory, 16); });
    std::free(memory);
    const bool calloc_seen = allocated([&] { memory = zero_allocate(1, 8); });
    std::free(memory);
    const bool new_seen = allocated([&] { memory = scalar_new(8); });
    ::operator delete(memory);
    const bool array_seen = allocated([&] { memory = array_new(8); });
    ::operator delete[](memory);
    const auto alignment = std::align_val_t{64};
    const bool aligned_seen = allocated([&] { memory = aligned_new(64, alignment); });
    ::operator delete(memory, alignment);
    if (empty || !malloc_seen || !calloc_seen || !realloc_seen || !new_seen || !array_seen ||
        !aligned_seen) {
        std::cerr
            << "Allocation controls failed (empty/malloc/calloc/realloc/new/new[]/aligned new): "
            << empty << '/' << malloc_seen << '/' << calloc_seen << '/' << realloc_seen << '/'
            << new_seen << '/' << array_seen << '/' << aligned_seen << '\n';
        return false;
    }
    return true;
}

bool check_core_views()
{
    namespace core = opentlv::core;
    const uint8_t byte = 0xaa;
    for (bool valid : {true, false}) {
        const uint8_t *data = valid ? &byte : nullptr;
        const auto expected = valid ? TLV_OK : TLV_ERR_NULL_ARG;
        const auto category = valid ? TLV_CATEGORY_SUCCESS : TLV_CATEGORY_INVALID_ARGUMENT;
        tlv_value_t native{};
        core::value_view typed{};
        tlv_result_category_t c_category;
        core::category cpp_category;
        tlv_result_t c_result;
        core::result cpp_result;
        const bool heap = allocated([&] {
            c_category = tlv_result_category(expected);
            cpp_category = core::result_category(core::from_c(expected));
            c_result = tlv_value_init(data, 1, &native);
            cpp_result = core::make_value_view(data, 1, typed);
        });
        if (heap || c_category != category || core::to_c(cpp_category) != category ||
            c_result != expected || core::to_c(cpp_result) != expected || native.data != data ||
            typed.data != data || native.size != (valid ? 1u : 0u) || typed.size != native.size) {
            std::cerr << "Core result classification or value view failed (valid=" << valid
                      << ", heap allocation=" << heap << ")\n";
            return false;
        }
    }
    return true;
}

template<class Adapter> bool check(const workload &entry, const fixture &data, const char *language)
{
    auto destination = fixture::initial_destination();
    Adapter adapter(entry.operation, data.input(), destination.bytes.data() + data.data_offset(),
                    fixture::initial_output());
    tlv_result_t initialized;
    tlv_result_t result = TLV_END;
    const bool heap = allocated([&] {
        initialized = adapter.reset_cursor();
        if (initialized == TLV_OK) {
            result = adapter.run();
        }
    });
    if (initialized != TLV_OK) {
        std::cerr << language << '/' << entry.id() << " failed: cursor initialization\n";
        return false;
    }
    const auto mismatch = data.validate(adapter.output(result), destination);
    if (heap || !mismatch.empty()) {
        std::cerr << language << '/' << entry.id()
                  << " failed: " << (heap ? "heap allocation" : mismatch) << '\n';
        return false;
    }
    return true;
}
} // namespace

int main()
{
    if (!check_controls() || !check_core_views()) {
        return EXIT_FAILURE;
    }
    const auto cases = workloads();
    if (cases.size() != 1792) {
        std::cerr << "Expected 1792 v1 workloads, found " << cases.size() << '\n';
        return EXIT_FAILURE;
    }
    for (const auto &entry : cases) {
        const fixture data(entry);
        if (!check<c_timing>(entry, data, "c99") || !check<cpp_timing>(entry, data, "cpp17")) {
            return EXIT_FAILURE;
        }
    }
    std::cout << "Zero observed heap allocations in Core result/value checks and "
              << 2 * cases.size() << " C99 and C++17 workloads. All allocation controls passed.\n";
    return EXIT_SUCCESS;
}
