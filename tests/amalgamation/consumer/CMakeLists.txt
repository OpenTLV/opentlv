# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Marek Cingel

# Compiles the generated opentlv.c and a consumer as strict C99. CMake only
# drives the compiler here: the target has no OpenTLV include directory,
# library or compile definition beyond the amalgamation directory.
cmake_minimum_required(VERSION 3.16)
project(opentlv_amalgamation_consumer LANGUAGES C)

if(NOT OPENTLV_AMALGAMATION_DIR)
    message(FATAL_ERROR "OPENTLV_AMALGAMATION_DIR is required")
endif()
add_executable(consumer consumer.c "${OPENTLV_AMALGAMATION_DIR}/opentlv.c")
# The symbol isolation regression links opentlv.c as one object beside the
# historical colliding names (#763).
add_executable(collision ../collision.c "${OPENTLV_AMALGAMATION_DIR}/opentlv.c")
target_compile_definitions(collision PRIVATE OPENTLV_COLLISION_AMALGAMATION)
foreach(target IN ITEMS consumer collision)
    target_include_directories(${target} PRIVATE "${OPENTLV_AMALGAMATION_DIR}")
    set_target_properties(${target} PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin$<0:>")
    # The Library's warning policy (cmake/compiler.cmake), with strict ISO C.
    target_compile_options(${target} PRIVATE
        "$<$<C_COMPILER_ID:GNU>:-pedantic-errors;-Werror>"
        "$<$<C_COMPILER_ID:Clang,AppleClang>:-pedantic-errors;-Wall;-Wextra;-Wpedantic;-Wstrict-prototypes;-Werror>"
        "$<$<C_COMPILER_ID:MSVC>:/WX>")
endforeach()
