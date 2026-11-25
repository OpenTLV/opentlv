cmake_minimum_required(VERSION 3.15)
project(OpenTLVExamples LANGUAGES C CXX)

find_package(OpenTLV CONFIG REQUIRED)

add_executable(opentlv-write write.c)
target_link_libraries(opentlv-write PRIVATE OpenTLV::io)
set_target_properties(opentlv-write PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED YES C_EXTENSIONS OFF)

add_executable(opentlv-read read.cpp)
target_link_libraries(opentlv-read PRIVATE OpenTLV::io)

add_executable(opentlv-write-cpp write.cpp)
target_link_libraries(opentlv-write-cpp PRIVATE OpenTLV::io)
set_target_properties(opentlv-read opentlv-write-cpp PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS OFF
)

enable_testing()
add_test(NAME opentlv-write COMMAND opentlv-write)
add_test(NAME opentlv-read COMMAND opentlv-read)
add_test(NAME opentlv-write-cpp COMMAND opentlv-write-cpp)
