cmake_minimum_required(VERSION 3.18)
project(OpenTLVFuzz LANGUAGES C CXX)

if(NOT CMAKE_C_COMPILER_ID STREQUAL "Clang" OR NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    message(FATAL_ERROR "Use Clang and Clang++ with libFuzzer, ASan and UBSan")
endif()

set(BUILD_TESTING OFF)
set(BUILD_SHARED_LIBS OFF)
if(CMAKE_C_SIMULATE_ID STREQUAL "MSVC")
    # Match the static CRT used by LLVM's Windows libFuzzer driver.
    set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreaded)
endif()
add_compile_options(-fsanitize=fuzzer-no-link,address,undefined
                    -fno-sanitize-recover=all -fno-omit-frame-pointer)
add_subdirectory(../.. opentlv EXCLUDE_FROM_ALL)

foreach(suite IN ITEMS reader writer)
    set(target fuzz_${suite})
    add_executable(${target} ${suite}.c)
    set_target_properties(${target} PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED YES
                          C_EXTENSIONS NO LINKER_LANGUAGE CXX)
    target_link_libraries(${target} PRIVATE OpenTLV::io)
    target_link_options(${target} PRIVATE -fsanitize=fuzzer,address,undefined)
endforeach()
