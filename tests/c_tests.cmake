cmake_minimum_required(VERSION 3.16)

set(test_target test-${test_group}-tlv)

set(HEADERS
)

set(SOURCES
    architecture_test.cpp
    schema/failure_test.cpp
    attributes_c_test.c
    attributes_test.cpp
    builtins/asn1/asn1_codec_test.cpp
    builtins/asn1/ber_io_test.cpp
    builtins/asn1/cer_test.cpp
    builtins/asn1/cer_values_test.cpp
    builtins/asn1/der_schema_test.cpp
    builtins/asn1/der_test.cpp
    builtins/asn1/der_values_test.cpp
    builtins/asn1/format_ber_test.cpp
    builtins/bluetooth/ad_codec_test.cpp
    builtins/bluetooth/ad_data_test.cpp
    builtins/bluetooth/ad_schema_test.cpp
    builtins/bluetooth/ad_types_test.cpp
    builtins/bluetooth/company_ids_test.cpp
    builtins/bluetooth/format_bluetooth_ltv_conformance_test.cpp
    builtins/bluetooth/format_bluetooth_ltv_test.cpp
    builtins/bluetooth/manufacturer_data_test.cpp
    builtins/bluetooth/semantic_test.cpp
    builtins/bluetooth/service_data_test.cpp
    builtins/bluetooth/uuid_test.cpp
    builtins/dhcp/codec_test.cpp
    builtins/dhcp/container_test.cpp
    builtins/dhcp/dhcpv4_test.cpp
    builtins/dhcp/options_test.cpp
    builtins/emv/dol_test.cpp
    builtins/emv/emv_schema_test.cpp
    builtins/emv/emv_test.cpp
    builtins/emv/format_emv_test.cpp
    builtins/emv/tag_c_test.c
    builtins/lldp/codec_test.cpp
    builtins/lldp/lldp_test.cpp
    codec/codec_test.cpp
    codec/raw_result.c
    codec/ipv4_test.cpp
    codec/values_test.cpp
    codec/digits_test.cpp
    codec/text_test.cpp
    compiler_c_test.c
    compiler_test.cpp
    copy_test.cpp
    definition_test.cpp
    diagnostic_test.cpp
    document/document_test.cpp
    element_test.cpp
    endian_c_test.c
    endian_test.cpp
    format_init_test.cpp
    format_test.cpp
    invalid_enum_c_test.c
    invalid_enum_test.cpp
    formats/fixed_dhcp_options_test.cpp
    formats/fixed_io_test.cpp
    formats/fixed_field_adapters_test.cpp
    formats/format_fixed_test.cpp
    formats/packed_test.cpp
    formats/variable_test.cpp
    formats/escaped_test.cpp
    builtins/nfc/type2_test.cpp
    builtins/nfc/type2_vectors_test.cpp
    field/packed_test.cpp
    field/variable_test.cpp
    field/escaped_test.cpp
    field/fixed_test.cpp
    query/query_test.cpp
    query/program_test.cpp
    query/raw_result.c
    query/f2_test.cpp
    query/f3_test.cpp
    reader/reader_test.cpp
    reader/incremental_test.cpp
    reader/result_taxonomy_test.cpp
    reader/tree_test.cpp
    reader/visitor_test.cpp
    schema/constraint_test.cpp
    schema/schema_checked_test.cpp
    schema/schema_parity_test.cpp
    schema/schema_report_test.cpp
    schema/schema_report_c_test.c
    schema/schema_test.cpp
    size_test.cpp
    tag_test.cpp
    tagged_binary_test.cpp
    transformed_tag_test.cpp
    value_test.cpp
    versiontest.cpp
    writer/writer_test.cpp
    writer/tree_test.cpp
)

# A split source exists only in the group containing relevant cases.
foreach(source IN LISTS SOURCES)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${source}")
        list(REMOVE_ITEM SOURCES "${source}")
    endif()
endforeach()

