cmake_minimum_required(VERSION 3.16)

include(cmake/Example.cmake)

# The C and C++ use cases retain the document shared with opentlv-rust.
add_subdirectory(builtins/asn1)
add_subdirectory(builtins/dhcp)
add_subdirectory(builtins/emv)
add_subdirectory(builtins/lldp)
add_subdirectory(builtins/nfc/type2)
add_subdirectory(codec)
add_subdirectory(core)
add_subdirectory(document)
add_subdirectory(generator)
add_subdirectory(io)
add_subdirectory(query)
add_subdirectory(schema)
