opentlv_example(c query query
    SOURCES query.c
    LINK OpenTLV::Core OpenTLV::Query OpenTLV::ASN1
    QUERY_FRONTEND
)

opentlv_example(cpp query query
    SOURCES query.cpp
    LINK OpenTLV::Core OpenTLV::IO OpenTLV::Document OpenTLV::Query OpenTLV::QueryDocument OpenTLV::ASN1
    QUERY_FRONTEND
)
