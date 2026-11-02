# Architectural guardrails for Field Encoding and the canonical processing engines.
# Semantic behavior is covered by Integration_Tlv_Pipeline and component tests.
get_filename_component(root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
if(EXISTS "${root}/include/opentlv/io/reader/visitor.hpp")
    message(FATAL_ERROR "Use tree_reader::visit; the superseded C++ Visitor facade must not return")
endif()

function(forbid path pattern reason)
    file(READ "${root}/${path}" source)
    # Ignore comments so explanations of the boundary do not trigger the guard.
    string(REGEX REPLACE "/\\*([^*]|\\*+[^*/])*\\*+/" "" source "${source}")
    string(REGEX REPLACE "//[^\n]*" "" source "${source}")
    if(source MATCHES "${pattern}")
        message(FATAL_ERROR "${path}: ${reason}")
    endif()
endfunction()

file(GLOB_RECURSE field_files RELATIVE "${root}"
     "${root}/include/opentlv/core/field/*.h"
     "${root}/src/core/internal/field/*.h"
     "${root}/src/core/field/*.c")
foreach(path IN LISTS field_files)
    forbid("${path}" "#include[ \t]+[\"<]([^\">]*/)?(format\\.h|formats/)"
           "Field Encoding must not depend on Format or Format Composition")
endforeach()
if(EXISTS "${root}/include/opentlv/core/layout.h")
    message(FATAL_ERROR "Composition belongs in opentlv/core/formats/compose.h; opentlv/core/layout.h must not return")
endif()

foreach(path include/opentlv/schema/schema.h src/schema/report.c)
    forbid("${path}" "tlv_schema_(issue_t|report_t|issue_path_string)|TLV_SCHEMA_PATH_MAX"
           "Schema reports must use the shared diagnostic model")
endforeach()

foreach(path io/reader/reader.c io/reader/tree.c io/reader/visitor.c io/writer/writer.c io/writer/tree.c)
    forbid("src/${path}" "(malloc|calloc|realloc|alloca)[ \t\r\n]*\\("
           "Reader/Writer storage must remain caller-owned")
    forbid("src/${path}" "#include[ \t]+[\"<]opentlv/(document|query|schema|codec|builtin)/"
           "lower processing layers must not depend on higher layers or protocols")
endforeach()
foreach(path io/reader/tree.c io/reader/visitor.c query/query.c document/reader.c document/writer.c io/writer/tree.c)
    forbid("src/${path}" "tlv_format_(decode|encode|measure)[ \t\r\n]*\\(|->[ \t]*(decode|encode|measure)[ \t\r\n]*\\("
           "wire processing must go through Reader/Writer")
    forbid("src/${path}" "tlv_(fields|variable|packed|fixed)_(decode|encode|measure)[ \t\r\n]*\\("
           "layout mechanics belong to Format, not higher processing layers")
endforeach()
# Owned topology may ask Format to validate/measure a tag during insertion,
# independently of Writer. It must never decode or emit wire bytes itself.
forbid("src/document/document.c" "tlv_format_(decode|encode)[ \t\r\n]*\\(|->[ \t]*(decode|encode)[ \t\r\n]*\\("
       "Document wire import and serialization belong to explicit integrations")
forbid("src/document/document.c" "tlv_(fields|variable|packed|fixed)_(decode|encode|measure)[ \t\r\n]*\\("
       "Document must use Format contracts, not layout internals")
foreach(path io/reader/reader.c io/reader/tree.c)
    forbid("src/${path}" "#include[ \t]+[\"<]tlv/(io/writer/|io/reader/visitor)"
           "Reader engines must not depend on encoding or Visitor adapters")
endforeach()
foreach(path io/writer/writer.c io/writer/tree.c)
    forbid("src/${path}" "#include[ \t]+[\"<]opentlv/io/reader/"
           "Writer engines consume semantic content, not Reader traversal")
endforeach()
forbid("src/io/reader/reader.c" "#include[ \t]+[\"<]opentlv/io/reader/tree"
       "Reader must not depend on Tree Reader")
forbid("src/io/writer/writer.c" "#include[ \t]+[\"<]opentlv/io/writer/tree"
       "Writer must not depend on Tree Writer")
foreach(path query/query.c document/document.c io/reader/visitor.c)
    forbid("src/${path}" "tlv_(read|read_diag|read_source_diag)[ \t\r\n]*\\("
           "higher layers must consume cursor traversal instead of parsing bytes")
endforeach()
forbid("src/query/query.c" "tlv_(reader_next|reader_next_diag|reader_next_source_diag)[ \t\r\n]*\\("
       "nested Query traversal belongs to Tree Reader")
message(STATUS "Canonical pipeline dependency guardrails passed")
