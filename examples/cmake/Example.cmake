# Create one runnable example; callers own the sources and direct module links.
function(opentlv_example language module name)
    cmake_parse_arguments(EXAMPLE "QUERY_FRONTEND" "PASS_REGULAR_EXPRESSION"
        "SOURCES;LINK;ARGS" ${ARGN})
    if(EXAMPLE_UNPARSED_ARGUMENTS OR EXAMPLE_KEYWORDS_MISSING_VALUES
       OR NOT EXAMPLE_SOURCES OR NOT EXAMPLE_LINK)
        message(FATAL_ERROR "Invalid opentlv_example(${language} ${module} ${name}) declaration")
    endif()
    if(NOT language MATCHES "^(c|cpp)$")
        message(FATAL_ERROR "Unknown example language: ${language}")
    endif()

    set(target "example-${language}-${module}-${name}")
    set(test "example.${language}.${module}.${name}")
    if(language STREQUAL "cpp" AND NOT OPENTLV_BUILD_CXX)
        message(STATUS "Skipping ${target}: OPENTLV_BUILD_CXX=OFF")
        return()
    endif()
    if(EXAMPLE_QUERY_FRONTEND AND NOT OPENTLV_QUERY_FRONTEND)
        message(STATUS "Skipping ${target}: OPENTLV_QUERY_FRONTEND=OFF")
        return()
    endif()

    add_executable(${target} ${EXAMPLE_SOURCES})
    target_link_libraries(${target} PRIVATE ${EXAMPLE_LINK})
    opentlv_configure_compiler(${target})
    opentlv_copy_shared_runtime(${target})
    add_test(NAME ${test} COMMAND $<TARGET_FILE:${target}> ${EXAMPLE_ARGS})
    set_tests_properties(${test} PROPERTIES LABELS "integration;examples")
    if(DEFINED EXAMPLE_PASS_REGULAR_EXPRESSION)
        set_tests_properties(${test} PROPERTIES
            PASS_REGULAR_EXPRESSION "${EXAMPLE_PASS_REGULAR_EXPRESSION}")
    endif()
endfunction()
