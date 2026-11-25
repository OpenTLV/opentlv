# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Marek Cingel

find_package(Python3 REQUIRED COMPONENTS Interpreter)
get_filename_component(compiler_dir "${CMAKE_C_COMPILER}" DIRECTORY)
set(size_tools llvm-size gsize)
if(MSVC)
    list(PREPEND size_tools dumpbin)
endif()
if(_CMAKE_TOOLCHAIN_PREFIX)
    list(PREPEND size_tools "${_CMAKE_TOOLCHAIN_PREFIX}size")
endif()
if(CMAKE_HOST_SYSTEM_NAME MATCHES "^(Linux|Windows)$")
    list(APPEND size_tools size)
endif()
find_program(OPENTLV_SIZE_TOOL
    NAMES ${size_tools}
    HINTS "${compiler_dir}"
    DOC "GNU size, llvm-size or dumpbin used to report artifact sections")

add_executable(opentlv_footprint_c "${OpenTLV_SOURCE_DIR}/examples/write.c")
add_executable(opentlv_footprint_cpp "${OpenTLV_SOURCE_DIR}/examples/write.cpp")
foreach(consumer IN ITEMS opentlv_footprint_c opentlv_footprint_cpp)
    target_link_libraries(${consumer} PRIVATE OpenTLV::io)
    if(BUILD_TESTING AND OPENTLV_TEST_COVERAGE)
        target_link_options(${consumer} PRIVATE --coverage)
    endif()
    set_target_properties(${consumer} PROPERTIES
        C_STANDARD 99 C_STANDARD_REQUIRED YES C_EXTENSIONS NO
        CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO
        RUNTIME_OUTPUT_DIRECTORY "$<TARGET_FILE_DIR:opentlv_io>")
    add_test(NAME ${consumer} COMMAND ${consumer})
    set_tests_properties(${consumer} PROPERTIES LABELS footprint
        PASS_REGULAR_EXPRESSION "12 34 00 03 AA BB CC")
endforeach()

add_test(NAME FootprintSectionReports
    COMMAND "${Python3_EXECUTABLE}" -B -m unittest discover
        -s "${CMAKE_CURRENT_SOURCE_DIR}" -p test_report.py)
set_tests_properties(FootprintSectionReports PROPERTIES LABELS footprint)

set(report "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/footprint.json")
set(size_args)
if(OPENTLV_SIZE_TOOL)
    list(APPEND size_args --size-tool "${OPENTLV_SIZE_TOOL}")
endif()
add_custom_target(opentlv_footprint
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/report.py"
        --metadata "${CMAKE_CURRENT_BINARY_DIR}/../$<CONFIG>/metadata.json"
        --output "${report}" ${size_args}
        --artifact core "$<TARGET_FILE:opentlv_core>"
        --artifact io "$<TARGET_FILE:opentlv_io>"
        --artifact c99-write "$<TARGET_FILE:opentlv_footprint_c>"
        --artifact cpp17-write "$<TARGET_FILE:opentlv_footprint_cpp>"
    DEPENDS opentlv_benchmark_metadata opentlv_footprint_c opentlv_footprint_cpp
    COMMENT "Recording library and linked consumer sizes"
    VERBATIM)
