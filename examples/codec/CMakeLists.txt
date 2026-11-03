opentlv_example(c codec codecs-and-endian
    SOURCES codecs_and_endian.c
    LINK OpenTLV::Core OpenTLV::Codec OpenTLV::ASN1
)

opentlv_example(cpp codec typed-fields
    SOURCES typed_fields.cpp
    LINK OpenTLV::Core OpenTLV::IO OpenTLV::Codec OpenTLV::IOCodec OpenTLV::Document OpenTLV::DocumentCodec
)
