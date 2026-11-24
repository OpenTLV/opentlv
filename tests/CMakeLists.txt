# PRE_TEST discovers cases for the configuration selected by CTest.
cmake_minimum_required(VERSION 3.18)
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()
include(FetchContent)
include(GoogleTest)

set(OPENTLV_GOOGLETEST_URL
    "https://github.com/google/googletest/archive/refs/tags/v1.17.0.tar.gz"
    CACHE STRING "URL or absolute path to the GoogleTest 1.17.0 source archive"
)
FetchContent_Declare(googletest
    URL "${OPENTLV_GOOGLETEST_URL}"
    URL_HASH SHA256=65fab701d9829d38cb77c14acdc431d2108bfdbf8979e40eb8ae567edf10b27c
)

# These settings apply only in the test directory and its dependencies.
set(BUILD_SHARED_LIBS OFF)
set(BUILD_GMOCK OFF)
set(INSTALL_GTEST OFF)
set(gtest_force_shared_crt ON)
FetchContent_MakeAvailable(googletest)

option(OPENTLV_TEST_COVERAGE "Measure Core and IO across all contract tests with GCC" OFF)
if(OPENTLV_TEST_COVERAGE)
    foreach(target IN ITEMS opentlv_core opentlv_io)
        target_compile_options(${target} PRIVATE -O0 --coverage -pedantic-errors -Wall -Wextra -Werror)
        target_link_options(${target} PRIVATE --coverage)
    endforeach()
    add_link_options(--coverage)
    foreach(source IN ITEMS fixed_model.c fixed_reader.c fixed_writer.c fixed_diagnostics.c fixed_model.cpp)
        string(REPLACE "." "_" target "${source}")
        add_executable(${target} "${source}")
        target_compile_options(${target} PRIVATE -pedantic-errors -Wall -Wextra -Werror)
        set_target_properties(${target} PROPERTIES C_STANDARD 99 CXX_STANDARD 11
            C_STANDARD_REQUIRED YES CXX_STANDARD_REQUIRED YES C_EXTENSIONS NO CXX_EXTENSIONS NO)
        if(source STREQUAL "fixed_model.c")
            target_link_libraries(${target} PRIVATE OpenTLV::core)
        else()
            target_link_libraries(${target} PRIVATE OpenTLV::io)
        endif()
        if(WIN32)
            set_target_properties(${target} PROPERTIES
                RUNTIME_OUTPUT_DIRECTORY "$<TARGET_FILE_DIR:opentlv_core>")
        endif()
        add_test(NAME ${target} COMMAND ${target})
    endforeach()
endif()

add_executable(opentlv_core_tests
    core/fixed_length_test.cpp
    core/fixed_measure_test.cpp
    core/result_test.cpp
    core/size_test.cpp
    core/value_test.cpp
)
add_executable(opentlv_io_tests
    io/diagnostic_test.cpp
    io/reader_test.cpp
    io/writer_test.cpp
)

add_executable(opentlv_integration_tests integration/fixed_round_trip_test.cpp)

function(register_opentlv_tests suite module)
    set(test_target opentlv_${suite}_tests)
    target_link_libraries(${test_target} PRIVATE OpenTLV::${module} GTest::gtest_main)
    if(WIN32)
        # Windows must find the module DLLs during discovery and execution.
        set_target_properties(${test_target} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "$<TARGET_FILE_DIR:opentlv_${module}>"
        )
    endif()
    gtest_discover_tests(${test_target}
        DISCOVERY_MODE PRE_TEST
        NO_PRETTY_VALUES
        PROPERTIES LABELS ${suite}
    )
endfunction()

register_opentlv_tests(core core)
register_opentlv_tests(io io)
register_opentlv_tests(integration io)
