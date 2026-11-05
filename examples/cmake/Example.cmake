# JSON metadata uses only CMake 3.16 facilities, also usable by standalone examples.
function(opentlv_example_json_list output)
    set(items)
    foreach(value IN LISTS ARGN)
        string(REPLACE "\\" "\\\\" value "${value}")
        string(REPLACE "\"" "\\\"" value "${value}")
        list(APPEND items "\"${value}\"")
    endforeach()
    string(JOIN "," items ${items})
    set(${output} "[${items}]" PARENT_SCOPE)
endfunction()

# Create one runnable example; callers own the sources and direct module links.
function(opentlv_example language module name)
    cmake_parse_arguments(EXAMPLE "QUERY_FRONTEND;UMBRELLA;NATIVE_INTEROP" "PASS_REGULAR_EXPRESSION"
        "SOURCES;LINK" ${ARGN})
    if(EXAMPLE_UNPARSED_ARGUMENTS OR EXAMPLE_KEYWORDS_MISSING_VALUES
       OR NOT EXAMPLE_SOURCES OR NOT EXAMPLE_LINK)
        message(FATAL_ERROR "Invalid opentlv_example(${language} ${module} ${name}) declaration")
    endif()
    if(NOT language MATCHES "^(c|cpp)$")
        message(FATAL_ERROR "Unknown example language: ${language}")
    endif()

    set(target "example-${language}-${module}-${name}")
    set(test "example.${language}.${module}.${name}")

    # Record declarations before skips so C-only/feature-off audits cover all sources.
    set(sources)
    foreach(source IN LISTS EXAMPLE_SOURCES)
        get_filename_component(source "${source}" ABSOLUTE)
        file(RELATIVE_PATH source "${OPENTLV_EXAMPLE_SOURCE_DIR}" "${source}")
        list(APPEND sources "${source}")
    endforeach()
    opentlv_example_json_list(sources ${sources})
    opentlv_example_json_list(links ${EXAMPLE_LINK})
    foreach(flag IN ITEMS UMBRELLA NATIVE_INTEROP QUERY_FRONTEND)
        if(EXAMPLE_${flag})
            set(${flag} true)
        else()
            set(${flag} false)
        endif()
    endforeach()
    file(APPEND "${OPENTLV_EXAMPLE_REGISTRY}"
        "{\"target\":\"${target}\",\"test\":\"${test}\",\"language\":\"${language}\",\"sources\":${sources},\"link\":${links},\"umbrella\":${UMBRELLA},\"native_interop\":${NATIVE_INTEROP},\"query_frontend\":${QUERY_FRONTEND}}\n")

    if(language STREQUAL "cpp" AND NOT OPENTLV_BUILD_CXX)
        message(STATUS "Skipping ${target}: OPENTLV_BUILD_CXX=OFF")
        return()
    endif()
    if(OPENTLV_EXAMPLES_STANDALONE)
        # Component names are the exported target suffixes, not a second registry.
        string(REPLACE "OpenTLV::" "" components "${EXAMPLE_LINK}")
        if(EXAMPLE_UMBRELLA)
            find_package(OpenTLV CONFIG REQUIRED OPTIONAL_COMPONENTS ${components})
            if(NOT OpenTLV_tlv_FOUND)
                message(STATUS "Skipping ${target}: optional OpenTLV::tlv component is unavailable")
                return()
            endif()
        else()
            find_package(OpenTLV CONFIG REQUIRED COMPONENTS ${components})
        endif()
        foreach(dependency IN LISTS EXAMPLE_LINK)
            if(NOT TARGET "${dependency}")
                message(FATAL_ERROR "${target}: missing expected exported target ${dependency}")
            endif()
        endforeach()
        if(EXAMPLE_QUERY_FRONTEND)
            include(CheckCSourceCompiles)
            # This probe only reads the public feature header. Linking the mixed
            # C/C++ facade into a C-only try_compile would require a C++ compiler.
            get_target_property(CMAKE_REQUIRED_INCLUDES OpenTLV::Query INTERFACE_INCLUDE_DIRECTORIES)
            check_c_source_compiles("#include <opentlv/query/features.h>
#ifndef OPENTLV_QUERY_FRONTEND
#error Missing Query feature contract
#endif
int main(void) { return 0; }" OPENTLV_EXAMPLES_QUERY_FEATURES)
            if(NOT OPENTLV_EXAMPLES_QUERY_FEATURES)
                message(FATAL_ERROR "Installed OpenTLV Query feature header is missing or invalid")
            endif()
            check_c_source_compiles("#include <opentlv/query/features.h>
#if !OPENTLV_QUERY_FRONTEND
#error Query frontend is disabled
#endif
int main(void) { return 0; }" OPENTLV_EXAMPLES_QUERY_FRONTEND)
            set(OPENTLV_QUERY_FRONTEND "${OPENTLV_EXAMPLES_QUERY_FRONTEND}")
        endif()
    endif()
    if(EXAMPLE_QUERY_FRONTEND AND NOT OPENTLV_QUERY_FRONTEND)
        message(STATUS "Skipping ${target}: OPENTLV_QUERY_FRONTEND=OFF")
        return()
    endif()
    add_executable(${target} ${EXAMPLE_SOURCES})
    target_link_libraries(${target} PRIVATE ${EXAMPLE_LINK})
    if(NOT OPENTLV_EXAMPLES_STANDALONE)
        opentlv_configure_compiler(${target})
        opentlv_copy_shared_runtime(${target})
    endif()
    add_test(NAME ${test} COMMAND $<TARGET_FILE:${target}>)
    set_tests_properties(${test} PROPERTIES LABELS "integration;examples")
    if(WIN32)
        # All OpenTLV DLLs share Core's runtime directory, in-tree and installed.
        # ENVIRONMENT and TARGET_FILE_DIR work with CMake 3.16 and multi-config.
        get_target_property(core_type OpenTLV::Core TYPE)
        if(core_type STREQUAL "SHARED_LIBRARY")
            string(REPLACE ";" "\\;" runtime_path "$<TARGET_FILE_DIR:OpenTLV::Core>;$ENV{PATH}")
            set_tests_properties(${test} PROPERTIES ENVIRONMENT "PATH=${runtime_path}")
        endif()
    endif()
    if(DEFINED EXAMPLE_PASS_REGULAR_EXPRESSION)
        set_tests_properties(${test} PROPERTIES
            PASS_REGULAR_EXPRESSION "${EXAMPLE_PASS_REGULAR_EXPRESSION}")
    endif()
endfunction()
