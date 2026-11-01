# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Marek Cingel

opentlv_add_module(document-codec FACADE OWNER document EXPORT_NAME DocumentCodec
    INCLUDE_DIR ../tlv++/include CONSUMER ../tests/modules/document_codec.cpp
    PUBLIC_HEADERS ../tlv++/include/tlv++/document/codec.hpp
    PUBLIC_DEPS document codec)
opentlv_add_module(io-codec FACADE OWNER codec EXPORT_NAME IOCodec
    INCLUDE_DIR ../tlv++/include CONSUMER ../tests/modules/io_codec.cpp
    PUBLIC_HEADERS ../tlv++/include/tlv++/codec/writer.hpp
    PUBLIC_DEPS io codec)

opentlv_module_cxx(core CONSUMER tests/modules/core.cpp
    HEADERS
        tlv++/compat.hpp
        tlv++/definition.hpp
        tlv++/detail/format_access.hpp
        tlv++/diagnostic.hpp
        tlv++/error.hpp
        tlv++/format.hpp
        tlv++/format_traits.hpp
        tlv++/formats/fixed_format.hpp
        tlv++/formats/options.hpp
        tlv++/formats/runtime_fixed.hpp
        tlv++/types.hpp
        tlv++/version.hpp
)

opentlv_module_cxx(io CONSUMER tests/modules/io.cpp
    HEADERS
        tlv++/detail/visitor.hpp
        tlv++/reader/reader.hpp
        tlv++/reader/tree.hpp
        tlv++/tree.hpp
        tlv++/visitor.hpp
        tlv++/writer/builder.hpp
        tlv++/writer/tree.hpp
        tlv++/writer/value.hpp
        tlv++/writer/writer.hpp
)

opentlv_module_cxx(document CONSUMER tests/modules/document.cpp
    HEADERS
        tlv++/document/document.hpp
)

opentlv_module_cxx(query CONSUMER tests/modules/query.cpp
    HEADERS
        tlv++/query/builder.hpp
        tlv++/query/options.hpp
        tlv++/query/program.hpp
        tlv++/query/query.hpp
        tlv++/query/static.hpp
)

opentlv_module_cxx(schema CONSUMER tests/modules/schema.cpp
    HEADERS
        tlv++/schema/definition.hpp
        tlv++/schema/report.hpp
        tlv++/schema/schema.hpp
        tlv++/schema/error.hpp
)

opentlv_module_cxx(codec CONSUMER tests/modules/codec.cpp
    HEADERS
        tlv++/codec/dynamic.hpp
        tlv++/codec/registry.hpp
        tlv++/codec/typed.hpp
        tlv++/codec/typed_error.hpp
        tlv++/detail/builtin_codec.hpp
)

opentlv_module_cxx(generator CONSUMER tests/modules/generator.cpp
    HEADERS
        tlv++/generator.hpp
)

opentlv_module_cxx(query-document CONSUMER tests/modules/query_document.cpp
    HEADERS
        tlv++/query/document.hpp
        tlv++/document/diff.hpp
)

opentlv_module_cxx(query-codec CONSUMER tests/modules/query_codec.cpp
    HEADERS
        tlv++/query/codec.hpp
)

opentlv_module_cxx(codec-structure CONSUMER tests/modules/codec_structure.cpp
    HEADERS
        tlv++/codec/structure.hpp
)

opentlv_module_cxx(asn1 CONSUMER tests/modules/asn1.cpp
    HEADERS
        tlv++/builtins/asn1/ber.hpp
        tlv++/builtins/asn1/cer.hpp
        tlv++/builtins/asn1/codec.hpp
        tlv++/builtins/asn1/der.hpp
        tlv++/builtins/asn1/identifier.hpp
        tlv++/builtins/asn1/query.hpp
)

opentlv_module_cxx(emv CONSUMER tests/modules/emv.cpp
    HEADERS
        tlv++/builtins/emv/codec.hpp
        tlv++/builtins/emv/dictionary.hpp
        tlv++/builtins/emv/format.hpp
        tlv++/builtins/emv/query.hpp
    DEPENDENCIES io
)

opentlv_module_cxx(bluetooth CONSUMER tests/modules/bluetooth.cpp
    HEADERS
        tlv++/builtins/bluetooth/codec.hpp
        tlv++/builtins/bluetooth/ltv.hpp
        tlv++/builtins/bluetooth/metadata.hpp
)

opentlv_module_cxx(nfc CONSUMER tests/modules/nfc.cpp
    HEADERS
        tlv++/builtins/nfc/type2.hpp
    DEPENDENCIES io
)

opentlv_module_cxx(dhcp CONSUMER tests/modules/dhcp.cpp
    HEADERS
        tlv++/builtins/dhcp/codec.hpp
        tlv++/builtins/dhcp/container.hpp
        tlv++/builtins/dhcp/dhcpv4.hpp
)

opentlv_module_cxx(lldp CONSUMER tests/modules/lldp.cpp
    HEADERS
        tlv++/builtins/lldp/codec.hpp
        tlv++/builtins/lldp/lldp.hpp
)
