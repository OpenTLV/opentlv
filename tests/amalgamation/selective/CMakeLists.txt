# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Marek Cingel

# Builds the registered module consumers of a selective amalgamation against
# its opentlv.c, compiled as strict C99, and its include/ tree. The consumers
# are the sources the module isolation gate links to the conventional modules.
# C_CONSUMERS and CXX_CONSUMERS are '|'-separated source lists.
cmake_minimum_required(VERSION 3.16)
project(opentlv_amalgamation_selective LANGUAGES C)

if(NOT OPENTLV_AMALGAMATION_DIR OR NOT C_CONSUMERS)
    message(FATAL_ERROR "OPENTLV_AMALGAMATION_DIR and C_CONSUMERS are required")
endif()
string(REPLACE "|" ";" C_CONSUMERS "${C_CONSUMERS}")
string(REPLACE "|" ";" CXX_CONSUMERS "${CXX_CONSUMERS}")
if(CXX_CONSUMERS)
    enable_language(CXX)
endif()

add_library(opentlv STATIC "${OPENTLV_AMALGAMATION_DIR}/opentlv.c")
set(targets opentlv)
set(names)
foreach(source IN LISTS C_CONSUMERS CXX_CONSUMERS)
    get_filename_component(stem "${source}" NAME_WE)
    get_filename_component(extension "${source}" EXT)
    string(REPLACE "." "" extension "${extension}")
    set(name "consumer-${stem}-${extension}")
    add_executable(${name} "${source}")
    target_include_directories(${name} PRIVATE "${OPENTLV_AMALGAMATION_DIR}/include")
    target_link_libraries(${name} PRIVATE opentlv)
    list(APPEND targets ${name})
    list(APPEND names ${name})
endforeach()
file(WRITE "${CMAKE_BINARY_DIR}/consumers.txt" "${names}")
foreach(target IN LISTS targets)
    set_target_properties(${target} PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF
        CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin$<0:>")
    # The Library's warning policy, with strict ISO C, as tests/amalgamation/consumer.
    target_compile_options(${target} PRIVATE
        "$<$<COMPILE_LANG_AND_ID:C,GNU,Clang,AppleClang>:-Wshadow;-Werror=shadow>"
        "$<$<COMPILE_LANG_AND_ID:C,GNU>:-pedantic-errors;-Werror>"
        "$<$<COMPILE_LANG_AND_ID:C,Clang,AppleClang>:-pedantic-errors;-Wall;-Wextra;-Wpedantic;-Wstrict-prototypes;-Werror>"
        "$<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>:-pedantic-errors;-Werror>"
        "$<$<C_COMPILER_ID:MSVC>:/WX>")
endforeach()