# Generic Codec scenarios link only Codec and Core, including callback contracts.
if(test_group STREQUAL "unit")
    set(codec_sources)
    foreach(source IN LISTS SOURCES)
        if(source MATCHES "^codec/")
            list(APPEND codec_sources "${source}")
        endif()
    endforeach()
    list(REMOVE_ITEM SOURCES ${codec_sources})
    opentlv_add_module_test(test-codec OWNER codec SOURCES ${codec_sources} codec/diagnostic_test.cpp)
    target_link_libraries(test-codec PRIVATE GTest::gtest_main)
    target_include_directories(test-codec PRIVATE ${OpenTLV_SOURCE_DIR}/tests)
    opentlv_configure_compiler(test-codec)
    opentlv_copy_shared_runtime(test-codec)
    set_tests_properties(test-codec PROPERTIES LABELS "unit;module:codec")
endif()

# Extracted Schema scenarios retain their original assertions and test names.
if(test_group STREQUAL "unit")
    list(REMOVE_ITEM SOURCES schema/schema_test.cpp schema/constraint_test.cpp schema/failure_test.cpp)
    opentlv_add_module_test(test-schema OWNER schema SOURCES schema/schema_test.cpp schema/constraint_test.cpp)
    opentlv_add_module_test(test-schema-number OWNER schema-number SOURCES schema/number_test.cpp)
    opentlv_add_module_test(test-codec-structure OWNER codec-structure SOURCES schema/failure_test.cpp)
    if(OPENTLV_QUERY_FRONTEND)
        opentlv_add_module_test(test-schema-query OWNER schema-query-document SOURCES schema/query_test.cpp)
        target_link_libraries(test-schema-query PRIVATE GTest::gtest_main)
        target_include_directories(test-schema-query PRIVATE ${OpenTLV_SOURCE_DIR}/tests)
        opentlv_configure_compiler(test-schema-query)
        opentlv_copy_shared_runtime(test-schema-query)
        if(OPENTLV_BUILD_CXX)
            opentlv_add_module_test(test-schema-query-cxx OWNER schema-query-document SOURCES schema/query_cxx_test.cpp)
            target_include_directories(test-schema-query-cxx PRIVATE ${OpenTLV_SOURCE_DIR}/tests ${OpenTLV_SOURCE_DIR}/tlv++/include)
            target_link_libraries(test-schema-query-cxx PRIVATE GTest::gtest_main)
            target_compile_features(test-schema-query-cxx PRIVATE cxx_std_11)
            opentlv_configure_compiler(test-schema-query-cxx)
            opentlv_copy_shared_runtime(test-schema-query-cxx)
        endif()
    endif()
    foreach(target IN ITEMS test-schema test-schema-number test-codec-structure)
        target_link_libraries(${target} PRIVATE GTest::gtest_main)
        target_include_directories(${target} PRIVATE ${OpenTLV_SOURCE_DIR}/tests)
        opentlv_configure_compiler(${target})
        opentlv_copy_shared_runtime(${target})
    endforeach()
elseif(test_group STREQUAL "integration")
    set(schema_sources schema/schema_checked_test.cpp schema/schema_parity_test.cpp
        schema/schema_report_test.cpp schema/schema_report_c_test.c schema/schema_test.cpp)
    list(REMOVE_ITEM SOURCES ${schema_sources})
    opentlv_add_module_test(test-schema-integration OWNER schema SOURCES ${schema_sources})
    # BER is an explicit builtin fixture for generic Schema tests.
    target_link_libraries(test-schema-integration PRIVATE OpenTLV::ASN1 GTest::gtest_main)
    target_include_directories(test-schema-integration PRIVATE ${OpenTLV_SOURCE_DIR}/tests)
    opentlv_configure_compiler(test-schema-integration)
    opentlv_copy_shared_runtime(test-schema-integration)
endif()

# Runtime text scenarios belong to the Query frontend feature.
if(NOT OPENTLV_QUERY_FRONTEND)
    list(FILTER SOURCES EXCLUDE REGEX "^query/")
endif()

