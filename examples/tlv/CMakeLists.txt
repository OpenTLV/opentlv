cmake_minimum_required(VERSION 3.16)

project(example-tlv LANGUAGES C)

add_executable(example-tlv-tree-writer src/tree_writer.c)
target_link_libraries(example-tlv-tree-writer PRIVATE OpenTLV::Core OpenTLV::IO)
opentlv_configure_compiler(example-tlv-tree-writer)
opentlv_copy_shared_runtime(example-tlv-tree-writer)
add_test(NAME Integration_example_tlv_tree_writer COMMAND $<TARGET_FILE:example-tlv-tree-writer>)
set_tests_properties(Integration_example_tlv_tree_writer PROPERTIES LABELS "integration;examples")

add_executable(example-tlv-incremental-reader src/incremental_reader.c)
target_link_libraries(example-tlv-incremental-reader PRIVATE OpenTLV::Core OpenTLV::IO)
opentlv_configure_compiler(example-tlv-incremental-reader)
opentlv_copy_shared_runtime(example-tlv-incremental-reader)
add_test(NAME Integration_example_tlv_incremental_reader COMMAND $<TARGET_FILE:example-tlv-incremental-reader>)
set_tests_properties(Integration_example_tlv_incremental_reader PROPERTIES LABELS "integration;examples")

add_executable(example-tlv-tree-reader src/tree_reader.c)
target_link_libraries(example-tlv-tree-reader PRIVATE OpenTLV::Core OpenTLV::IO)
opentlv_configure_compiler(example-tlv-tree-reader)
opentlv_copy_shared_runtime(example-tlv-tree-reader)
add_test(NAME Integration_example_tlv_tree_reader COMMAND $<TARGET_FILE:example-tlv-tree-reader>)
set_tests_properties(Integration_example_tlv_tree_reader PROPERTIES LABELS "integration;examples")

add_executable(example-tlv-visitor src/visitor.c)
target_link_libraries(example-tlv-visitor PRIVATE OpenTLV::Core OpenTLV::IO)
opentlv_configure_compiler(example-tlv-visitor)
opentlv_copy_shared_runtime(example-tlv-visitor)
add_test(NAME Integration_example_tlv_visitor COMMAND $<TARGET_FILE:example-tlv-visitor>)
set_tests_properties(Integration_example_tlv_visitor PROPERTIES LABELS "integration;examples")

add_executable(example-tlv-nfc_type2 src/nfc_type2.c)
target_link_libraries(example-tlv-nfc_type2 PRIVATE OpenTLV::Core OpenTLV::IO OpenTLV::NFC OpenTLV::Document)
opentlv_configure_compiler(example-tlv-nfc_type2)
opentlv_copy_shared_runtime(example-tlv-nfc_type2)
add_test(NAME Integration_example_tlv_nfc_type2 COMMAND $<TARGET_FILE:example-tlv-nfc_type2>)
set_tests_properties(Integration_example_tlv_nfc_type2 PROPERTIES LABELS "integration;examples")
add_executable(example-tlv-dhcpv4 src/dhcpv4.c)
target_link_libraries(example-tlv-dhcpv4 PRIVATE OpenTLV::Core OpenTLV::IO OpenTLV::DHCP OpenTLV::Codec)
opentlv_configure_compiler(example-tlv-dhcpv4)
opentlv_copy_shared_runtime(example-tlv-dhcpv4)
add_test(NAME Integration_example_tlv_dhcpv4 COMMAND $<TARGET_FILE:example-tlv-dhcpv4>)
set_tests_properties(Integration_example_tlv_dhcpv4 PROPERTIES LABELS "integration;examples")
add_executable(example-tlv-lldp src/lldp.c)
target_link_libraries(example-tlv-lldp PRIVATE OpenTLV::Core OpenTLV::IO OpenTLV::LLDP OpenTLV::Schema)
opentlv_configure_compiler(example-tlv-lldp)
opentlv_copy_shared_runtime(example-tlv-lldp)
add_test(NAME Integration_example_tlv_lldp COMMAND $<TARGET_FILE:example-tlv-lldp>)
set_tests_properties(Integration_example_tlv_lldp PROPERTIES LABELS "integration;examples")

# Backs the C example in the README and Getting Started; scripts/check_doc_examples.py
# keeps the Markdown copies identical to this file.
add_executable(example-tlv-quick-start src/quick_start.c)
target_link_libraries(example-tlv-quick-start PRIVATE OpenTLV::Core OpenTLV::IO)
opentlv_configure_compiler(example-tlv-quick-start)
opentlv_copy_shared_runtime(example-tlv-quick-start)
add_test(NAME Integration_example_tlv_quick_start COMMAND $<TARGET_FILE:example-tlv-quick-start>)
set_tests_properties(Integration_example_tlv_quick_start PROPERTIES
    LABELS "integration;examples" PASS_REGULAR_EXPRESSION "Hello, world!")

