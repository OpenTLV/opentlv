cmake_minimum_required(VERSION 3.19)
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

include(FetchContent)
set(OPENTLV_GOOGLE_BENCHMARK_VERSION 1.9.4)
set(OPENTLV_GOOGLE_BENCHMARK_URL
    "https://github.com/google/benchmark/archive/refs/tags/v${OPENTLV_GOOGLE_BENCHMARK_VERSION}.tar.gz"
    CACHE STRING "URL or absolute path to the Google Benchmark 1.9.4 source archive"
)

# Keep the framework private and static even when measuring shared OpenTLV.
function(add_google_benchmark)
    string(REPLACE "\\" "/" archive "${OPENTLV_GOOGLE_BENCHMARK_URL}")
    FetchContent_Declare(googlebenchmark
        URL "${archive}"
        URL_HASH SHA256=b334658edd35efcf06a99d9be21e4e93e092bd5f95074c1673d5c8705d95c104
    )
    set(BUILD_SHARED_LIBS OFF)
    set(BENCHMARK_ENABLE_TESTING OFF)
    set(BENCHMARK_ENABLE_INSTALL OFF)
    set(BENCHMARK_ENABLE_WERROR OFF)
    FetchContent_MakeAvailable(googlebenchmark)
endfunction()
add_google_benchmark()

include(metadata.cmake)
add_library(opentlv_benchmark_workloads STATIC adapter.c adapter.cpp fixtures.cpp)
set_target_properties(opentlv_benchmark_workloads PROPERTIES
    C_STANDARD 99 C_STANDARD_REQUIRED YES C_EXTENSIONS NO
    CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO
)
target_link_libraries(opentlv_benchmark_workloads PUBLIC OpenTLV::io)
if(BUILD_TESTING AND OPENTLV_TEST_COVERAGE)
    # Consumers of coverage-instrumented static libraries need the gcov runtime.
    target_link_options(opentlv_benchmark_workloads INTERFACE --coverage)
endif()
add_executable(opentlv_check_fixtures check_fixtures.cpp)
target_link_libraries(opentlv_check_fixtures PRIVATE opentlv_benchmark_workloads)
add_executable(opentlv_benchmark google_benchmark.cpp)
target_link_libraries(opentlv_benchmark PRIVATE opentlv_benchmark_workloads benchmark::benchmark)
add_dependencies(opentlv_check_fixtures opentlv_benchmark_metadata)
add_dependencies(opentlv_benchmark opentlv_benchmark_metadata)
# Both executables must be beside the OpenTLV DLLs in Windows shared builds.
set_target_properties(opentlv_check_fixtures opentlv_benchmark PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "$<TARGET_FILE_DIR:opentlv_io>"
    CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO
)
add_test(NAME BenchmarkFixtures COMMAND opentlv_check_fixtures)
add_test(NAME BenchmarkRunner COMMAND opentlv_benchmark --benchmark_min_time=32x)
set_tests_properties(BenchmarkFixtures BenchmarkRunner PROPERTIES LABELS benchmark)
