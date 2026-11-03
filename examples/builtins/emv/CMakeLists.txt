opentlv_example(c emv tag-decoding
    SOURCES tag_decoding.c
    LINK OpenTLV::Core OpenTLV::IO OpenTLV::Codec OpenTLV::EMV
)
