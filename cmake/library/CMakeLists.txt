cmake_minimum_required(VERSION 3.16)

include(modules.cmake)
include(cxx.cmake)

# Optional convenience interface for applications deliberately using all modules.
# In-tree consumers declare their functional dependencies explicitly.
add_library(opentlv_tlv INTERFACE)
set_target_properties(opentlv_tlv PROPERTIES EXPORT_NAME tlv)
get_property(_modules GLOBAL PROPERTY OPENTLV_MODULES)
foreach(_module IN LISTS _modules)
    get_property(_target GLOBAL PROPERTY OPENTLV_MODULE_${_module})
    target_link_libraries(opentlv_tlv INTERFACE ${_target})
endforeach()
