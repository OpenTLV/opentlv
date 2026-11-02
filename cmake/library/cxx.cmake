# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Marek Cingel

opentlv_add_module(document-codec FACADE OWNER document EXPORT_NAME DocumentCodec
    INCLUDE_DIR ../../include CONSUMER ../../tests/modules/document_codec.cpp
    PUBLIC_HEADERS ../../include/opentlv/document/codec.hpp
    PUBLIC_DEPS document codec)
opentlv_add_module(io-codec FACADE OWNER codec EXPORT_NAME IOCodec
    INCLUDE_DIR ../../include CONSUMER ../../tests/modules/io_codec.cpp
    PUBLIC_HEADERS ../../include/opentlv/codec/writer.hpp
    PUBLIC_DEPS io codec)

opentlv_module_cxx(core CONSUMER tests/modules/core.cpp
    HEADERS
        opentlv/core/compat.hpp
        opentlv/core/definition.hpp
        opentlv/core/detail/format_access.hpp
        opentlv/core/diagnostic.hpp
        opentlv/core/error.hpp
        opentlv/core/format.hpp
        opentlv/core/format_traits.hpp
        opentlv/core/formats/fixed_format.hpp
        opentlv/core/formats/options.hpp
        opentlv/core/formats/runtime_fixed.hpp
        opentlv/core/types.hpp
        opentlv/core/version.hpp
)

opentlv_module_cxx(io CONSUMER tests/modules/io.cpp
    HEADERS
        opentlv/io/detail/visitor.hpp
        opentlv/io/reader/reader.hpp
        opentlv/io/reader/tree.hpp
        opentlv/io/tree.hpp
        opentlv/io/visitor.hpp
        opentlv/io/writer/builder.hpp
        opentlv/io/writer/tree.hpp
        opentlv/io/writer/value.hpp
        opentlv/io/writer/writer.hpp
)

opentlv_module_cxx(document CONSUMER tests/modules/document.cpp
    HEADERS
        opentlv/document/document.hpp
)

opentlv_module_cxx(query CONSUMER tests/modules/query.cpp
    HEADERS
        opentlv/query/builder.hpp
        opentlv/query/options.hpp
        opentlv/query/program.hpp
        opentlv/query/query.hpp
        opentlv/query/static.hpp
)

opentlv_module_cxx(schema CONSUMER tests/modules/schema.cpp
    HEADERS
        opentlv/schema/definition.hpp
        opentlv/schema/report.hpp
        opentlv/schema/schema.hpp
        opentlv/schema/error.hpp
)

opentlv_module_cxx(codec CONSUMER tests/modules/codec.cpp
    HEADERS
        opentlv/codec/dynamic.hpp
        opentlv/codec/registry.hpp
        opentlv/codec/typed.hpp
        opentlv/codec/typed_error.hpp
        opentlv/codec/detail/builtin_codec.hpp
)

opentlv_module_cxx(generator CONSUMER tests/modules/generator.cpp
    HEADERS
        opentlv/generator/generator.hpp
)

opentlv_module_cxx(query-document CONSUMER tests/modules/query_document.cpp
    HEADERS
        opentlv/query/document.hpp
        opentlv/query/diff.hpp
)

opentlv_module_cxx(query-codec CONSUMER tests/modules/query_codec.cpp
    HEADERS
        opentlv/query/codec.hpp
)

opentlv_module_cxx(codec-structure CONSUMER tests/modules/codec_structure.cpp
    HEADERS
        opentlv/codec/structure.hpp
)

opentlv_module_cxx(asn1 CONSUMER tests/modules/asn1.cpp
    HEADERS
        opentlv/builtin/asn1/ber.hpp
        opentlv/builtin/asn1/cer.hpp
        opentlv/builtin/asn1/codec.hpp
        opentlv/builtin/asn1/der.hpp
        opentlv/builtin/asn1/identifier.hpp
        opentlv/builtin/asn1/query.hpp
)

opentlv_module_cxx(emv CONSUMER tests/modules/emv.cpp
    HEADERS
        opentlv/builtin/emv/codec.hpp
        opentlv/builtin/emv/dictionary.hpp
        opentlv/builtin/emv/format.hpp
        opentlv/builtin/emv/query.hpp
    DEPENDENCIES io
)

opentlv_module_cxx(bluetooth CONSUMER tests/modules/bluetooth.cpp
    HEADERS
        opentlv/builtin/bluetooth/codec.hpp
        opentlv/builtin/bluetooth/ltv.hpp
        opentlv/builtin/bluetooth/metadata.hpp
)

opentlv_module_cxx(nfc CONSUMER tests/modules/nfc.cpp
    HEADERS
        opentlv/builtin/nfc/type2.hpp
    DEPENDENCIES io
)

opentlv_module_cxx(dhcp CONSUMER tests/modules/dhcp.cpp
    HEADERS
        opentlv/builtin/dhcp/codec.hpp
        opentlv/builtin/dhcp/container.hpp
        opentlv/builtin/dhcp/dhcpv4.hpp
)

opentlv_module_cxx(lldp CONSUMER tests/modules/lldp.cpp
    HEADERS
        opentlv/builtin/lldp/codec.hpp
        opentlv/builtin/lldp/lldp.hpp
)
