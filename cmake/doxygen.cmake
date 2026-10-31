find_package(Doxygen 1.9.1 REQUIRED COMPONENTS doxygen)

# Base URL of the published documentation site, used by the @docs alias to
# link generated API pages back to the guides and concepts.
set(OPENTLV_DOCS_URL "https://opentlv.github.io/opentlv-docs/latest/")

# Public declarations now use each module's generated export macro. Doxygen
# expands only PREDEFINED macros, so derive the full list from the registry.
set(OPENTLV_DOXYGEN_EXPORTS "TLV_API=")
get_property(_opentlv_doc_modules GLOBAL PROPERTY OPENTLV_MODULES)
foreach(_module IN LISTS _opentlv_doc_modules)
    get_property(_target GLOBAL PROPERTY OPENTLV_MODULE_${_module})
    string(TOUPPER "${_target}" _prefix)
    string(APPEND OPENTLV_DOXYGEN_EXPORTS " ${_prefix}_API=")
endforeach()

configure_file(
    "${PROJECT_SOURCE_DIR}/docs/api/Doxyfile.in"
    "${PROJECT_BINARY_DIR}/Doxyfile"
    @ONLY
)

add_custom_target(c-api-docs
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${PROJECT_BINARY_DIR}/docs/c-api"
    COMMAND Doxygen::doxygen "${PROJECT_BINARY_DIR}/Doxyfile"
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    COMMENT "Generating the OpenTLV C API reference"
    VERBATIM
)

configure_file(
    "${PROJECT_SOURCE_DIR}/docs/api/Doxyfile.common.in"
    "${PROJECT_BINARY_DIR}/Doxyfile.common" @ONLY
)
configure_file(
    "${PROJECT_SOURCE_DIR}/docs/api/Doxyfile.cxx.in"
    "${PROJECT_BINARY_DIR}/Doxyfile.cxx" @ONLY
)

add_custom_target(cxx-api-docs
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${PROJECT_BINARY_DIR}/docs/cxx-api"
    COMMAND Doxygen::doxygen "${PROJECT_BINARY_DIR}/Doxyfile.cxx"
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    COMMENT "Generating the OpenTLV C++ API reference"
    VERBATIM
)
# Resolve C references without adding C declarations to C++ navigation.
add_dependencies(cxx-api-docs c-api-docs)
