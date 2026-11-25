add_executable(opentlv_abi_layout ../abi_layout.cpp)
target_link_libraries(opentlv_abi_layout PRIVATE OpenTLV::io)
set_target_properties(opentlv_abi_layout PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS OFF)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang" AND NOT MSVC)
    target_compile_options(opentlv_abi_layout PRIVATE -pedantic-errors -Wall -Wextra -Werror)
endif()

add_test(NAME abi-layout COMMAND opentlv_abi_layout)
set_tests_properties(abi-layout PROPERTIES SKIP_RETURN_CODE 77)
