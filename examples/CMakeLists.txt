cmake_minimum_required(VERSION 3.16)

project(examples LANGUAGES C)

# Each example declares the functional modules it uses.
add_subdirectory(tlv)

if(OPENTLV_BUILD_CXX)
    add_subdirectory(tlv++)
endif()
