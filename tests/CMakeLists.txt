cmake_minimum_required(VERSION 3.16)

project(tests LANGUAGES C)

opentlv_add_module_test(module-query OWNER query SOURCES modules/query.c)
opentlv_add_module_test(module-query-document OWNER query-document SOURCES modules/query_document.c)
opentlv_configure_compiler(module-query)
opentlv_configure_compiler(module-query-document)
opentlv_copy_shared_runtime(module-query)
opentlv_copy_shared_runtime(module-query-document)

opentlv_add_module_test(module-core OWNER core SOURCES modules/core.c)
opentlv_add_module_test(module-io OWNER io SOURCES modules/io.c)
opentlv_add_module_test(module-document OWNER document SOURCES modules/document.c)
opentlv_add_module_test(module-generator OWNER generator SOURCES modules/generator.c)
opentlv_configure_compiler(module-generator)
opentlv_copy_shared_runtime(module-generator)
if(OPENTLV_BUILD_CXX)
    opentlv_add_module_test(module-generator-cxx OWNER generator SOURCES modules/generator.cpp)
    target_include_directories(module-generator-cxx PRIVATE ${OpenTLV_SOURCE_DIR}/tlv++/include)
    target_compile_features(module-generator-cxx PRIVATE cxx_std_11)
    opentlv_configure_compiler(module-generator-cxx)
    opentlv_copy_shared_runtime(module-generator-cxx)
endif()
opentlv_configure_compiler(module-core)
opentlv_configure_compiler(module-io)
opentlv_configure_compiler(module-document)
opentlv_copy_shared_runtime(module-core)
opentlv_copy_shared_runtime(module-io)
opentlv_copy_shared_runtime(module-document)

if(OPENTLV_BUILD_MEMCHECK)
    add_test(NAME memcheck-launcher
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/memcheck_launcher_test.py")
    set_tests_properties(memcheck-launcher PROPERTIES LABELS "infrastructure;no-memcheck")
endif()

find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    add_test(NAME diagnostic-vocabulary-generator
        COMMAND "${Python3_EXECUTABLE}" -B
                "${OpenTLV_SOURCE_DIR}/scripts/test_diagnostic_vocabulary.py")
    # Canonical C diagnostic enums must have names for standalone vocabularies.
    add_test(NAME diagnostic-vocabulary-generated
        COMMAND "${Python3_EXECUTABLE}"
            "${OpenTLV_SOURCE_DIR}/scripts/generate_diagnostic_vocabulary.py" --check)
    set_tests_properties(diagnostic-vocabulary-generated diagnostic-vocabulary-generator PROPERTIES
        LABELS "infrastructure;bindings;no-memcheck")
endif()

add_executable(capability-contract capabilities.c)
get_target_property(_capability_sources tlv SOURCES)
file(GENERATE OUTPUT "${OpenTLV_BINARY_DIR}/capability-sources.txt"
    CONTENT "${_capability_sources}")
target_link_libraries(capability-contract PRIVATE tlv)
add_test(NAME capability-contract COMMAND capability-contract)
set_tests_properties(capability-contract PROPERTIES LABELS "capabilities")
add_executable(diagnostic-strings-contract diagnostic_strings.c)
target_link_libraries(diagnostic-strings-contract PRIVATE tlv)
opentlv_configure_compiler(diagnostic-strings-contract)
add_test(NAME diagnostic-strings-contract COMMAND diagnostic-strings-contract)
set_tests_properties(diagnostic-strings-contract PROPERTIES LABELS "capabilities;diagnostics")
if(OPENTLV_BUILD_CXX)
    add_executable(capability-cxx-contract capabilities.cpp)
    target_link_libraries(capability-cxx-contract PRIVATE tlv++)
    add_test(NAME capability-cxx-contract COMMAND capability-cxx-contract)
    set_tests_properties(capability-cxx-contract PROPERTIES LABELS "capabilities")
endif()
if(OPENTLV_BUILD_SHARED_LIBS)
    foreach(contract IN ITEMS capability-contract diagnostic-strings-contract)
        add_custom_command(TARGET ${contract} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:tlv> $<TARGET_FILE:opentlv_core> $<TARGET_FILE:opentlv_io> $<TARGET_FILE:opentlv_document> $<TARGET_FILE:opentlv_generator> $<TARGET_FILE:opentlv_query> $<TARGET_FILE:opentlv_query_document> $<TARGET_FILE_DIR:${contract}>)
    endforeach()
endif()

if(OPENTLV_BUILD_UNIT_TESTS OR OPENTLV_BUILD_INTEGRATION_TESTS)
    enable_language(CXX)
    include(tag_layout_checks.cmake)
    include(field_encoding_checks.cmake)
    include(fixed_format_checks.cmake)
    include(custom_format_checks.cmake)
    include(typed_field_checks.cmake)
    include(cxx_contract_checks.cmake)
    include(FetchContent)

    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.18.0
    )

    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    foreach(test_group IN ITEMS unit integration)
        string(TOUPPER "${test_group}" group_option)
        if(OPENTLV_BUILD_${group_option}_TESTS)
            add_subdirectory(${test_group})
        endif()
    endforeach()
endif()

if(OPENTLV_BUILD_QUERY_TESTS OR OPENTLV_BUILD_UNIT_TESTS OR OPENTLV_BUILD_INTEGRATION_TESTS OR OPENTLV_BUILD_PROPERTY_TESTS)
    add_subdirectory(query)
endif()

if(OPENTLV_BUILD_INTEGRATION_TESTS)
    add_test(NAME architecture-pipeline
        COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_SOURCE_DIR}/pipeline_checks.cmake)
    set_tests_properties(architecture-pipeline PROPERTIES LABELS "integration;architecture")
endif()

if(OPENTLV_BUILD_PROPERTY_TESTS)
    add_subdirectory(property)
endif()
