# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2026 Marek Cingel
cmake_minimum_required(VERSION 3.16)
project(discovery LANGUAGES NONE)

if(MODE STREQUAL "embedded")
    enable_language(C)
    if(DEPENDENCY STREQUAL "subdirectory")
        add_subdirectory("${LIBRARY_SOURCE}" opentlv)
    else()
        include(FetchContent)
        set(FETCHCONTENT_SOURCE_DIR_${DEPENDENCY_UPPER} "${LIBRARY_SOURCE}")
        set(arguments)
        if(NOT CMAKE_VERSION VERSION_LESS 3.24)
            if(OVERRIDE_PACKAGE)
                set(arguments OVERRIDE_FIND_PACKAGE)
            elseif(NOT PLAIN_FETCH)
                set(arguments FIND_PACKAGE_ARGS CONFIG NAMES OpenTLV)
            endif()
        endif()
        FetchContent_Declare(${DEPENDENCY} SOURCE_DIR "${LIBRARY_SOURCE}" ${arguments})
        FetchContent_MakeAvailable(${DEPENDENCY})
    endif()
endif()

if(CASE STREQUAL "required-version")
    find_package(${PACKAGE} ${OTHER_VERSION} EXACT CONFIG REQUIRED NAMES OpenTLV)
    message(FATAL_ERROR "Accepted a different required version")
endif()

if(CASE STREQUAL "required-component")
    if(MODE STREQUAL "fetch-package")
        include(FetchContent)
        FetchContent_Declare(${PACKAGE}
            GIT_REPOSITORY "${CMAKE_CURRENT_SOURCE_DIR}/nonexistent"
            FIND_PACKAGE_ARGS CONFIG REQUIRED NAMES OpenTLV COMPONENTS unavailable)
        FetchContent_MakeAvailable(${PACKAGE})
    else()
        find_package(${PACKAGE} CONFIG REQUIRED NAMES OpenTLV COMPONENTS unavailable)
    endif()
    message(FATAL_ERROR "Accepted an unknown required component")
endif()

# A failed probe must not import partial targets and poison a source fallback.
find_package(${PACKAGE} CONFIG QUIET NAMES OpenTLV COMPONENTS Core unavailable)
if(${PACKAGE}_FOUND OR OpenTLV_FOUND)
    message(FATAL_ERROR "Accepted an unknown required component")
endif()
if(NOT MODE STREQUAL "embedded" AND TARGET OpenTLV::Core)
    message(FATAL_ERROR "Failed component lookup imported partial targets")
endif()

# An unrelated target must not make an unknown optional component available.
add_library(OpenTLV::unavailable INTERFACE IMPORTED)
if(MODE STREQUAL "fetch-package")
    include(FetchContent)
    FetchContent_Declare(${PACKAGE}
        GIT_REPOSITORY "${CMAKE_CURRENT_SOURCE_DIR}/nonexistent"
        FIND_PACKAGE_ARGS ${VERSION} EXACT CONFIG REQUIRED NAMES OpenTLV
            COMPONENTS Core OPTIONAL_COMPONENTS unavailable)
    FetchContent_MakeAvailable(${PACKAGE})
else()
    find_package(${PACKAGE} ${VERSION} EXACT CONFIG REQUIRED NAMES OpenTLV
        COMPONENTS Core OPTIONAL_COMPONENTS unavailable)
endif()
if(NOT TARGET OpenTLV::Core OR NOT ${PACKAGE}_Core_FOUND OR NOT OpenTLV_Core_FOUND
   OR ${PACKAGE}_unavailable_FOUND OR OpenTLV_unavailable_FOUND)
    message(FATAL_ERROR "Incorrect component metadata")
endif()
if(NOT MODE STREQUAL "embedded" AND (TARGET OpenTLV::IO OR TARGET OpenTLV::tlv))
    message(FATAL_ERROR "Core-only lookup loaded unrelated targets")
endif()
if(NOT "${${PACKAGE}_VERSION}" VERSION_EQUAL VERSION OR NOT OpenTLV_VERSION VERSION_EQUAL VERSION)
    message(FATAL_ERROR "Missing or inconsistent Library version metadata")
endif()

# Wrong versions fail even when embedded aliases already exist. The conflicting
# installation advertises OTHER_VERSION, so a redirect miss cannot hide a mix.
find_package(${PACKAGE} ${OTHER_VERSION} EXACT CONFIG QUIET NAMES OpenTLV)
if(${PACKAGE}_FOUND)
    message(FATAL_ERROR "Accepted a different exact version")
endif()
find_package(${PACKAGE} ${VERSION}.1 EXACT CONFIG QUIET NAMES OpenTLV)
if(${PACKAGE}_FOUND)
    message(FATAL_ERROR "Accepted a different exact tweak version")
endif()
if(MODE STREQUAL "embedded")
    find_package(OpenTLV ${OTHER_VERSION} EXACT CONFIG QUIET
        PATHS "${CONFLICT_PREFIX}" NO_DEFAULT_PATH)
    if(OpenTLV_FOUND OR NOT OpenTLV_NOT_FOUND_MESSAGE MATCHES "different Library|does not match requested version")
        message(FATAL_ERROR "Conflicting installed and embedded Library identities were not rejected")
    endif()
    if(NOT CMAKE_VERSION VERSION_LESS 3.24)
        # Old releases did not validate target provenance. A mismatched redirect
        # must not fall through and execute even such an installed config.
        set(legacy "${CMAKE_CURRENT_BINARY_DIR}/legacy-package")
        file(MAKE_DIRECTORY "${legacy}")
        configure_file("${CONFLICT_PREFIX}/lib/cmake/OpenTLV/OpenTLVConfigVersion.cmake"
            "${legacy}/OpenTLVConfigVersion.cmake" COPYONLY)
        file(WRITE "${legacy}/OpenTLVConfig.cmake"
            "message(FATAL_ERROR \"Discovery executed a conflicting legacy config\")\n")
        find_package(${PACKAGE} ${OTHER_VERSION} EXACT CONFIG QUIET NAMES OpenTLV
            PATHS "${legacy}" NO_DEFAULT_PATH)
        if(${PACKAGE}_FOUND OR NOT OpenTLV_NOT_FOUND_MESSAGE MATCHES "does not match requested version")
            message(FATAL_ERROR "Redirect did not reject the version before installed discovery")
        endif()
    endif()
endif()

# Failed lookups must not leave FOUND false on a later valid request. Both the
# canonical package name and dependency spelling must describe the same targets.
find_package(OpenTLV ${VERSION} EXACT CONFIG REQUIRED COMPONENTS tlv)
find_package(${PACKAGE} CONFIG REQUIRED NAMES OpenTLV COMPONENTS Core)
if(NOT OpenTLV_FOUND OR NOT ${PACKAGE}_FOUND OR NOT TARGET OpenTLV::tlv OR
   NOT OpenTLV_VERSION VERSION_EQUAL VERSION OR NOT "${${PACKAGE}_VERSION}" VERSION_EQUAL VERSION)
    message(FATAL_ERROR "Repeated discovery retained stale package metadata")
endif()

# An explicit config include bypasses redirects; the provenance check still
# rejects another installation even when it advertises the same version.
set(CMAKE_FIND_PACKAGE_NAME OpenTLV)
include("${OTHER_CONFIG}")
if(OpenTLV_FOUND OR NOT OpenTLV_NOT_FOUND_MESSAGE MATCHES "different Library")
    message(FATAL_ERROR "Accepted targets belonging to another package directory")
endif()