# Use-case examples shared across languages: the same document is parsed,
# written, queried and validated in examples/tlv, examples/tlv++ and the
# opentlv-rust examples.
set(example_parse OpenTLV::Core OpenTLV::IO OpenTLV::ASN1)
set(example_write OpenTLV::Core OpenTLV::IO OpenTLV::ASN1)
set(example_query OpenTLV::Core OpenTLV::IO OpenTLV::Query OpenTLV::QueryDocument OpenTLV::Document OpenTLV::ASN1)
set(example_validate OpenTLV::Core OpenTLV::IO OpenTLV::Schema OpenTLV::ASN1)
foreach(use_case parse write query validate)
    set(example_dependencies ${example_${use_case}})
    if(use_case STREQUAL "query" AND NOT OPENTLV_QUERY_FRONTEND)
        continue()
    endif()
    add_executable(example-tlv-${use_case} src/${use_case}.c)
    target_link_libraries(example-tlv-${use_case} PRIVATE ${example_dependencies})
    opentlv_configure_compiler(example-tlv-${use_case})
    opentlv_copy_shared_runtime(example-tlv-${use_case})
    add_test(NAME Integration_example_tlv_${use_case} COMMAND $<TARGET_FILE:example-tlv-${use_case}>)
    set_tests_properties(Integration_example_tlv_${use_case} PROPERTIES LABELS "integration;examples")
endforeach()

# One topic per example, each its own small self-checking program.
set(example_sequential_io OpenTLV::Core OpenTLV::IO OpenTLV::ASN1)
set(example_copies OpenTLV::Core OpenTLV::IO OpenTLV::ASN1)
set(example_schema_visitor OpenTLV::Core OpenTLV::IO OpenTLV::Schema OpenTLV::ASN1)
set(example_codecs_and_endian OpenTLV::Core OpenTLV::Codec OpenTLV::ASN1)
set(example_custom_format OpenTLV::Core OpenTLV::IO)
foreach(topic sequential_io copies schema_visitor codecs_and_endian custom_format)
    set(example_dependencies ${example_${topic}})
    string(REPLACE "_" "-" topic_dashed ${topic})
    add_executable(example-tlv-${topic_dashed} src/${topic}.c)
    target_link_libraries(example-tlv-${topic_dashed} PRIVATE ${example_dependencies})
    opentlv_configure_compiler(example-tlv-${topic_dashed})
    opentlv_copy_shared_runtime(example-tlv-${topic_dashed})
    add_test(NAME Integration_example_tlv_${topic} COMMAND $<TARGET_FILE:example-tlv-${topic_dashed}>)
    set_tests_properties(Integration_example_tlv_${topic} PROPERTIES LABELS "integration;examples")
endforeach()

# Builtin-specific examples: one per wire format.
add_executable(example-tlv-builtins-asn1-ber src/builtins/asn1/ber.c)
target_link_libraries(example-tlv-builtins-asn1-ber PRIVATE OpenTLV::Core OpenTLV::IO OpenTLV::ASN1)
opentlv_configure_compiler(example-tlv-builtins-asn1-ber)
opentlv_copy_shared_runtime(example-tlv-builtins-asn1-ber)
add_test(NAME Integration_example_tlv_builtins_asn1_ber COMMAND $<TARGET_FILE:example-tlv-builtins-asn1-ber>)
set_tests_properties(Integration_example_tlv_builtins_asn1_ber PROPERTIES LABELS "integration;examples")

add_executable(example-tlv-builtins-asn1-cer src/builtins/asn1/cer.c)
target_link_libraries(example-tlv-builtins-asn1-cer PRIVATE OpenTLV::Core OpenTLV::IO OpenTLV::ASN1)
opentlv_configure_compiler(example-tlv-builtins-asn1-cer)
opentlv_copy_shared_runtime(example-tlv-builtins-asn1-cer)
add_test(NAME Integration_example_tlv_builtins_asn1_cer COMMAND $<TARGET_FILE:example-tlv-builtins-asn1-cer>)
set_tests_properties(Integration_example_tlv_builtins_asn1_cer PROPERTIES LABELS "integration;examples")

# Generic-format example: a runtime-configurable fixed-width format.
# Fixed is always available as part of the generic core.
add_executable(example-tlv-formats-fixed-format src/formats/fixed_format.c)
target_link_libraries(example-tlv-formats-fixed-format PRIVATE OpenTLV::Core OpenTLV::IO)
opentlv_configure_compiler(example-tlv-formats-fixed-format)
opentlv_copy_shared_runtime(example-tlv-formats-fixed-format)
add_test(NAME Integration_example_tlv_formats_fixed_format
    COMMAND $<TARGET_FILE:example-tlv-formats-fixed-format>)
set_tests_properties(Integration_example_tlv_formats_fixed_format PROPERTIES LABELS "integration;examples")

# Builtin-specific example: traverses an EMV template with the EMV dictionary.
# Links the EMV module and the IO, Codec and Schema APIs used by the example.
add_executable(example-tlv-builtins-emv-tag-decoding src/builtins/emv/tag_decoding.c)
target_include_directories(example-tlv-builtins-emv-tag-decoding PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/builtins/emv
)
target_link_libraries(example-tlv-builtins-emv-tag-decoding PRIVATE OpenTLV::Core OpenTLV::IO OpenTLV::EMV OpenTLV::Codec OpenTLV::Schema)
opentlv_configure_compiler(example-tlv-builtins-emv-tag-decoding)
opentlv_copy_shared_runtime(example-tlv-builtins-emv-tag-decoding)

add_test(NAME Integration_example_tlv_builtins_emv_tag_decoding
    COMMAND $<TARGET_FILE:example-tlv-builtins-emv-tag-decoding>)
set_tests_properties(Integration_example_tlv_builtins_emv_tag_decoding PROPERTIES LABELS "integration;examples")
