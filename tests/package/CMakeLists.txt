cmake_minimum_required(VERSION 3.15)
project(OpenTLVConsumer LANGUAGES ${CONSUMER_LANGUAGE})

set(CMAKE_DISABLE_FIND_PACKAGE_GTest TRUE)
# Restrict discovery to the relocated install, even on a host with OpenTLV installed.
find_package(OpenTLV CONFIG REQUIRED PATHS "${PACKAGE_PREFIX}" NO_DEFAULT_PATH)

if(CONSUMER_LANGUAGE STREQUAL "C")
    add_executable(consumer consumer.c)
    target_link_libraries(consumer PRIVATE OpenTLV::core)
    set_target_properties(consumer PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED YES C_EXTENSIONS OFF)
else()
    add_executable(consumer consumer.cpp)
    target_link_libraries(consumer PRIVATE OpenTLV::io)
    set_target_properties(consumer PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS OFF)
endif()
if(CMAKE_${CONSUMER_LANGUAGE}_COMPILER_ID MATCHES "GNU|Clang" AND NOT MSVC)
    target_compile_options(consumer PRIVATE -pedantic-errors -Wall -Wextra -Werror)
endif()

enable_testing()
add_test(NAME installed-consumer COMMAND consumer)
