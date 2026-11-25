# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Marek Cingel

cmake_minimum_required(VERSION 3.19)

execute_process(COMMAND "${git}" rev-parse --verify HEAD
    WORKING_DIRECTORY "${source_dir}"
    RESULT_VARIABLE result OUTPUT_VARIABLE revision OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT result EQUAL 0 OR NOT revision MATCHES "^[0-9a-f]+$")
    message(FATAL_ERROR "Benchmark metadata requires the Git revision of the OpenTLV source tree.")
endif()

# CMake build outputs can live inside the source checkout. Exclude this build
# tree while still detecting new, untracked sources alongside tracked edits.
file(RELATIVE_PATH build_relative "${source_dir}" "${build_dir}")
set(paths .)
if(NOT build_relative STREQUAL "" AND NOT IS_ABSOLUTE "${build_relative}"
        AND NOT build_relative MATCHES "^\\.\\.(/|$)")
    list(APPEND paths ":(exclude,literal)${build_relative}")
endif()
execute_process(COMMAND "${git}" --no-optional-locks status --porcelain=v1 --untracked-files=normal -- ${paths}
    WORKING_DIRECTORY "${source_dir}"
    RESULT_VARIABLE result OUTPUT_VARIABLE changes OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Unable to inspect the benchmark source tree.")
endif()
if(changes STREQUAL "")
    set(dirty false)
else()
    set(dirty true)
endif()

file(READ "${input}" metadata)
string(JSON metadata SET "${metadata}" revision "\"${revision}\"")
string(JSON metadata SET "${metadata}" dirty ${dirty})
file(CONFIGURE OUTPUT "${output}" CONTENT "@metadata@\n" @ONLY)
