# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Marek Cingel

cmake_minimum_required(VERSION 3.16)
project(amalgamation_comparison LANGUAGES C CXX)
find_package(OpenTLV REQUIRED CONFIG)

# Compile exactly the installed samples against conventional module headers.
# This test-only adapter supplies the flat header name used by the C sample.
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/opentlv.h"
    "#include <opentlv/tlv.h>\n#include <opentlv/core/version.h>\n#include <opentlv/query/features.h>\n")
foreach(language IN ITEMS c cpp)
    add_executable(consumer-${language} "${SAMPLES}/consumer.${language}")
    target_include_directories(consumer-${language} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
    target_link_libraries(consumer-${language} PRIVATE OpenTLV::tlv)
    set_target_properties(consumer-${language} PROPERTIES
        C_STANDARD 99 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF
        CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin$<0:>")
endforeach()
