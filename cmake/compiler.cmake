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

# Copy the local shared-library dependency closure beside Windows consumers.
function(opentlv_copy_shared_runtime target)
    if(WIN32 AND OPENTLV_BUILD_SHARED_LIBS)
        get_target_property(links ${target} LINK_LIBRARIES)
        set(runtime opentlv_core)
        if(NOT links STREQUAL "opentlv_core")
            list(APPEND runtime opentlv_io)
            if(links STREQUAL "opentlv_query")
                list(APPEND runtime opentlv_query)
            elseif(links STREQUAL "opentlv_query_document")
                list(APPEND runtime opentlv_query opentlv_document opentlv_query_document)
            elseif(links STREQUAL "opentlv_generator")
                list(APPEND runtime opentlv_generator)
            elseif(NOT links STREQUAL "opentlv_io")
                list(APPEND runtime opentlv_document)
                if(NOT links STREQUAL "opentlv_document")
                    list(APPEND runtime tlv opentlv_generator opentlv_query opentlv_query_document)
                endif()
            endif()
        endif()
        foreach(library IN LISTS runtime)
            add_custom_command(TARGET ${target} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:${library}>" "$<TARGET_FILE_DIR:${target}>")
        endforeach()
    endif()
endfunction()
