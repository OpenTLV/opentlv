cmake_minimum_required(VERSION 3.16)

project(tests LANGUAGES C)

foreach(module IN ITEMS asn1 emv bluetooth nfc dhcp lldp codec query-codec schema schema-number schema-query schema-query-document codec-structure)
    string(REPLACE "-" "_" source "${module}")
    opentlv_add_module_test(module-${module} OWNER ${module} SOURCES modules/${source}.c)
    opentlv_configure_compiler(module-${module})
    opentlv_copy_shared_runtime(module-${module})
endforeach()

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
    get_property(modules GLOBAL PROPERTY OPENTLV_MODULES)
    foreach(module IN LISTS modules)
        get_property(target GLOBAL PROPERTY OPENTLV_MODULE_${module})
        get_target_property(consumer ${target} INTERFACE_OPENTLV_CXX_CONSUMER)
        get_target_property(facade ${target} INTERFACE_OPENTLV_FACADE)
        if(facade)
            get_target_property(consumer ${target} INTERFACE_OPENTLV_CONSUMER)
        endif()
        if(consumer)
            opentlv_add_module_test(module-${module}-cxx OWNER ${module} SOURCES ${consumer})
            opentlv_configure_compiler(module-${module}-cxx)
            opentlv_copy_shared_runtime(module-${module}-cxx)
        endif()
    endforeach()
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

# C source amalgamation (#740). The generator needs no compiler or Python. The
# integration check generates this build's amalgamation below the test tree,
# never <build>/amalgamation/, and compiles it as C99 in a separate project,
# with representative selective compositions (#770).
if(_OPENTLV_TOP_LEVEL AND OPENTLV_BUILD_UNIT_TESTS)
    add_test(NAME amalgamation-generator
        COMMAND "${CMAKE_COMMAND}" "-DOPENTLV_SOURCE_DIR=${OpenTLV_SOURCE_DIR}"
            "-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/amalgamation-generator"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/amalgamation/generator_checks.cmake")
    set_tests_properties(amalgamation-generator PROPERTIES LABELS "unit;amalgamation;no-memcheck")
endif()
if(_OPENTLV_TOP_LEVEL AND OPENTLV_BUILD_INTEGRATION_TESTS)
    # Selective compositions also build the registered C++ consumers when the
    # build has a C++ compiler.
    set(_amalgamation_cxx)
    if(CMAKE_CXX_COMPILER_LOADED)
        set(_amalgamation_cxx "-DCXX_COMPILER=${CMAKE_CXX_COMPILER}")
    endif()
    add_test(NAME amalgamation-c99
        COMMAND "${CMAKE_COMMAND}" "-DOPENTLV_SOURCE_DIR=${OpenTLV_SOURCE_DIR}"
            "-DOPENTLV_BINARY_DIR=${OpenTLV_BINARY_DIR}"
            "-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/amalgamation-c99"
            "-DC_COMPILER=${CMAKE_C_COMPILER}" ${_amalgamation_cxx} "-DGENERATOR=${CMAKE_GENERATOR}"
            "-DGENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
            "-DGENERATOR_TOOLSET=${CMAKE_GENERATOR_TOOLSET}"
            "-DMAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}" "-DCONFIG=$<CONFIG>"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/amalgamation/c99_checks.cmake")
    set_tests_properties(amalgamation-c99 PROPERTIES LABELS "integration;amalgamation;no-memcheck")
endif()

# Symbol isolation (#763): historical unprefixed names in the consumer must not
# collide with the static archives or interpose the shared modules.
add_executable(symbol-collision amalgamation/collision.c)
target_link_libraries(symbol-collision PRIVATE OpenTLV::Core OpenTLV::Document OpenTLV::Query
    OpenTLV::QueryDocument OpenTLV::Codec OpenTLV::EMV)
add_test(NAME symbol-collision COMMAND symbol-collision)
set_tests_properties(symbol-collision PROPERTIES LABELS "integration;symbols")

add_executable(capability-contract capabilities.c)
target_link_libraries(capability-contract PRIVATE OpenTLV::Core OpenTLV::IO OpenTLV::Query OpenTLV::QueryDocument OpenTLV::Document OpenTLV::Schema OpenTLV::Codec)
add_test(NAME capability-contract COMMAND capability-contract)
set_tests_properties(capability-contract PROPERTIES LABELS "capabilities")
add_executable(diagnostic-strings-contract diagnostic_strings.c)
target_link_libraries(diagnostic-strings-contract PRIVATE OpenTLV::Core OpenTLV::IO OpenTLV::Query OpenTLV::Schema OpenTLV::Codec OpenTLV::ASN1)
opentlv_configure_compiler(diagnostic-strings-contract)
add_test(NAME diagnostic-strings-contract COMMAND diagnostic-strings-contract)
set_tests_properties(diagnostic-strings-contract PROPERTIES LABELS "capabilities;diagnostics")
if(OPENTLV_BUILD_CXX)
    add_executable(capability-cxx-contract capabilities.cpp)
    target_link_libraries(capability-cxx-contract PRIVATE OpenTLV::Core OpenTLV::IO OpenTLV::Query OpenTLV::QueryCodec OpenTLV::Codec OpenTLV::EMV)
    add_test(NAME capability-cxx-contract COMMAND capability-cxx-contract)
    set_tests_properties(capability-cxx-contract PROPERTIES LABELS "capabilities")
endif()
opentlv_copy_shared_runtime(capability-contract)
opentlv_copy_shared_runtime(diagnostic-strings-contract)


if(OPENTLV_BUILD_CXX AND (OPENTLV_BUILD_UNIT_TESTS OR OPENTLV_BUILD_INTEGRATION_TESTS))
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

if(OPENTLV_BUILD_CXX AND OPENTLV_BUILD_PROPERTY_TESTS)
    add_subdirectory(property)
endif()
