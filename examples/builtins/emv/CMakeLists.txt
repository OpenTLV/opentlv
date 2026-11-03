opentlv_example(c emv tag-decoding
    SOURCES tag_decoding.c
    LINK OpenTLV::Core OpenTLV::IO OpenTLV::EMV OpenTLV::Codec OpenTLV::Schema
)
