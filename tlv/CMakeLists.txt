cmake_minimum_required(VERSION 3.16)

project(tlv LANGUAGES C)

set(_tlv_library_type STATIC)
if(OPENTLV_BUILD_SHARED_LIBS)
    set(_tlv_library_type SHARED)
endif()

include(modules.cmake)

# Transitional library for modules extracted by subsequent issues. Extracted modules
# are linked once; no implementation is duplicated in the compatibility target.
add_library(${PROJECT_NAME} ${_tlv_library_type}
    src/schema/diagnostic_strings.c
)
target_link_libraries(${PROJECT_NAME} PUBLIC OpenTLV::Document OpenTLV::Generator OpenTLV::QueryDocument OpenTLV::QueryCodec)
if(NOT CMAKE_INSTALL_RPATH)
    if(APPLE)
        set_property(TARGET ${PROJECT_NAME} PROPERTY INSTALL_RPATH "@loader_path")
    elseif(UNIX)
        set_property(TARGET ${PROJECT_NAME} PROPERTY INSTALL_RPATH "$ORIGIN")
    endif()
endif()


if(OPENTLV_SCHEMA)
    target_sources(${PROJECT_NAME} PRIVATE
        src/schema/schema.c
        src/schema/constraint.c
    )
endif()

# Codec-owned codec-structure integration. Schema extraction (#652) registers
# its separate target; the base Codec never links this transitional aggregate.
if(OPENTLV_CODEC AND OPENTLV_SCHEMA)
    target_sources(${PROJECT_NAME} PRIVATE src/codec/structure.c)
endif()

if(OPENTLV_SCHEMA AND OPENTLV_CODEC)
    target_sources(${PROJECT_NAME} PRIVATE src/schema/number.c)
endif()
if(OPENTLV_SCHEMA)
    target_sources(${PROJECT_NAME} PRIVATE src/schema/structure.c src/schema/report.c)
endif()
if(OPENTLV_SCHEMA AND OPENTLV_QUERY)
    target_sources(${PROJECT_NAME} PRIVATE src/schema/query.c)
endif()


# Builtin framing and optional protocol integrations.
if(OPENTLV_NFC)
    target_sources(${PROJECT_NAME} PRIVATE
        src/builtins/nfc/type2.c
    )
endif()

if(OPENTLV_DHCP)
    target_sources(${PROJECT_NAME} PRIVATE
        src/builtins/dhcp/dhcpv4.c
    )
endif()

if(OPENTLV_DHCP AND OPENTLV_CODEC)
    target_sources(${PROJECT_NAME} PRIVATE
        src/builtins/dhcp/options.c
    )
endif()

if(OPENTLV_DHCP)
    target_sources(${PROJECT_NAME} PRIVATE
        src/builtins/dhcp/container.c
    )
endif()

if(OPENTLV_LLDP)
    target_sources(${PROJECT_NAME} PRIVATE
        src/builtins/lldp/lldp.c
    )
endif()

if(OPENTLV_LLDP AND OPENTLV_CODEC)
    target_sources(${PROJECT_NAME} PRIVATE
        src/builtins/lldp/codec.c
    )
endif()

if(OPENTLV_LLDP AND OPENTLV_SCHEMA)
    target_sources(${PROJECT_NAME} PRIVATE
        src/builtins/lldp/schema.c
    )
endif()

if(OPENTLV_BLUETOOTH)
    target_sources(${PROJECT_NAME} PRIVATE
        src/builtins/bluetooth/ad_types.c
        src/builtins/bluetooth/company_ids.c
        src/builtins/bluetooth/bluetooth_ltv.c
    )
endif()

if(OPENTLV_BLUETOOTH)
    target_sources(${PROJECT_NAME} PRIVATE
        src/builtins/bluetooth/ad_data.c
    )
endif()

if(OPENTLV_BLUETOOTH AND OPENTLV_SCHEMA)
    target_sources(${PROJECT_NAME} PRIVATE
        src/builtins/bluetooth/ad_schema.c
    )
endif()

if(OPENTLV_BLUETOOTH AND OPENTLV_CODEC)
    target_sources(${PROJECT_NAME} PRIVATE
        src/builtins/bluetooth/ad_codec.c
        src/builtins/bluetooth/uuid.c
        src/builtins/bluetooth/service_data.c
        src/builtins/bluetooth/manufacturer_data.c
    )
endif()

if(OPENTLV_FORMAT_BER)
    target_sources(${PROJECT_NAME} PRIVATE
        src/builtins/asn1/ber.c
        src/builtins/asn1/identifier.c
        src/builtins/asn1/ber_internal.c
        src/builtins/asn1/asn1_values_internal.c
    )
endif()

if(OPENTLV_FORMAT_BER AND OPENTLV_CODEC)
    target_sources(${PROJECT_NAME} PRIVATE
        src/builtins/asn1/asn1_codec.c
    )
endif()

if(OPENTLV_FORMAT_BER AND OPENTLV_QUERY AND OPENTLV_CODEC)
    target_sources(${PROJECT_NAME} PRIVATE
        src/builtins/asn1/query.c
    )
endif()

if(OPENTLV_FORMAT_DER OR OPENTLV_FORMAT_CER)
    target_sources(${PROJECT_NAME} PRIVATE
        src/builtins/asn1/asn1_internal.c
    )
endif()

if(OPENTLV_FORMAT_DER)
    target_sources(${PROJECT_NAME} PRIVATE
        src/builtins/asn1/der.c
        src/builtins/asn1/der_validation.c
    )
endif()

if(OPENTLV_FORMAT_DER AND OPENTLV_SCHEMA AND OPENTLV_CODEC)
    target_sources(${PROJECT_NAME} PRIVATE
        src/builtins/asn1/der_schema.c
    )
endif()

if(OPENTLV_FORMAT_CER)
    target_sources(${PROJECT_NAME} PRIVATE
        src/builtins/asn1/cer.c
        src/builtins/asn1/cer_validation.c
        src/builtins/asn1/cer_values_internal.c
    )
endif()

if(OPENTLV_EMV)
    target_sources(${PROJECT_NAME} PRIVATE
        src/builtins/emv/format.c
        src/builtins/emv/dol.c
    )
endif()

if(OPENTLV_EMV AND OPENTLV_SCHEMA AND OPENTLV_CODEC)
    target_sources(${PROJECT_NAME} PRIVATE
        src/builtins/emv/emv.c
        src/builtins/emv/dictionary.c
        src/builtins/emv/emv_codec.c
        src/builtins/emv/presentation.c
        src/builtins/emv/emv_schema.c
    )
endif()

if(OPENTLV_EMV AND OPENTLV_SCHEMA AND OPENTLV_CODEC AND OPENTLV_QUERY)
    target_sources(${PROJECT_NAME} PRIVATE
        src/builtins/emv/query.c
    )
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
if(NOT OPENTLV_BUILD_SHARED_LIBS)
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
