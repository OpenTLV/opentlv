include(modules.cmake)
include(cxx.cmake)

# Optional convenience interface for applications deliberately using all modules.
# In-tree consumers declare their functional dependencies explicitly.
add_library(opentlv_tlv INTERFACE)
set_target_properties(opentlv_tlv PROPERTIES EXPORT_NAME tlv)
# Its umbrella headers contain only includes. The registry inventory lists them
# so that the amalgamation's include/ tree ships them unchanged.
set_property(GLOBAL PROPERTY OPENTLV_AGGREGATE_HEADERS
    "${OpenTLV_SOURCE_DIR}/include/opentlv/tlv.h"
    "${OpenTLV_SOURCE_DIR}/include/opentlv/tlv.hpp"
    "${OpenTLV_SOURCE_DIR}/include/opentlv/native.hpp")
get_property(_modules GLOBAL PROPERTY OPENTLV_MODULES)
foreach(_module IN LISTS _modules)
    get_property(_target GLOBAL PROPERTY OPENTLV_MODULE_${_module})
    target_link_libraries(opentlv_tlv INTERFACE ${_target})
endforeach()
