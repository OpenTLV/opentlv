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

    # An opt-in source distribution, independent of conventional libraries.
    # Never generate during install, nor silently omit missing outputs.
    set(destination "${CMAKE_INSTALL_DATADIR}/OpenTLV/amalgamation")
    string(CONFIGURE [=[
        foreach(path IN ITEMS opentlv.h opentlv.c opentlv-amalgamation.json include/opentlv/tlv.h)
            if(NOT EXISTS "@OpenTLV_BINARY_DIR@/amalgamation/${path}")
                message(FATAL_ERROR "Amalgamation requires generated outputs. Run cmake --build <build> --target opentlv_amalgamation before installing --component Amalgamation.")
            endif()
        endforeach()
    ]=] check_outputs @ONLY)
    install(CODE "${check_outputs}" COMPONENT Amalgamation EXCLUDE_FROM_ALL)
    install(FILES
        "${OpenTLV_BINARY_DIR}/amalgamation/opentlv.h"
        "${OpenTLV_BINARY_DIR}/amalgamation/opentlv.c"
        "${OpenTLV_BINARY_DIR}/amalgamation/opentlv-amalgamation.json"
        "${OpenTLV_SOURCE_DIR}/LICENSE" "${OpenTLV_SOURCE_DIR}/THIRD_PARTY.md"
        DESTINATION "${destination}" COMPONENT Amalgamation EXCLUDE_FROM_ALL)
    install(DIRECTORY "${OpenTLV_BINARY_DIR}/amalgamation/include"
        DESTINATION "${destination}" COMPONENT Amalgamation EXCLUDE_FROM_ALL)
    install(FILES "${OpenTLV_SOURCE_DIR}/LICENSES/MIT.txt"
        DESTINATION "${destination}/LICENSES" COMPONENT Amalgamation EXCLUDE_FROM_ALL)
    install(FILES
        "${OpenTLV_SOURCE_DIR}/tests/amalgamation/consumer/consumer.c"
        "${OpenTLV_SOURCE_DIR}/tests/amalgamation/consumer/consumer.cpp"
        "${OpenTLV_SOURCE_DIR}/tests/amalgamation/consumer/README.md"
        DESTINATION "${destination}/examples" COMPONENT Amalgamation EXCLUDE_FROM_ALL)
endfunction()
