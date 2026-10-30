# These consumers currently expose the full composed API. Keep requirements
# explicit rather than producing unresolved symbols in reduced builds.
if(NOT OPENTLV_SCHEMA)
    foreach(component IN ITEMS EXAMPLES BENCHMARKS FUZZING)
        if(OPENTLV_BUILD_${component})
            message(STATUS "Reduced capability build: skipping ${component} (requires Reader, Writer, Query, Schema and Codec)")
            set(OPENTLV_BUILD_${component} OFF)
        endif()
    endforeach()
    set(OPENTLV_BUILD_UNIT_TESTS OFF)
    set(OPENTLV_BUILD_INTEGRATION_TESTS OFF)
    set(OPENTLV_BUILD_PROPERTY_TESTS OFF)
endif()
