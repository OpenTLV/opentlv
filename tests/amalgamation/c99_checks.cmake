# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Marek Cingel

# Generates the amalgamation of this build's registry twice, requires identical
# bytes, then builds and runs a consumer that compiles opentlv.c as C99.
# Outputs stay below WORK_DIR; <build>/amalgamation/ is left untouched.
cmake_minimum_required(VERSION 3.16)

foreach(variable IN ITEMS OPENTLV_SOURCE_DIR OPENTLV_BINARY_DIR WORK_DIR C_COMPILER GENERATOR)
    if(NOT ${variable})
        message(FATAL_ERROR "${variable} is required")
    endif()
endforeach()
if(NOT CONFIG)
    set(CONFIG Debug)
endif()
file(REMOVE_RECURSE "${WORK_DIR}")

function(run)
    execute_process(COMMAND ${ARGN} RESULT_VARIABLE code OUTPUT_VARIABLE output ERROR_VARIABLE errors)
    if(NOT code EQUAL 0)
        string(REPLACE ";" " " command "${ARGN}")
        message(FATAL_ERROR "${command} failed (${code}):\n${output}${errors}")
    endif()
    message(STATUS "${output}")
endfunction()

foreach(name IN ITEMS first second)
    run("${CMAKE_COMMAND}" "-DOPENTLV_AMALGAMATION_CONFIG=${OPENTLV_BINARY_DIR}/opentlv-amalgamation.cmake"
        "-DOPENTLV_AMALGAMATION_OUTPUT=${WORK_DIR}/${name}"
        -P "${OPENTLV_SOURCE_DIR}/cmake/amalgamation/generate.cmake")
    file(GLOB_RECURSE files_${name} LIST_DIRECTORIES false RELATIVE "${WORK_DIR}/${name}" "${WORK_DIR}/${name}/*")
    list(SORT files_${name})
endforeach()
if(NOT files_first STREQUAL files_second)
    message(FATAL_ERROR "Two generations wrote different files")
endif()
foreach(path IN LISTS files_first)
    file(SHA256 "${WORK_DIR}/first/${path}" first)
    file(SHA256 "${WORK_DIR}/second/${path}" second)
    if(NOT first STREQUAL second)
        message(FATAL_ERROR "Two generations wrote different bytes to ${path}")
    endif()
endforeach()
foreach(path IN ITEMS opentlv.h opentlv.c opentlv-amalgamation.json include/opentlv/core/export.h
        include/opentlv/core/version.h include/opentlv/query/features.h include/opentlv/tlv.h)
    if(NOT path IN_LIST files_first)
        message(FATAL_ERROR "The amalgamation lacks ${path}")
    endif()
endforeach()

set(options -G "${GENERATOR}" "-DCMAKE_C_COMPILER=${C_COMPILER}" "-DCMAKE_BUILD_TYPE=${CONFIG}"
    "-DOPENTLV_AMALGAMATION_DIR=${WORK_DIR}/first")
if(GENERATOR_PLATFORM)
    list(APPEND options -A "${GENERATOR_PLATFORM}")
endif()
if(GENERATOR_TOOLSET)
    list(APPEND options -T "${GENERATOR_TOOLSET}")
endif()
if(MAKE_PROGRAM AND GENERATOR MATCHES "Ninja|Makefiles")
    list(APPEND options "-DCMAKE_MAKE_PROGRAM=${MAKE_PROGRAM}")
endif()
run("${CMAKE_COMMAND}" -S "${CMAKE_CURRENT_LIST_DIR}/consumer" -B "${WORK_DIR}/consumer" ${options})
run("${CMAKE_COMMAND}" --build "${WORK_DIR}/consumer" --config "${CONFIG}")
run("${WORK_DIR}/consumer/bin/consumer")
run("${WORK_DIR}/consumer/bin/collision")
