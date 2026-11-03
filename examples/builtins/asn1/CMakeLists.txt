opentlv_example(c asn1 ber
    SOURCES ber.c
    LINK OpenTLV::Core OpenTLV::IO OpenTLV::ASN1
)

opentlv_example(c asn1 cer
    SOURCES cer.c
    LINK OpenTLV::Core OpenTLV::IO OpenTLV::ASN1
)
