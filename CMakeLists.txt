cmake_minimum_required(VERSION 3.16)

# Core, IO, Document, Query, Schema and Codec are selected by linking. Reject obsolete options
# instead of silently building a different feature set from the one requested.
foreach(_removed IN ITEMS OPENTLV_READER OPENTLV_WRITER OPENTLV_DOCUMENT OPENTLV_QUERY OPENTLV_CODEC OPENTLV_SCHEMA)
    if(DEFINED ${_removed})
        message(FATAL_ERROR "${_removed} was removed; link OpenTLV::Core, OpenTLV::IO, OpenTLV::Document, OpenTLV::Query OpenTLV::Codec or OpenTLV::Schema instead")
    endif()
endforeach()

# Protocol families are selected by linking their targets, not by cache options.
foreach(_family IN ITEMS FORMAT_ASN1 FORMAT_BER FORMAT_DER FORMAT_CER EMV BLUETOOTH NFC DHCP LLDP)
    if(DEFINED OPENTLV_${_family})
        message(FATAL_ERROR "OPENTLV_${_family} was removed; link OpenTLV::ASN1, OpenTLV::EMV, OpenTLV::Bluetooth, OpenTLV::NFC, OpenTLV::DHCP or OpenTLV::LLDP instead")
    endif()
    # Transitional feature guards and config mirrors remain constant until #654.
    set(OPENTLV_${_family} ON)
endforeach()

include(cmake/version.cmake)

project(OpenTLV VERSION ${OPENTLV_VERSION} LANGUAGES C)

opentlv_generate_version_files()

# Keep standalone defaults while embedding only the libraries in a consumer.
set(_OPENTLV_TOP_LEVEL OFF)
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(_OPENTLV_TOP_LEVEL ON)
endif()

# Native libraries
option(OPENTLV_BUILD_SHARED_LIBS "Build compiled modules as shared libraries instead of static" ON)
option(OPENTLV_BUILD_CXX "Build the tlv++ header-only C++ layer and its tests" ON)

# Core capabilities
# Compatibility macro selection remains constant until aggregate cleanup.
set(OPENTLV_DOCUMENT ON)
set(OPENTLV_QUERY ON)
set(OPENTLV_SCHEMA ON)
set(OPENTLV_CODEC ON)

# Query features
option(OPENTLV_QUERY_FRONTEND "Include Query text parsing and compilation" ON)
option(OPENTLV_QUERY_SET_OPERATIONS "Include Query union, intersection and difference operations" ON)

# Tools and documentation
option(OPENTLV_BUILD_EXAMPLES "Build examples" ${_OPENTLV_TOP_LEVEL})
option(OPENTLV_BUILD_DOCS "Enable the Doxygen C and C++ API reference targets" OFF)

# Tests and quality checks
option(OPENTLV_BUILD_TESTS "Build tests" ${_OPENTLV_TOP_LEVEL})
option(OPENTLV_BUILD_UNIT_TESTS "Build component contract tests" ON)
option(OPENTLV_BUILD_INTEGRATION_TESTS "Build format and layer integration tests" ON)
option(OPENTLV_BUILD_QUERY_TESTS "Build independent C Query conformance tests (no C++ or GoogleTest required)" ON)
option(OPENTLV_BUILD_PROPERTY_TESTS "Build deterministic generated-corpus property tests (requires C++11 and Python)" OFF)
option(OPENTLV_BUILD_BENCHMARKS "Build Google Benchmark performance suites" OFF)
option(OPENTLV_BUILD_FUZZING "Build C API fuzz targets with libFuzzer, ASan and UBSan" OFF)
option(OPENTLV_BUILD_MEMCHECK "Enable native Linux CTest Valgrind Memcheck runs" OFF)
option(OPENTLV_WARNINGS_AS_ERRORS "Treat OpenTLV compiler warnings as errors" ON)

configure_file(tlv/resources/config.h.in "${PROJECT_BINARY_DIR}/generated/include/tlv/config.h" @ONLY)
configure_file(tlv/resources/config.c.in "${PROJECT_BINARY_DIR}/generated/src/config.c" @ONLY)

# Only executable C++ consumers need a compiler. The header-only facade carries
# INTERFACE requirements without enabling CXX in a C-only library configuration.
# Enable it here for multi-config generators before configuring consumers.
if((OPENTLV_BUILD_TESTS AND (OPENTLV_BUILD_UNIT_TESTS OR
   OPENTLV_BUILD_INTEGRATION_TESTS OR OPENTLV_BUILD_PROPERTY_TESTS)) OR
   (OPENTLV_BUILD_CXX AND (OPENTLV_BUILD_TESTS OR OPENTLV_BUILD_EXAMPLES)) OR
   OPENTLV_BUILD_BENCHMARKS)
    enable_language(CXX)
endif()

include(cmake/compiler.cmake)
include(cmake/memcheck.cmake)
include(GNUInstallDirs)
include(cmake/modules.cmake)

if(_OPENTLV_TOP_LEVEL)
    include(cmake/clang_format.cmake)
    opentlv_add_clang_format_targets()
    include(cmake/cppcheck.cmake)
    opentlv_add_cppcheck_targets()
endif()

add_subdirectory(tlv)
add_library(OpenTLV::tlv ALIAS tlv)

if(OPENTLV_BUILD_DOCS)
    include(cmake/doxygen.cmake)
endif()

if(OPENTLV_BUILD_FUZZING)
    add_subdirectory(tests/fuzz)
endif()

if(OPENTLV_BUILD_CXX)
    add_subdirectory(tlv++)
    add_library(OpenTLV::tlv++ ALIAS tlv++)
endif()

if(_OPENTLV_TOP_LEVEL)
    include(cmake/clang_tidy.cmake)
    opentlv_add_clang_tidy_targets()
endif()

if(OPENTLV_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(OPENTLV_BUILD_EXAMPLES)
    # Runnable examples are acceptance tests even without GoogleTest targets.
    enable_testing()
    add_subdirectory(examples)
endif()

if(OPENTLV_BUILD_BENCHMARKS AND OPENTLV_FORMAT_BER)
    add_subdirectory(benchmarks)
endif()

opentlv_finalize_modules()
opentlv_finalize_shared_runtime()
include(cmake/install.cmake)
if(_OPENTLV_TOP_LEVEL)
    include(cmake/packaging.cmake)
endif()
