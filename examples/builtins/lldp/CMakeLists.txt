opentlv_example(c lldp lldp
    SOURCES lldp.c
    LINK OpenTLV::Core OpenTLV::IO OpenTLV::Codec OpenTLV::LLDP
)

opentlv_example(cpp lldp lldp
    SOURCES lldp.cpp
    LINK OpenTLV::Codec OpenTLV::LLDP
)
