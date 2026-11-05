# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Marek Cingel

include_guard(GLOBAL)

# Adds the explicit opentlv_amalgamation target, which writes the C source
# amalgamation to <build>/amalgamation/ (docs/development/amalgamation.md).
# Standalone builds only, after opentlv_finalize_modules(). The target is not
# part of ALL, and install() never generates the amalgamation.
function(opentlv_add_amalgamation_target)
    set(registry "${OpenTLV_BINARY_DIR}/opentlv-registry.cmake")
    if(NOT EXISTS "${registry}")
        message(FATAL_ERROR "Call opentlv_finalize_modules() before opentlv_add_amalgamation_target()")
    endif()
    set(config "${OpenTLV_BINARY_DIR}/opentlv-amalgamation.cmake")
    set(content "# Generated amalgamation inputs; see docs/development/amalgamation.md.\n")
    foreach(pair IN ITEMS
            "SOURCE_DIR=${OpenTLV_SOURCE_DIR}" "BINARY_DIR=${OpenTLV_BINARY_DIR}"
            "REGISTRY=${registry}" "VERSION=${OPENTLV_VERSION_STRING}"
            "REVISION=${OPENTLV_VERSION_REVISION}" "COMMIT=${OPENTLV_GIT_COMMIT_HASH}"
            "BRANCH=${OPENTLV_GIT_BRANCH}" "TAG=${OPENTLV_GIT_TAG}"
            "QUERY_FRONTEND=${OPENTLV_QUERY_FRONTEND}"
            "QUERY_SET_OPERATIONS=${OPENTLV_QUERY_SET_OPERATIONS}")
        string(FIND "${pair}" "=" split)
        string(SUBSTRING "${pair}" 0 ${split} name)
        math(EXPR split "${split} + 1")
        string(SUBSTRING "${pair}" ${split} -1 value)
        string(APPEND content "set(OPENTLV_AMALGAMATION_${name} [==[${value}]==])\n")
    endforeach()
    file(WRITE "${config}" "${content}")
    add_custom_target(opentlv_amalgamation
        COMMAND "${CMAKE_COMMAND}" "-DOPENTLV_AMALGAMATION_CONFIG=${config}"
            "-DOPENTLV_AMALGAMATION_OUTPUT=${OpenTLV_BINARY_DIR}/amalgamation"
            -P "${OpenTLV_SOURCE_DIR}/cmake/amalgamation/generate.cmake"
        COMMENT "Generating the OpenTLV C source amalgamation"
        VERBATIM)
endfunction()
