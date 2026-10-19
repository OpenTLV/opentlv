# New releases use v-prefixed Semantic Versioning. Historical unprefixed tags
# remain readable so existing development checkouts retain their version.
set(_OPENTLV_VERSION_MODULE_DIR "${CMAKE_CURRENT_LIST_DIR}")
get_filename_component(_OPENTLV_VERSION_SOURCE_DIR "${_OPENTLV_VERSION_MODULE_DIR}/.." ABSOLUTE)

# Remove the legacy manually configured value; pre-release now comes from Git.
unset(OPENTLV_VERSION_PRERELEASE CACHE)

set(_opentlv_archive_metadata "${_OPENTLV_VERSION_SOURCE_DIR}/opentlv-source.txt")
set(_opentlv_from_archive OFF)
if(EXISTS "${_opentlv_archive_metadata}")
    # Release archives carry data, never an executable CMake include. Prefer it
    # over any unrelated parent repository when embedded by a consumer.
    if(EXISTS "${_OPENTLV_VERSION_SOURCE_DIR}/.git")
        message(FATAL_ERROR "Source archive metadata is not allowed in a Git checkout")
    endif()
    file(READ "${_opentlv_archive_metadata}" _opentlv_archive_data)
    if(NOT _opentlv_archive_data MATCHES "^version=([^\n]+)\ncommit=([0-9a-f]+)\n$")
        message(FATAL_ERROR "Invalid OpenTLV source archive metadata")
    endif()
    set(OPENTLV_GIT_TAG "v${CMAKE_MATCH_1}")
    set(_opentlv_archive_commit "${CMAKE_MATCH_2}")
    string(LENGTH "${_opentlv_archive_commit}" _opentlv_commit_length)
    if(NOT _opentlv_commit_length EQUAL 40)
        message(FATAL_ERROR "Source archive must identify an exact Git commit")
    endif()
    set(_opentlv_from_archive ON)
    set(_opentlv_git_tag_result 0)
