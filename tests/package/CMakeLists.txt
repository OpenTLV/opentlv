cmake_minimum_required(VERSION 3.15)
project(OpenTLVConsumer LANGUAGES ${CONSUMER_LANGUAGE})
include(../strict_compile_options.cmake)

set(CMAKE_DISABLE_FIND_PACKAGE_GTest TRUE)
# Restrict discovery to the package under test, even on a host with OpenTLV installed.
find_package(OpenTLV CONFIG REQUIRED PATHS "${PACKAGE_PREFIX}" NO_DEFAULT_PATH)

get_target_property(core_links OpenTLV::core INTERFACE_LINK_LIBRARIES)
get_target_property(io_links OpenTLV::io INTERFACE_LINK_LIBRARIES)
if(core_links MATCHES "OpenTLV::io" OR NOT "OpenTLV::core" IN_LIST io_links)
    message(FATAL_ERROR "Expected IO to depend publicly on Core and Core to be usable alone. "
                        "Core links: ${core_links}. IO links: ${io_links}.")
endif()

enable_testing()
set(CMAKE_${CONSUMER_LANGUAGE}_STANDARD_REQUIRED YES)
set(CMAKE_${CONSUMER_LANGUAGE}_EXTENSIONS OFF)
if(CONSUMER_LANGUAGE STREQUAL "C")
    set(CMAKE_C_STANDARD 99)
    set(probe_extension c)
    add_executable(core-consumer consumer.c)
    add_executable(io-consumer ../../examples/write.c)
else()
    # The imported targets must raise this project's default to C++17.
    set(CMAKE_CXX_STANDARD 14)
    set(probe_extension cpp)
    add_executable(core-consumer core.cpp)
    add_executable(io-consumer consumer.cpp)
    add_subdirectory(../abi abi)
endif()

foreach(module IN ITEMS core io)
    target_link_libraries(${module}-consumer PRIVATE OpenTLV::${module})
    opentlv_strict_compile_options(${module}-consumer ${CONSUMER_LANGUAGE})
    add_test(NAME ${module}-consumer COMMAND ${module}-consumer)
endforeach()

# Use the source inventory, but resolve each include through the imported target.
# This checks that the relocated package supplies every expected public header.
set(include_root "${CMAKE_CURRENT_LIST_DIR}/../../include")
file(GLOB_RECURSE headers RELATIVE "${include_root}" "${include_root}/opentlv/*.h")
if(CONSUMER_LANGUAGE STREQUAL "CXX")
    file(GLOB_RECURSE cpp_headers RELATIVE "${include_root}" "${include_root}/opentlv/*.hpp")
    list(APPEND headers ${cpp_headers})
endif()
foreach(header IN LISTS headers)
    if(header MATCHES "^opentlv/core/")
        set(module core)
    else()
        # The umbrella headers expose both modules.
        set(module io)
    endif()
    string(REPLACE "/" "_" probe "${header}")
    set(source "${CMAKE_CURRENT_BINARY_DIR}/${probe}.${probe_extension}")
    file(WRITE "${source}" "#include <${header}>\ntypedef int header_probe;\n")
    target_sources(${module}-consumer PRIVATE "${source}")
endforeach()
