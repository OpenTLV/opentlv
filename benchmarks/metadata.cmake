# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Marek Cingel

find_package(Git REQUIRED)

# string(JSON SET) takes a JSON value, including its quotes and escapes.
function(benchmark_json_string object key value)
    string(REPLACE "\\" "\\\\" value "${value}")
    string(REPLACE "\"" "\\\"" value "${value}")
    foreach(code RANGE 1 31)
        string(ASCII ${code} control)
        string(HEX "${control}" digits)
        string(REPLACE "${control}" "\\u00${digits}" value "${value}")
    endforeach()
    string(JSON updated SET "${${object}}" "${key}" "\"${value}\"")
    set(${object} "${updated}" PARENT_SCOPE)
endfunction()

# Generate each configuration's inputs independently so literal compiler flags
# survive unchanged (including semicolons and generator-expression syntax).
function(benchmark_metadata_inputs config)
    set(metadata "{\"schema_version\": 1}")
    benchmark_json_string(metadata configuration "${config}")
    benchmark_json_string(metadata generator "${CMAKE_GENERATOR}")
    benchmark_json_string(metadata system "${CMAKE_SYSTEM_NAME}")
    benchmark_json_string(metadata processor "${CMAKE_SYSTEM_PROCESSOR}")
    benchmark_json_string(metadata generator_platform "${CMAKE_GENERATOR_PLATFORM}")
    benchmark_json_string(metadata osx_architectures "${CMAKE_OSX_ARCHITECTURES}")
    math(EXPR pointer_bits "8 * ${CMAKE_SIZEOF_VOID_P}")
    string(JSON metadata SET "${metadata}" pointer_bits ${pointer_bits})
    if(BUILD_SHARED_LIBS)
        set(linkage shared)
    else()
        set(linkage static)
    endif()
    benchmark_json_string(metadata library_linkage "${linkage}")
    if(MSVC)
        if(DEFINED CMAKE_MSVC_RUNTIME_LIBRARY)
            set(runtime "${CMAKE_MSVC_RUNTIME_LIBRARY}")
        else()
            # CMake's default selects the debug DLL runtime only in Debug.
            set(runtime "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
        endif()
        benchmark_json_string(metadata msvc_runtime_library "${runtime}")
    endif()

    set(coverage false)
    if(BUILD_TESTING AND OPENTLV_TEST_COVERAGE)
        set(coverage true)
    endif()
    string(JSON metadata SET "${metadata}" test_coverage ${coverage})

    string(TOUPPER "${config}" uppercase_config)
    set(ipo "${CMAKE_INTERPROCEDURAL_OPTIMIZATION}")
    if(DEFINED CMAKE_INTERPROCEDURAL_OPTIMIZATION_${uppercase_config})
        set(ipo "${CMAKE_INTERPROCEDURAL_OPTIMIZATION_${uppercase_config}}")
    endif()
    if(ipo)
        string(JSON metadata SET "${metadata}" interprocedural_optimization true)
    else()
        string(JSON metadata SET "${metadata}" interprocedural_optimization false)
    endif()

    set(tool_versions "{}")
    benchmark_json_string(tool_versions cmake "${CMAKE_VERSION}")
    benchmark_json_string(tool_versions git "${GIT_VERSION_STRING}")
    if(OPENTLV_BUILD_BENCHMARKS)
        benchmark_json_string(tool_versions google_benchmark "${OPENTLV_GOOGLE_BENCHMARK_VERSION}")
    endif()
    string(JSON metadata SET "${metadata}" tools "${tool_versions}")

    set(compilers "{}")
    foreach(lang IN ITEMS C CXX)
        set(compiler "{}")
        benchmark_json_string(compiler id "${CMAKE_${lang}_COMPILER_ID}")
        benchmark_json_string(compiler version "${CMAKE_${lang}_COMPILER_VERSION}")
        benchmark_json_string(compiler path "${CMAKE_${lang}_COMPILER}")
        benchmark_json_string(compiler target "${CMAKE_${lang}_COMPILER_TARGET}")
        benchmark_json_string(compiler architecture "${CMAKE_${lang}_COMPILER_ARCHITECTURE_ID}")
        benchmark_json_string(compiler flags "${CMAKE_${lang}_FLAGS}")
        benchmark_json_string(compiler configuration_flags "${CMAKE_${lang}_FLAGS_${uppercase_config}}")
        string(JSON compilers SET "${compilers}" "${lang}" "${compiler}")
    endforeach()
    string(JSON metadata SET "${metadata}" compilers "${compilers}")
    benchmark_json_string(metadata linker_flags "${CMAKE_EXE_LINKER_FLAGS}")
    benchmark_json_string(metadata configuration_linker_flags "${CMAKE_EXE_LINKER_FLAGS_${uppercase_config}}")
    file(CONFIGURE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/metadata-${config}.json"
        CONTENT "@metadata@\n" @ONLY)
endfunction()

get_property(multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(multi_config)
    foreach(config IN LISTS CMAKE_CONFIGURATION_TYPES)
        benchmark_metadata_inputs("${config}")
    endforeach()
else()
    benchmark_metadata_inputs("${CMAKE_BUILD_TYPE}")
endif()

# Read Git at build time. This also refreshes reports after a checkout or an
# edit which does not cause CMake to reconfigure.
add_custom_target(opentlv_benchmark_metadata ALL
    COMMAND "${CMAKE_COMMAND}"
        "-Dinput=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/metadata-$<CONFIG>.json"
        "-Doutput=${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/metadata.json"
        "-Dsource_dir=${OpenTLV_SOURCE_DIR}"
        "-Dbuild_dir=${CMAKE_BINARY_DIR}"
        "-Dgit=${GIT_EXECUTABLE}"
        -P "${CMAKE_CURRENT_LIST_DIR}/write_metadata.cmake"
    COMMENT "Recording benchmark build metadata"
    VERBATIM
)
