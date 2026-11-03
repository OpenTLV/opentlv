opentlv_example(c codec codecs-and-endian
    SOURCES codecs_and_endian.c
    LINK OpenTLV::Core OpenTLV::IO OpenTLV::Codec
)

opentlv_example(cpp codec typed-fields
    SOURCES typed_fields.cpp
    LINK OpenTLV::Core OpenTLV::IO OpenTLV::Document OpenTLV::Codec OpenTLV::DocumentCodec OpenTLV::IOCodec
)
