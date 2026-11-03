opentlv_example(c schema schema-visitor
    SOURCES schema_visitor.c
    LINK OpenTLV::Core OpenTLV::IO OpenTLV::Schema OpenTLV::ASN1
)

opentlv_example(c schema validate
    SOURCES validate.c
    LINK OpenTLV::Core OpenTLV::IO OpenTLV::Schema OpenTLV::ASN1
)

opentlv_example(cpp schema validate
    SOURCES validate.cpp
    LINK OpenTLV::Core OpenTLV::IO OpenTLV::Schema OpenTLV::ASN1
)