else()
    execute_process(
        COMMAND git describe --tags --abbrev=0 --match "v[0-9]*" --match "[0-9]*" HEAD
        WORKING_DIRECTORY "${_OPENTLV_VERSION_SOURCE_DIR}"
        RESULT_VARIABLE _opentlv_git_tag_result
        OUTPUT_VARIABLE _opentlv_described_tag
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
endif()

if(_opentlv_git_tag_result EQUAL 0)
    if(NOT _opentlv_from_archive)
        execute_process(
            COMMAND git rev-list -n 1 "${_opentlv_described_tag}"
            WORKING_DIRECTORY "${_OPENTLV_VERSION_SOURCE_DIR}"
            RESULT_VARIABLE _opentlv_tag_commit_result
            OUTPUT_VARIABLE _opentlv_tag_commit
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
        if(NOT _opentlv_tag_commit_result EQUAL 0)
            message(FATAL_ERROR
                "Could not resolve Git tag '${_opentlv_described_tag}'")
        endif()

        execute_process(
            COMMAND git tag --points-at "${_opentlv_tag_commit}"
                --list "v[0-9]*" "[0-9]*" --sort=-version:refname
            WORKING_DIRECTORY "${_OPENTLV_VERSION_SOURCE_DIR}"
            RESULT_VARIABLE _opentlv_tags_at_commit_result
            OUTPUT_VARIABLE _opentlv_tags_at_commit
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
        if(NOT _opentlv_tags_at_commit_result EQUAL 0)
            message(FATAL_ERROR
                "Could not inspect Git tags at commit '${_opentlv_tag_commit}'")
        endif()

        string(REPLACE "\r\n" "\n" _opentlv_tags_at_commit
            "${_opentlv_tags_at_commit}")
        string(REPLACE "\n" ";" _opentlv_tags_at_commit
            "${_opentlv_tags_at_commit}")

        set(OPENTLV_GIT_TAG "")
        set(_opentlv_prerelease_tag "")
        foreach(_tag IN LISTS _opentlv_tags_at_commit)
            if(_tag MATCHES "^v?[0-9]+\\.[0-9]+\\.[0-9]+$")
                set(OPENTLV_GIT_TAG "${_tag}")
                break()
            elseif(NOT _opentlv_prerelease_tag AND _tag MATCHES
                    "^v?[0-9]+\\.[0-9]+\\.[0-9]+-[0-9A-Za-z-]+(\\.[0-9A-Za-z-]+)*$")
                set(_opentlv_prerelease_tag "${_tag}")
            endif()
        endforeach()
        if(NOT OPENTLV_GIT_TAG)
            set(OPENTLV_GIT_TAG "${_opentlv_prerelease_tag}")
        endif()
    endif()

    if(NOT OPENTLV_GIT_TAG MATCHES
            "^v?([0-9]+)\\.([0-9]+)\\.([0-9]+)(-([0-9A-Za-z-]+(\\.[0-9A-Za-z-]+)*))?$")
        message(FATAL_ERROR
            "Git tag '${OPENTLV_GIT_TAG}' is not a valid Semantic Version")
    endif()

    set(OPENTLV_VERSION_MAJOR "${CMAKE_MATCH_1}")
    set(OPENTLV_VERSION_MINOR "${CMAKE_MATCH_2}")
    set(OPENTLV_VERSION_PATCH "${CMAKE_MATCH_3}")
    set(OPENTLV_VERSION_PRERELEASE "${CMAKE_MATCH_5}")
    set(OPENTLV_VERSION
        "${OPENTLV_VERSION_MAJOR}.${OPENTLV_VERSION_MINOR}.${OPENTLV_VERSION_PATCH}")

    foreach(_component IN ITEMS
            OPENTLV_VERSION_MAJOR OPENTLV_VERSION_MINOR OPENTLV_VERSION_PATCH)
        if(${_component} MATCHES "^0[0-9]+$")
            message(FATAL_ERROR
                "Git tag '${OPENTLV_GIT_TAG}' contains a numeric component with leading zeroes")
        endif()
    endforeach()

    if(OPENTLV_VERSION_PRERELEASE)
        string(REPLACE "." ";" _opentlv_prerelease_identifiers
            "${OPENTLV_VERSION_PRERELEASE}")
        foreach(_identifier IN LISTS _opentlv_prerelease_identifiers)
            if(_identifier MATCHES "^0[0-9]+$")
                message(FATAL_ERROR
                    "Git tag '${OPENTLV_GIT_TAG}' contains a numeric pre-release identifier with leading zeroes")
            endif()
        endforeach()
    endif()

    if(_opentlv_from_archive)
        set(OPENTLV_VERSION_REVISION 0)
    else()
        execute_process(
            COMMAND git rev-list --count "${OPENTLV_GIT_TAG}..HEAD"
            WORKING_DIRECTORY "${_OPENTLV_VERSION_SOURCE_DIR}"
            RESULT_VARIABLE _opentlv_git_revision_result
            OUTPUT_VARIABLE OPENTLV_VERSION_REVISION
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
        if(NOT _opentlv_git_revision_result EQUAL 0)
            message(FATAL_ERROR
                "Could not count commits since Git tag '${OPENTLV_GIT_TAG}'")
        endif()
    endif()
else()
    set(OPENTLV_GIT_TAG "unknown")
    set(OPENTLV_VERSION_MAJOR 0)
    set(OPENTLV_VERSION_MINOR 0)
    set(OPENTLV_VERSION_PATCH 0)
    set(OPENTLV_VERSION_PRERELEASE "")
    set(OPENTLV_VERSION "0.0.0")
    set(OPENTLV_VERSION_REVISION 0)
    message(WARNING
        "No Semantic Version Git tag found; using fallback version 0.0.0")
endif()

if(OPENTLV_VERSION_PRERELEASE)
    set(OPENTLV_VERSION_STRING
        "${OPENTLV_VERSION}-${OPENTLV_VERSION_PRERELEASE}")
else()
    set(OPENTLV_VERSION_STRING "${OPENTLV_VERSION}")
endif()

execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY "${_OPENTLV_VERSION_SOURCE_DIR}"
    RESULT_VARIABLE _opentlv_git_hash_result
    OUTPUT_VARIABLE OPENTLV_GIT_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(NOT _opentlv_git_hash_result EQUAL 0)
    set(OPENTLV_GIT_COMMIT_HASH "unknown")
endif()

execute_process(
    COMMAND git rev-parse --abbrev-ref HEAD
    WORKING_DIRECTORY "${_OPENTLV_VERSION_SOURCE_DIR}"
    RESULT_VARIABLE _opentlv_git_branch_result
    OUTPUT_VARIABLE OPENTLV_GIT_BRANCH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(NOT _opentlv_git_branch_result EQUAL 0)
    set(OPENTLV_GIT_BRANCH "unknown")
endif()

if(_opentlv_from_archive)
    set(OPENTLV_GIT_COMMIT_HASH "${_opentlv_archive_commit}")
    set(OPENTLV_GIT_BRANCH "archive")
endif()

set(OPENTLV_GIT_REPO_VERSION
    "${OPENTLV_VERSION_STRING}-${OPENTLV_VERSION_REVISION}-${OPENTLV_GIT_COMMIT_HASH}")

function(opentlv_generate_version_files)
    configure_file(
        "${_OPENTLV_VERSION_MODULE_DIR}/../tlv/resources/version.h.in"
        "${PROJECT_BINARY_DIR}/generated/include/tlv/version.h"
        @ONLY
    )
    configure_file(
        "${_OPENTLV_VERSION_MODULE_DIR}/../tlv/resources/version.c.in"
        "${PROJECT_BINARY_DIR}/generated/src/version.c"
        @ONLY
    )

    message(STATUS
        "OpenTLV version: ${OPENTLV_VERSION_STRING} "
        "(${OPENTLV_GIT_REPO_VERSION}, branch ${OPENTLV_GIT_BRANCH})")
endfunction()
