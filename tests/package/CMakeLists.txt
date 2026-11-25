cmake_minimum_required(VERSION 3.15)
project(OpenTLVConsumer LANGUAGES ${CONSUMER_LANGUAGE})

set(CMAKE_DISABLE_FIND_PACKAGE_GTest TRUE)
# Restrict discovery to the package under test, even on a host with OpenTLV installed.
find_package(OpenTLV CONFIG REQUIRED PATHS "${PACKAGE_PREFIX}" NO_DEFAULT_PATH)

enable_testing()
set(CMAKE_${CONSUMER_LANGUAGE}_STANDARD_REQUIRED YES)
set(CMAKE_${CONSUMER_LANGUAGE}_EXTENSIONS OFF)
if(CONSUMER_LANGUAGE STREQUAL "C")
    set(CMAKE_C_STANDARD 99)
    add_executable(core-consumer consumer.c)
    add_executable(io-consumer ../../examples/write.c)
else()
    # The imported targets must raise this project's default to C++17.
    set(CMAKE_CXX_STANDARD 14)
    add_executable(core-consumer core.cpp)
    add_executable(io-consumer consumer.cpp)
endif()

foreach(module IN ITEMS core io)
    target_link_libraries(${module}-consumer PRIVATE OpenTLV::${module})
    if(CMAKE_${CONSUMER_LANGUAGE}_COMPILER_ID MATCHES "GNU|Clang" AND NOT MSVC)
        target_compile_options(${module}-consumer PRIVATE -pedantic-errors -Wall -Wextra -Werror)
    endif()
    add_test(NAME ${module}-consumer COMMAND ${module}-consumer)
endforeach()
