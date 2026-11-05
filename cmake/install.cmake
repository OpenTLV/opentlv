include(CMakePackageConfigHelpers)

install(TARGETS opentlv_tlv EXPORT OpenTLVTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY include/opentlv DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h" PATTERN "*.def")

# C++ facades are installed with their owning module regardless of build language.
install(FILES include/opentlv/tlv.hpp include/opentlv/native.hpp
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/opentlv)

set(_opentlv_package_dir "${CMAKE_INSTALL_LIBDIR}/cmake/OpenTLV")
# The same relocatable config loads installed or build-tree export files.
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/OpenTLVConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/OpenTLVConfig.cmake"
    INSTALL_DESTINATION "${_opentlv_package_dir}")
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/OpenTLVConfigVersion.cmake"
    VERSION "${OpenTLV_VERSION}" COMPATIBILITY ExactVersion)
install(EXPORT OpenTLVTargets NAMESPACE OpenTLV::
    DESTINATION "${_opentlv_package_dir}")
export(EXPORT OpenTLVTargets NAMESPACE OpenTLV::
    FILE "${OpenTLV_BINARY_DIR}/OpenTLVTargets.cmake")
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/OpenTLVConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/OpenTLVConfigVersion.cmake"
    DESTINATION "${_opentlv_package_dir}")
install(FILES LICENSE THIRD_PARTY.md README.md CHANGELOG.md
    DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/doc/OpenTLV")
install(DIRECTORY LICENSES
    DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/doc/OpenTLV")

# Record target provenance without exporting build paths into installed packages.
get_filename_component(_opentlv_build_dir "${OpenTLV_BINARY_DIR}" REALPATH)
get_property(_opentlv_modules GLOBAL PROPERTY OPENTLV_MODULES)
foreach(_module IN LISTS _opentlv_modules)
    get_property(_target GLOBAL PROPERTY OPENTLV_MODULE_${_module})
    set_property(TARGET ${_target} PROPERTY INTERFACE_OPENTLV_PACKAGE_DIR "${_opentlv_build_dir}")
endforeach()
set_property(TARGET opentlv_tlv PROPERTY INTERFACE_OPENTLV_PACKAGE_DIR "${_opentlv_build_dir}")

if(NOT _OPENTLV_TOP_LEVEL)
    if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.24)
        set(_opentlv_redirect_dir "${CMAKE_FIND_PACKAGE_REDIRECTS_DIR}")
    else()
        # Older CMake uses explicit directory hints to the same forwarding
        # configs. A failed version probe must not lose the embedded identity.
        set(_opentlv_redirect_dir "${OpenTLV_BINARY_DIR}/package-redirects")
    endif()
    file(MAKE_DIRECTORY "${_opentlv_redirect_dir}")
    foreach(_name IN ITEMS OpenTLV opentlv opentlv_core)
        set(${_name}_DIR "${_opentlv_redirect_dir}" PARENT_SCOPE)
    endforeach()
    # Replace CMake's version-agnostic stubs for both dependency spellings.
    # Also cover add_subdirectory and declarations without FIND_PACKAGE_ARGS.
    foreach(_name IN ITEMS opentlv opentlv_core)
        file(WRITE "${_opentlv_redirect_dir}/${_name}-config.cmake"
            "set(_OpenTLV_redirect_check TRUE)\n"
            "include(\"${OpenTLV_BINARY_DIR}/OpenTLVConfig.cmake\")\n"
            "unset(_OpenTLV_redirect_check)\n")
        # Always reach our config, where the real ExactVersion file is
        # checked. Rejecting here would let CMake try an unrelated installed
        # config (including old releases without target provenance checks).
        file(WRITE "${_opentlv_redirect_dir}/${_name}-config-version.cmake"
            "set(PACKAGE_VERSION \"${OpenTLV_VERSION}\")\n"
            "set(PACKAGE_VERSION_COMPATIBLE TRUE)\n"
            "set(PACKAGE_VERSION_EXACT TRUE)\n")
    endforeach()
endif()
