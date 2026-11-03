opentlv_example(c nfc type2
    SOURCES type2.c
    LINK OpenTLV::IO OpenTLV::NFC
    ARGS "${CMAKE_CURRENT_SOURCE_DIR}/sample.bin"
)