# Existing builtin scenarios are owned by their family. Additional targets
# provide explicit test fixtures; the aggregate is unused.
foreach(family IN ITEMS asn1 emv bluetooth nfc dhcp lldp)
    set(family_sources)
    foreach(source IN LISTS SOURCES)
        if(source MATCHES "^builtins/${family}/")
            list(APPEND family_sources "${source}")
        endif()
    endforeach()
    if(family_sources)
        list(REMOVE_ITEM SOURCES ${family_sources})
        set(family_target test-${test_group}-${family})
        set(family_dependencies io)
        if(family STREQUAL "emv" AND test_group STREQUAL "unit")
            list(APPEND family_dependencies asn1 schema-number)
        elseif(test_group STREQUAL "integration")
            if(family MATCHES "^(emv|nfc|dhcp|lldp)$")
                list(APPEND family_dependencies document query-document)
            endif()
            if(family STREQUAL "lldp")
                list(APPEND family_dependencies schema-number)
            endif()
        endif()
        opentlv_add_module_test(${family_target} OWNER ${family} SOURCES ${family_sources}
            DEPENDENCIES ${family_dependencies})
        target_link_libraries(${family_target} PRIVATE GTest::gtest_main)
        target_include_directories(${family_target} PRIVATE ${OpenTLV_SOURCE_DIR}/tests)
        if(family STREQUAL "nfc" AND test_group STREQUAL "integration")
            target_compile_definitions(${family_target} PRIVATE
                OPENTLV_NFC_TYPE2_CORPUS="${OpenTLV_SOURCE_DIR}/examples/nfc/type2")
        endif()
        opentlv_configure_compiler(${family_target})
        opentlv_copy_shared_runtime(${family_target})
        set_tests_properties(${family_target} PROPERTIES LABELS "${test_group};module:${family}")
    endif()
endforeach()

set(fixture_dependencies_core io document query query-document query-codec codec io-codec document-codec schema asn1 bluetooth emv nfc dhcp lldp)
set(fixture_dependencies_io core document query query-document codec codec-structure schema asn1 emv bluetooth nfc dhcp lldp)
set(fixture_dependencies_document core io query query-document codec document-codec asn1)
set(fixture_dependencies_query core io document query-document codec codec-structure query-codec schema schema-query-document asn1 emv)
set(fixture_dependencies_query-document core io document query codec query-codec asn1)
set(fixture_dependencies_codec core io io-codec document document-codec)
set(fixture_dependencies_asn1 core io emv bluetooth nfc dhcp lldp)
set(fixture_dependencies_nfc core io document query query-document)
set(fixture_dependencies_dhcp core io document query query-document)

# Preserve every scenario while assigning each source to one functional owner.
foreach(owner IN ITEMS core io document query query-document codec asn1 nfc dhcp)
    set(owned_sources)
    foreach(source IN LISTS SOURCES)
        set(source_owner core)
        if(source MATCHES "^(reader/|writer/|copy_test|architecture_test|layers_test|test_tlvpp|test_diagnostic|transformed_tag|formats/.*io_test|formats/test_custom_format)")
            set(source_owner io)
        elseif(source MATCHES "^document/")
            set(source_owner document)
        elseif(source MATCHES "^query/f3")
            set(source_owner query-document)
        elseif(source MATCHES "^query/")
            set(source_owner query)
        elseif(source MATCHES "^codec/")
            set(source_owner codec)
        elseif(source MATCHES "^builtins/nfc/")
            set(source_owner nfc)
        elseif(source MATCHES "^builtins/dhcp/")
            set(source_owner dhcp)
        elseif(source MATCHES "^builtins/")
            set(source_owner asn1)
        endif()
        if(source_owner STREQUAL owner)
            list(APPEND owned_sources ${source})
        endif()
    endforeach()
    if(NOT owned_sources)
        continue()
    endif()
    set(owned_target ${test_target}-${owner})
    opentlv_add_module_test(${owned_target} OWNER ${owner} SOURCES ${owned_sources}
        DEPENDENCIES ${fixture_dependencies_${owner}})
    target_include_directories(${owned_target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${OpenTLV_SOURCE_DIR}/tests)
    target_link_libraries(${owned_target} PRIVATE GTest::gtest_main)
    opentlv_configure_compiler(${owned_target})
    opentlv_copy_shared_runtime(${owned_target})
    set_tests_properties(${owned_target} PROPERTIES LABELS "${test_group};module:${owner}")
endforeach()
