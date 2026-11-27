cmake_minimum_required(VERSION 3.15)
project(OpenTLV VERSION 0.1.0 LANGUAGES C)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    option(BUILD_TESTING "Build OpenTLV tests" OFF)
else()
    option(OPENTLV_BUILD_TESTS "Build OpenTLV tests in the parent project" OFF)
    # Scope the test setting to OpenTLV and its test and benchmark directories.
    set(BUILD_TESTING "${OPENTLV_BUILD_TESTS}")
endif()
option(OPENTLV_BUILD_BENCHMARKS "Build the Fixed TLV benchmark runner and fixture checker" OFF)
option(OPENTLV_BUILD_CALLGRIND "Build the Linux Fixed TLV instruction-count probe" OFF)
option(OPENTLV_BUILD_ALLOCATION_CHECKS "Check Fixed TLV calls for heap allocation with GDB" OFF)
option(OPENTLV_BUILD_FOOTPRINT "Measure Fixed TLV libraries and linked consumers" OFF)

add_library(opentlv_core
    src/core/fixed.c
    src/core/result.c
    src/core/size.c
    src/core/value.c
)
# CMake 3.15 rejects C++ compile features when the consumer enables only C.
target_compile_features(opentlv_core INTERFACE "$<$<BOOL:$<CXX_COMPILER_ID>>:cxx_std_17>")
add_library(opentlv_io
    src/io/reader/reader.c
    src/io/writer/writer.c
)
target_link_libraries(opentlv_io PUBLIC opentlv_core)

foreach(module IN ITEMS core io)
    set(target opentlv_${module})
    add_library(OpenTLV::${module} ALIAS ${target})
    set_target_properties(${target} PROPERTIES
        EXPORT_NAME ${module}
        C_STANDARD 99
        C_STANDARD_REQUIRED YES
        C_EXTENSIONS NO
    )
    target_include_directories(${target} PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
    )

    if(BUILD_SHARED_LIBS)
        string(TOUPPER "${module}" module_upper)
        set(api OPENTLV_${module_upper}_API)
        if(WIN32)
            target_compile_definitions(${target}
                PRIVATE "${api}=__declspec(dllexport)"
                INTERFACE "${api}=__declspec(dllimport)"
            )
        elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
            set_target_properties(${target} PROPERTIES C_VISIBILITY_PRESET hidden)
            target_compile_definitions(${target} PUBLIC
                "${api}=__attribute__((visibility(\"default\")))"
            )
        endif()
    endif()
endforeach()

# The installed IO library loads Core from the same library directory.
if(BUILD_SHARED_LIBS AND UNIX AND NOT APPLE)
    set_target_properties(opentlv_io PROPERTIES INSTALL_RPATH "$ORIGIN")
endif()

install(TARGETS opentlv_core opentlv_io EXPORT OpenTLVTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)
install(DIRECTORY include/opentlv DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)
install(FILES LICENSE DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/OpenTLV")
set(opentlv_package_dir "${CMAKE_INSTALL_LIBDIR}/cmake/OpenTLV")
write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/OpenTLVConfigVersion.cmake"
    VERSION "${PROJECT_VERSION}"
    COMPATIBILITY ExactVersion
)
export(EXPORT OpenTLVTargets
    FILE "${CMAKE_CURRENT_BINARY_DIR}/OpenTLVConfig.cmake"
    NAMESPACE OpenTLV::
)
install(EXPORT OpenTLVTargets
    FILE OpenTLVConfig.cmake
    NAMESPACE OpenTLV::
    DESTINATION "${opentlv_package_dir}"
)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/OpenTLVConfigVersion.cmake"
    DESTINATION "${opentlv_package_dir}"
)

if(OPENTLV_BUILD_BENCHMARKS OR OPENTLV_BUILD_CALLGRIND
        OR OPENTLV_BUILD_ALLOCATION_CHECKS OR OPENTLV_BUILD_FOOTPRINT)
    set(opentlv_measurements_enabled ON)
else()
    set(opentlv_measurements_enabled OFF)
endif()
if(BUILD_TESTING OR opentlv_measurements_enabled)
    enable_language(CXX)
    enable_testing()
endif()
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
if(opentlv_measurements_enabled)
    add_subdirectory(benchmarks)
endif()
