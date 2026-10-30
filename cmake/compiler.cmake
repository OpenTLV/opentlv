function(opentlv_check_compiler)
    foreach(language IN ITEMS C CXX)
        if(CMAKE_${language}_COMPILER_ID STREQUAL "Clang"
           AND CMAKE_${language}_COMPILER_VERSION VERSION_LESS 18)
            message(FATAL_ERROR "OpenTLV requires Clang 18 or newer (${language} compiler).")
        endif()
    endforeach()
endfunction()

# Apply only to OpenTLV build targets, never to dependencies or consumers.
function(opentlv_configure_compiler target)
    opentlv_check_compiler()

    target_compile_options(${target} PRIVATE
        "$<$<COMPILE_LANG_AND_ID:C,Clang>:-Wall;-Wextra;-Wpedantic;-Wstrict-prototypes>"
        "$<$<COMPILE_LANG_AND_ID:CXX,Clang>:-Wall;-Wextra;-Wpedantic>"
        "$<$<AND:$<BOOL:${OPENTLV_WARNINGS_AS_ERRORS}>,$<OR:$<COMPILE_LANG_AND_ID:C,GNU,Clang>,$<COMPILE_LANG_AND_ID:CXX,GNU,Clang>>>:-Werror>"
        "$<$<AND:$<BOOL:${OPENTLV_WARNINGS_AS_ERRORS}>,$<OR:$<COMPILE_LANG_AND_ID:C,MSVC>,$<COMPILE_LANG_AND_ID:CXX,MSVC>>>:/WX>"
    )
endfunction()

# Register consumers now; resolve the module dependency graph after finalization.
function(opentlv_copy_shared_runtime target)
    if(WIN32 AND OPENTLV_BUILD_SHARED_LIBS)
        set_property(GLOBAL APPEND PROPERTY OPENTLV_RUNTIME_CONSUMERS "${target}")
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -P "${OpenTLV_BINARY_DIR}/runtime-${target}-$<CONFIG>.cmake")
    endif()
endfunction()

function(opentlv_finalize_shared_runtime)
    get_property(consumers GLOBAL PROPERTY OPENTLV_RUNTIME_CONSUMERS)
    foreach(consumer IN LISTS consumers)
        set(script "")
        set(pending "${consumer}")
        set(visited)
        while(pending)
            list(POP_FRONT pending target)
            if(NOT TARGET "${target}" OR target IN_LIST visited)
                continue()
            endif()
            list(APPEND visited "${target}")
            get_target_property(type "${target}" TYPE)
            get_target_property(imported "${target}" IMPORTED)
            if(type STREQUAL "SHARED_LIBRARY" AND NOT imported)
                string(APPEND script "execute_process(COMMAND \"${CMAKE_COMMAND}\" -E copy_if_different \"$<TARGET_FILE:${target}>\" \"$<TARGET_FILE_DIR:${consumer}>\" RESULT_VARIABLE rc)\nif(NOT rc EQUAL 0)\nmessage(FATAL_ERROR \"Runtime copy failed\")\nendif()\n")
            endif()
            foreach(property IN ITEMS LINK_LIBRARIES INTERFACE_LINK_LIBRARIES)
                get_target_property(links "${target}" ${property})
                foreach(link IN LISTS links)
                    if(TARGET "${link}")
                        list(APPEND pending "${link}")
                    endif()
                endforeach()
            endforeach()
        endwhile()
        file(GENERATE OUTPUT "${OpenTLV_BINARY_DIR}/runtime-${consumer}-$<CONFIG>.cmake" CONTENT "${script}")
    endforeach()
endfunction()
