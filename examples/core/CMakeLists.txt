opentlv_example(c core custom-format
    SOURCES custom_format.c
    LINK OpenTLV::Core OpenTLV::IO
)

opentlv_example(c core fixed-format
    SOURCES fixed_format.c
    LINK OpenTLV::Core OpenTLV::IO
)

opentlv_example(cpp core custom-format
    SOURCES custom_format.cpp
    LINK OpenTLV::Core OpenTLV::IO
)

opentlv_example(cpp core fixed-format
    SOURCES fixed_format.cpp
    LINK OpenTLV::Core OpenTLV::IO
)

opentlv_example(cpp core fixed-format-runtime
    SOURCES fixed_format_runtime.cpp
    LINK OpenTLV::Core OpenTLV::IO
)
