# Keep instrumentation private to a separate copy of the enabled C sources.
if(NOT CMAKE_C_COMPILER_ID STREQUAL "Clang" OR NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "OPENTLV_BUILD_FUZZING requires Clang 18+ on Linux (or WSL).")
endif()
opentlv_check_compiler()

include(CheckCSourceCompiles)
set(CMAKE_REQUIRED_FLAGS "-fsanitize=fuzzer,address,undefined")
check_c_source_compiles("
    #include <stddef.h>
    #include <stdint.h>
    int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
        (void)data; (void)size; return 0;
    }" OPENTLV_HAS_LIBFUZZER)
unset(CMAKE_REQUIRED_FLAGS)
if(NOT OPENTLV_HAS_LIBFUZZER)
    message(FATAL_ERROR "Clang must provide working libFuzzer, ASan and UBSan runtimes.")
endif()

# Instrument each functional module once. Fuzz executables link only their
# declared module closure; production libraries remain uninstrumented.
function(opentlv_fuzz_module key)
    if(TARGET fuzz_module_${key})
        return()
    endif()
    get_property(module GLOBAL PROPERTY OPENTLV_MODULE_${key})
    get_target_property(sources ${module} INTERFACE_OPENTLV_SOURCES)
    get_target_property(includes ${module} INCLUDE_DIRECTORIES)
    add_library(fuzz_module_${key} STATIC EXCLUDE_FROM_ALL ${sources})
    target_include_directories(fuzz_module_${key} PUBLIC ${includes})
    string(TOUPPER "${module}" prefix)
    target_compile_definitions(fuzz_module_${key} PUBLIC ${prefix}_STATIC_DEFINE)
    target_compile_features(fuzz_module_${key} PUBLIC c_std_99)
    opentlv_configure_compiler(fuzz_module_${key})
    target_compile_options(fuzz_module_${key} PRIVATE
        -fsanitize=fuzzer-no-link,address,undefined -fno-sanitize-recover=all
        -fno-omit-frame-pointer -g)
    foreach(scope IN ITEMS PUBLIC PRIVATE)
        get_target_property(dependencies ${module} INTERFACE_OPENTLV_${scope}_DEPS)
        foreach(dependency IN LISTS dependencies)
            opentlv_fuzz_module(${dependency})
            target_link_libraries(fuzz_module_${key} ${scope} fuzz_module_${dependency})
        endforeach()
    endforeach()
endfunction()

set(fuzz_modules_read core io asn1 bluetooth)
set(fuzz_modules_visitor core io asn1 bluetooth)
set(fuzz_modules_roundtrip core io asn1 bluetooth)
set(fuzz_modules_utf8 core)
set(fuzz_modules_fixed core io)
set(fuzz_modules_der core io asn1)
set(fuzz_modules_der_schema core io asn1 schema)
set(fuzz_modules_codec core codec emv)
set(fuzz_modules_dol core emv)
set(fuzz_modules_query_parse core query)
set(fuzz_modules_query_match core io query)
set(fuzz_modules_query_program core io query query-codec)
set(fuzz_modules_query_image core io query)
set(fuzz_modules_query_pairs core io query document query-document codec)

include(${CMAKE_CURRENT_LIST_DIR}/targets.cmake)
foreach(name IN LISTS fuzz_targets)
    add_executable(fuzz_${name} ${fuzz_source_${name}})
    target_include_directories(fuzz_${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    foreach(module IN LISTS fuzz_modules_${name})
        opentlv_fuzz_module(${module})
        target_link_libraries(fuzz_${name} PRIVATE fuzz_module_${module})
    endforeach()
    opentlv_configure_compiler(fuzz_${name})
    target_compile_options(fuzz_${name} PRIVATE
        -fsanitize=fuzzer-no-link,address,undefined -fno-sanitize-recover=all
        -fno-omit-frame-pointer -g)
    target_link_options(fuzz_${name} PRIVATE -fsanitize=fuzzer,address,undefined)
endforeach()
