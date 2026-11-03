opentlv_example(c io copies
    SOURCES copies.c
    LINK OpenTLV::Core OpenTLV::IO
)

opentlv_example(c io incremental-reader
    SOURCES incremental_reader.c
    LINK OpenTLV::Core OpenTLV::IO
)

opentlv_example(c io parse
    SOURCES parse.c
    LINK OpenTLV::Core OpenTLV::IO OpenTLV::ASN1
)

opentlv_example(c io quick-start
    SOURCES quick_start.c
    LINK OpenTLV::Core OpenTLV::IO
    PASS_REGULAR_EXPRESSION "Hello, world!"
)

opentlv_example(c io sequential-io
    SOURCES sequential_io.c
    LINK OpenTLV::Core OpenTLV::IO OpenTLV::ASN1
)

opentlv_example(c io tree-reader
    SOURCES tree_reader.c
    LINK OpenTLV::Core OpenTLV::IO
)

opentlv_example(c io tree-writer
    SOURCES tree_writer.c
    LINK OpenTLV::Core OpenTLV::IO
)

opentlv_example(c io visitor
    SOURCES visitor.c
    LINK OpenTLV::Core OpenTLV::IO
)

opentlv_example(c io write
    SOURCES write.c
    LINK OpenTLV::IO OpenTLV::ASN1
)

opentlv_example(cpp io advanced-control
    SOURCES advanced_control.cpp
    LINK OpenTLV::Core OpenTLV::IO OpenTLV::Document OpenTLV::Query OpenTLV::Schema OpenTLV::Codec OpenTLV::IOCodec
    NATIVE_INTEROP
)

opentlv_example(cpp io basic-usage
    SOURCES basic_usage.cpp
    LINK OpenTLV::IO OpenTLV::ASN1
)

opentlv_example(cpp io parse
    SOURCES parse.cpp
    LINK OpenTLV::IO OpenTLV::ASN1
)

opentlv_example(cpp io quick-start
    SOURCES quick_start.cpp
    LINK OpenTLV::tlv
    UMBRELLA
    PASS_REGULAR_EXPRESSION "Hello, world!"
)

opentlv_example(cpp io write
    SOURCES write.cpp
    LINK OpenTLV::IO OpenTLV::ASN1
)
