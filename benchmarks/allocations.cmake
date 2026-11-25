# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Marek Cingel

if(CMAKE_CROSSCOMPILING OR NOT (CMAKE_SYSTEM_NAME STREQUAL "Linux" OR MINGW)
   OR NOT CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang)$")
    message(FATAL_ERROR "OPENTLV_BUILD_ALLOCATION_CHECKS requires native GNU or Clang on Linux or MinGW.")
endif()
find_program(OPENTLV_GDB gdb REQUIRED)

add_executable(opentlv_allocation_probe allocation_probe.cpp)
target_link_libraries(opentlv_allocation_probe PRIVATE opentlv_benchmark_workloads)
# The debugger reads the probe's window and observation flags, also in Release.
target_compile_options(opentlv_allocation_probe PRIVATE -g)
set_target_properties(opentlv_allocation_probe PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "$<TARGET_FILE_DIR:opentlv_io>"
    CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO
)
add_dependencies(opentlv_allocation_probe opentlv_benchmark_metadata)
add_test(NAME FixedTLVAllocations COMMAND "${OPENTLV_GDB}" -nx -batch
    -x "${CMAKE_CURRENT_LIST_DIR}/check_allocations.gdb" "$<TARGET_FILE:opentlv_allocation_probe>")
set_tests_properties(FixedTLVAllocations PROPERTIES LABELS benchmark)
