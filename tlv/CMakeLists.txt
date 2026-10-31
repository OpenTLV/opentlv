cmake_minimum_required(VERSION 3.16)

project(tlv LANGUAGES C)

# The compatibility archive has no implementation symbols. Compiled modules
# independently follow OPENTLV_BUILD_SHARED_LIBS (an empty DLL is not portable).
set(_tlv_library_type STATIC)

include(modules.cmake)

# Compatibility aggregate: implementations are owned by independently linkable modules.
add_library(${PROJECT_NAME} ${_tlv_library_type}
    src/aggregate.c
)
target_link_libraries(${PROJECT_NAME} PUBLIC OpenTLV::Document OpenTLV::Generator OpenTLV::QueryDocument OpenTLV::QueryCodec OpenTLV::CodecStructure OpenTLV::SchemaNumber OpenTLV::SchemaQueryDocument
    OpenTLV::ASN1 OpenTLV::EMV OpenTLV::Bluetooth OpenTLV::NFC OpenTLV::DHCP OpenTLV::LLDP)
if(NOT CMAKE_INSTALL_RPATH)
    if(APPLE)
        set_property(TARGET ${PROJECT_NAME} PROPERTY INSTALL_RPATH "@loader_path")
    elseif(UNIX)
        set_property(TARGET ${PROJECT_NAME} PROPERTY INSTALL_RPATH "$ORIGIN")
    endif()
endif()


set_target_properties(${PROJECT_NAME} PROPERTIES
    C_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

include(GenerateExportHeader)
generate_export_header(${PROJECT_NAME}
    BASE_NAME TLV
    EXPORT_MACRO_NAME TLV_API
    EXPORT_FILE_NAME "${OpenTLV_BINARY_DIR}/generated/include/tlv/export.h"
)
if(_tlv_library_type STREQUAL "STATIC")
    target_compile_definitions(${PROJECT_NAME} PUBLIC TLV_STATIC_DEFINE)
endif()

target_include_directories(${PROJECT_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${OpenTLV_BINARY_DIR}/generated/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_compile_features(${PROJECT_NAME} PUBLIC
    c_std_99
)

opentlv_configure_compiler(${PROJECT_NAME})
