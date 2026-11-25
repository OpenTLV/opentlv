# Keep validation flags private to OpenTLV's targets, including the ABI probe
# built by the standalone package consumer.
function(opentlv_strict_compile_options target language)
    if(CMAKE_${language}_COMPILER_ID STREQUAL "MSVC")
        target_compile_options(${target} PRIVATE /W4 /WX)
        if(language STREQUAL "CXX")
            target_compile_options(${target} PRIVATE /permissive- /Zc:__cplusplus)
        elseif(MSVC_VERSION GREATER_EQUAL 1928)
            # MSVC's earliest selectable ISO C mode is C11.
            target_compile_options(${target} PRIVATE /std:c11)
        endif()
    elseif(CMAKE_${language}_COMPILER_ID MATCHES "GNU|Clang" AND NOT MSVC)
        target_compile_options(${target} PRIVATE -pedantic-errors -Wall -Wextra -Werror)
    endif()
endfunction()
