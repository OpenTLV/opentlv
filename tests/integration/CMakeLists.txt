include(${OpenTLV_SOURCE_DIR}/tests/c_tests.cmake)
if(TARGET opentlv_emv)
    # Source isolation is checked in the conventional build, not by external
    # distribution consumers (which must not compile replacement modules).
    include(${OpenTLV_SOURCE_DIR}/tests/emv_without_presentation.cmake)
endif()
if(OPENTLV_BUILD_CXX)
    include(${OpenTLV_SOURCE_DIR}/tests/cxx_tests.cmake)
endif()
