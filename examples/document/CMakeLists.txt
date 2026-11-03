opentlv_example(cpp document document
    SOURCES document.cpp
    LINK OpenTLV::Core OpenTLV::IO OpenTLV::Document OpenTLV::QueryDocument OpenTLV::Query OpenTLV::ASN1
)

opentlv_example(cpp document document-edit
    SOURCES document_edit.cpp
    LINK OpenTLV::Core OpenTLV::IO OpenTLV::Document OpenTLV::QueryDocument OpenTLV::Query OpenTLV::ASN1
)
