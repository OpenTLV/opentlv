# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Marek Cingel

# Target selection for the C source amalgamation (docs/development/amalgamation.md).
# Configuration and cmake/amalgamation/generate.cmake share this resolver; it
# reads the OPENTLV_REGISTRY_* variables of the registry inventory in the
# caller's scope and keeps no list of its own.

# opentlv_amalgamation_select(<selection> <requested-out> <resolved-out>)
#
# <selection> is a list of registered export names, such as "Core;IO" or
# "EMV". An empty selection selects every target. <requested-out> receives the
# selected keys in registration order, or nothing for the complete
# composition. <resolved-out> receives, in registration order, every target of
# the composition: the selected targets, the transitive closure of their
# public, private and C++ header dependencies, and each header-only facade
# whose dependencies are all resolved. Invalid names fail with a diagnostic.
function(opentlv_amalgamation_select selection requested_out resolved_out)
    set(names)
    set(facades)
    foreach(key IN LISTS OPENTLV_REGISTRY_MODULES)
        set(name "${OPENTLV_REGISTRY_${key}_EXPORT_NAME}")
        set("key_of_${name}" "${key}")
        if(OPENTLV_REGISTRY_${key}_FACADE)
            list(APPEND facades "${name}")
        else()
            list(APPEND names "${name}")
        endif()
    endforeach()
    string(REPLACE ";" ", " valid "${names}")
    set(selected)
    foreach(item IN LISTS selection)
        string(STRIP "${item}" item)
        if(item STREQUAL "")
            continue()
        endif()
        if(item IN_LIST facades)
            set(dependencies)
            foreach(key IN LISTS OPENTLV_REGISTRY_${key_of_${item}}_PUBLIC_DEPS)
                list(APPEND dependencies "${OPENTLV_REGISTRY_${key}_EXPORT_NAME}")
            endforeach()
            string(REPLACE ";" ", " dependencies "${dependencies}")
            message(FATAL_ERROR "opentlv_amalgamation: OPENTLV_AMALGAMATION_TARGETS names ${item}, "
                "a header-only C++ facade without C sources. Select the targets it depends on "
                "(${dependencies}); its headers ship when all of them are composed.")
        endif()
        if(NOT item IN_LIST names)
            set(hint "")
            string(TOLOWER "${item}" lower)
            foreach(name IN LISTS names)
                string(TOLOWER "${name}" candidate)
                if(candidate STREQUAL lower OR key_of_${name} STREQUAL item)
                    set(hint " Did you mean ${name}?")
                endif()
            endforeach()
            if(item MATCHES ",")
                set(hint " Separate target names with ';'.")
            elseif(item STREQUAL "tlv")
                set(hint " tlv is the aggregate of every target; leave the selection empty for the complete composition.")
            endif()
            message(FATAL_ERROR "opentlv_amalgamation: OPENTLV_AMALGAMATION_TARGETS names ${item}, "
                "which is not a compiled target of the registry.${hint} Valid names: ${valid}.")
        endif()
        list(APPEND selected "${key_of_${item}}")
    endforeach()

    set(resolved)
    if(NOT selected)
        set(requested)
        foreach(key IN LISTS OPENTLV_REGISTRY_MODULES)
            if(NOT OPENTLV_REGISTRY_${key}_FACADE)
                list(APPEND resolved "${key}")
            endif()
        endforeach()
    else()
        # The closure of public, private and C++ header dependencies. A compiled
        # dependency needs its C; a C++ header dependency needs its headers.
        set(pending ${selected})
        while(pending)
            list(GET pending 0 key)
            list(REMOVE_AT pending 0)
            if(key IN_LIST resolved)
                continue()
            endif()
            if(NOT DEFINED OPENTLV_REGISTRY_${key}_EXPORT_NAME)
                message(FATAL_ERROR "opentlv_amalgamation: the registry lists the undeclared dependency ${key}")
            endif()
            list(APPEND resolved "${key}")
            list(APPEND pending ${OPENTLV_REGISTRY_${key}_PUBLIC_DEPS}
                ${OPENTLV_REGISTRY_${key}_PRIVATE_DEPS} ${OPENTLV_REGISTRY_${key}_FACADE_DEPS})
        endwhile()
        set(requested)
        set(ordered)
        foreach(key IN LISTS OPENTLV_REGISTRY_MODULES)
            if(key IN_LIST selected)
                list(APPEND requested "${key}")
            endif()
            if(key IN_LIST resolved AND NOT OPENTLV_REGISTRY_${key}_FACADE)
                list(APPEND ordered "${key}")
            endif()
        endforeach()
        set(resolved ${ordered})
    endif()
    # Facades contribute only headers. They ship when every dependency does, so
    # the complete composition keeps all of them.
    foreach(key IN LISTS OPENTLV_REGISTRY_MODULES)
        if(OPENTLV_REGISTRY_${key}_FACADE)
            set(complete TRUE)
            foreach(dependency IN LISTS OPENTLV_REGISTRY_${key}_PUBLIC_DEPS)
                if(NOT dependency IN_LIST resolved)
                    set(complete FALSE)
                endif()
            endforeach()
            if(complete)
                list(APPEND resolved "${key}")
            endif()
        endif()
    endforeach()
    set(ordered)
    foreach(key IN LISTS OPENTLV_REGISTRY_MODULES)
        if(key IN_LIST resolved)
            list(APPEND ordered "${key}")
        endif()
    endforeach()
    set(${requested_out} "${requested}" PARENT_SCOPE)
    set(${resolved_out} "${ordered}" PARENT_SCOPE)
endfunction()
