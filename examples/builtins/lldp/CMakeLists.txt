opentlv_example(c lldp lldp
    SOURCES lldp.c
    LINK OpenTLV::Core OpenTLV::IO OpenTLV::LLDP OpenTLV::Schema
)

opentlv_example(cpp lldp lldp
    SOURCES lldp.cpp
    LINK OpenTLV::Core OpenTLV::IO OpenTLV::LLDP OpenTLV::Schema
)
