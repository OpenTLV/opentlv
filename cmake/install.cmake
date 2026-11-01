include(CMakePackageConfigHelpers)

install(TARGETS tlv EXPORT OpenTLVTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY tlv/include/tlv DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h" PATTERN "*.def")
install(FILES
    "${OpenTLV_BINARY_DIR}/generated/include/tlv/version.h"
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/tlv)

# C++ facades are installed with their owning module regardless of build language.
install(FILES tlv++/include/tlv++/tlv.hpp tlv++/include/tlv++/native.hpp
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/tlv++)

set(_opentlv_package_dir "${CMAKE_INSTALL_LIBDIR}/cmake/OpenTLV")
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/OpenTLVConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/OpenTLVConfig.cmake"
    INSTALL_DESTINATION "${_opentlv_package_dir}")
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/OpenTLVConfigVersion.cmake"
    VERSION "${OpenTLV_VERSION}" COMPATIBILITY ExactVersion)
install(EXPORT OpenTLVTargets NAMESPACE OpenTLV::
    DESTINATION "${_opentlv_package_dir}")
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/OpenTLVConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/OpenTLVConfigVersion.cmake"
    DESTINATION "${_opentlv_package_dir}")
install(FILES LICENSE THIRD_PARTY.md README.md CHANGELOG.md
    DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/doc/OpenTLV")
install(DIRECTORY LICENSES
    DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/doc/OpenTLV")
