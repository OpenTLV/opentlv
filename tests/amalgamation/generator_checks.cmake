# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Marek Cingel

# Regression tests for cmake/amalgamation/generate.cmake on fixture registries:
# composition order, include replacement and deduplication, include guards,
# private-header boundaries, notices, configuration rules and determinism.
#
#   cmake -DOPENTLV_SOURCE_DIR=<repository> -DWORK_DIR=<scratch directory> -P generator_checks.cmake
cmake_minimum_required(VERSION 3.16)
# Fixture notices below are test data, not this file's license.
# REUSE-IgnoreStart

foreach(variable IN ITEMS OPENTLV_SOURCE_DIR WORK_DIR)
    if(NOT ${variable})
        message(FATAL_ERROR "${variable} is required")
    endif()
endforeach()
set(generator "${OPENTLV_SOURCE_DIR}/cmake/amalgamation/generate.cmake")
file(REMOVE_RECURSE "${WORK_DIR}")
set_property(GLOBAL PROPERTY FAILURES 0)

# check(<message> <if() condition arguments>...)
function(check message)
    if(NOT (${ARGN}))
        message(SEND_ERROR "FAIL: ${message}")
        get_property(count GLOBAL PROPERTY FAILURES)
        math(EXPR count "${count} + 1")
        set_property(GLOBAL PROPERTY FAILURES ${count})
    endif()
endfunction()

# Index of a literal text in the value of a variable, or -1.
function(find variable text output)
    string(FIND "${${variable}}" "${text}" index)
    set(${output} ${index} PARENT_SCOPE)
endfunction()

function(occurrences variable text output)
    string(REPLACE "${text}" "" rest "${${variable}}")
    string(LENGTH "${${variable}}" full)
    string(LENGTH "${rest}" short)
    string(LENGTH "${text}" length)
    math(EXPR count "(${full} - ${short}) / ${length}")
    set(${output} ${count} PARENT_SCOPE)
endfunction()

set(mit "// SPDX-License-Identifier: MIT\n// Copyright (c) 2026 Fixture Author\n")
set(macro_line "#define FX_UTIL_ARRAY(name) int name[2] = {1, 2}; /* [brackets]; and \\ */")

# Writes a two-module fixture: base, with a generated header, a generated
# source and a private header; addon, a builtin that depends on base; and a
# facade, which contributes no C.
function(write_fixture root)
    set(src "${root}/source")
    set(bin "${root}/build")
    file(WRITE "${src}/include/fx/base/base.h" "${mit}
#ifndef FX_BASE_H
#define FX_BASE_H
#include \"fx/base/export.h\"
#include <stddef.h>
#ifdef __cplusplus
extern \"C\" {
#endif
OPENTLV_BASE_API int fx_base(void);
#ifdef __cplusplus
}
#endif
#endif /* FX_BASE_H */
")
    # A relative include, a second inclusion of base.h and CMake list characters.
    file(WRITE "${src}/include/fx/base/util.h" "${mit}
#ifndef FX_BASE_UTIL_H
#define FX_BASE_UTIL_H
#include \"base.h\" /* relative to this header */
${macro_line}
#endif
")
    file(WRITE "${src}/src/base/internal/base_internal.h" "${mit}
#ifndef FX_BASE_INTERNAL_H
#define FX_BASE_INTERNAL_H
static int base_helper(void) { return 1; }
#endif
")
    file(WRITE "${src}/src/base/base.c" "${mit}
#include \"fx/base/base.h\"
#include \"internal/base_internal.h\"
#include <string.h>
#define BASE_LOCAL 2
int fx_base(void) { return base_helper() + BASE_LOCAL; }
#undef BASE_LOCAL
")
    file(WRITE "${src}/src/base/more.c" "// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Other Author

#include \"fx/base/util.h\"
#include \"internal/base_internal.h\"
int fx_more(void) { return base_helper(); }
")
    file(WRITE "${src}/include/fx/addon/addon.h" "${mit}
#ifndef FX_ADDON_H
#define FX_ADDON_H
#include \"fx/addon/export.h\"
#include \"fx/base/base.h\"
OPENTLV_ADDON_API int fx_addon(void);
#endif
")
    file(WRITE "${src}/include/fx/addon/addon.hpp" "// SPDX-License-Identifier: MIT\n#include \"fx/addon/addon.h\"\n")
    file(WRITE "${src}/src/addon/addon.c" "${mit}
#include \"fx/addon/addon.h\"
int fx_addon(void) { return fx_base(); }
")
    file(WRITE "${src}/include/fx/all.h" "// SPDX-License-Identifier: MIT\n#include \"fx/addon/addon.h\"\n")
    # Configured inputs, at the paths cmake/version.cmake writes.
    file(WRITE "${bin}/generated/include/opentlv/core/version.h" "${mit}
#ifndef FX_VERSION_H
#define FX_VERSION_H
#include \"fx/base/export.h\"
#define FX_VERSION \"1.2.3\"
#endif
")
    file(WRITE "${bin}/generated/src/version.c" "${mit}
#include \"opentlv/core/version.h\"
const char* fx_version(void) { return FX_VERSION; }
")
    # Stand-ins for the CMake export headers, which the portable block replaces.
    file(WRITE "${bin}/generated/include/fx/base/export.h" "#error replaced\n")
    file(WRITE "${bin}/generated/include/fx/addon/export.h" "#error replaced\n")
    write_registry("${root}" "")
endfunction()

# The registry and configuration as opentlv_finalize_modules() and
# opentlv_add_amalgamation_target() write them; extra overrides fields.
function(write_registry root extra)
    set(src "${root}/source")
    set(bin "${root}/build")
    file(WRITE "${bin}/registry.cmake" "set(OPENTLV_REGISTRY_MODULES [==[base;addon;facade]==])
set(OPENTLV_REGISTRY_AGGREGATE_HEADERS [==[${src}/include/fx/all.h]==])
set(OPENTLV_REGISTRY_base_EXPORT_NAME [==[Base]==])
set(OPENTLV_REGISTRY_base_OWNER [==[base]==])
set(OPENTLV_REGISTRY_base_INCLUDE_DIR [==[${src}/include]==])
set(OPENTLV_REGISTRY_base_SOURCES [==[${src}/src/base/base.c;${src}/src/base/more.c;${bin}/generated/src/version.c]==])
set(OPENTLV_REGISTRY_base_PUBLIC_HEADERS [==[${src}/include/fx/base/base.h;${src}/include/fx/base/util.h]==])
set(OPENTLV_REGISTRY_base_GENERATED_HEADERS [==[${bin}/generated/include/opentlv/core/version.h]==])
set(OPENTLV_REGISTRY_base_PRIVATE_HEADERS [==[${src}/src/base/internal/base_internal.h]==])
set(OPENTLV_REGISTRY_base_EXPORT_HEADER [==[${bin}/generated/include/fx/base/export.h]==])
set(OPENTLV_REGISTRY_base_API_MACRO [==[OPENTLV_BASE_API]==])
set(OPENTLV_REGISTRY_base_COMPILE_DEFINITIONS [==[OPENTLV_BASE_STATIC_DEFINE]==])
set(OPENTLV_REGISTRY_addon_EXPORT_NAME [==[Addon]==])
set(OPENTLV_REGISTRY_addon_OWNER [==[addon]==])
set(OPENTLV_REGISTRY_addon_BUILTIN [==[TRUE]==])
set(OPENTLV_REGISTRY_addon_INCLUDE_DIR [==[${src}/include]==])
set(OPENTLV_REGISTRY_addon_SOURCES [==[${src}/src/addon/addon.c]==])
set(OPENTLV_REGISTRY_addon_PUBLIC_HEADERS [==[${src}/include/fx/addon/addon.h]==])
set(OPENTLV_REGISTRY_addon_CXX_HEADERS [==[${src}/include/fx/addon/addon.hpp]==])
set(OPENTLV_REGISTRY_addon_PUBLIC_DEPS [==[base]==])
set(OPENTLV_REGISTRY_addon_EXPORT_HEADER [==[${bin}/generated/include/fx/addon/export.h]==])
set(OPENTLV_REGISTRY_addon_API_MACRO [==[OPENTLV_ADDON_API]==])
set(OPENTLV_REGISTRY_facade_EXPORT_NAME [==[Facade]==])
set(OPENTLV_REGISTRY_facade_FACADE [==[TRUE]==])
set(OPENTLV_REGISTRY_facade_OWNER [==[addon]==])
set(OPENTLV_REGISTRY_facade_INCLUDE_DIR [==[${src}/include]==])
set(OPENTLV_REGISTRY_facade_PUBLIC_DEPS [==[base;addon]==])
${extra}
")
    file(WRITE "${bin}/config.cmake" "set(OPENTLV_AMALGAMATION_SOURCE_DIR [==[${src}]==])
set(OPENTLV_AMALGAMATION_BINARY_DIR [==[${bin}]==])
set(OPENTLV_AMALGAMATION_REGISTRY [==[${bin}/registry.cmake]==])
set(OPENTLV_AMALGAMATION_VERSION [==[1.2.3-rc.1]==])
set(OPENTLV_AMALGAMATION_REVISION [==[4]==])
set(OPENTLV_AMALGAMATION_COMMIT [==[abc1234]==])
set(OPENTLV_AMALGAMATION_BRANCH [==[feature/\"quoted\"]==])
set(OPENTLV_AMALGAMATION_TAG [==[v1.2.3-rc.1]==])
set(OPENTLV_AMALGAMATION_QUERY_FRONTEND [==[ON]==])
set(OPENTLV_AMALGAMATION_QUERY_SET_OPERATIONS [==[OFF]==])
")
endfunction()

function(generate root result messages)
    execute_process(COMMAND "${CMAKE_COMMAND}" "-DOPENTLV_AMALGAMATION_CONFIG=${root}/build/config.cmake"
        "-DOPENTLV_AMALGAMATION_OUTPUT=${root}/build/amalgamation" -P "${generator}"
        RESULT_VARIABLE code OUTPUT_VARIABLE output ERROR_VARIABLE errors)
    set(${result} "${code}" PARENT_SCOPE)
    string(REGEX REPLACE "[ \t\r\n]+" " " flat "${output}${errors}")
    set(${messages} "${flat}" PARENT_SCOPE)
endfunction()

# Each failure case starts from a valid fixture in ${WORK_DIR}/fail-<name>,
# changes one input and expects generation to fail with a message.
macro(failure_case name)
    set(case "${name}")
    set(root "${WORK_DIR}/fail-${name}")
    set(src "${root}/source")
    write_fixture("${root}")
endmacro()

function(expect_failure expected)
    generate("${root}" code messages)
    string(REGEX REPLACE "[ \t\r\n]+" " " expected "${expected}")
    string(FIND "${messages}" "opentlv_amalgamation: ${expected}" index)
    check("${case}: generation succeeded" NOT code EQUAL 0)
    check("${case}: expected 'opentlv_amalgamation: ${expected}', got: ${messages}" NOT index EQUAL -1)
    check("${case}: a failed generation wrote outputs" NOT EXISTS "${root}/build/amalgamation")
endfunction()

# --- Valid composition ----------------------------------------------------------

set(valid "${WORK_DIR}/valid")
write_fixture("${valid}")
generate("${valid}" code messages)
check("valid fixture: ${messages}" code EQUAL 0)
set(out "${valid}/build/amalgamation")
file(READ "${out}/opentlv.h" header)
file(READ "${out}/opentlv.c" source)
file(READ "${out}/opentlv-amalgamation.json" metadata)

# Order: registered targets and headers, each include expanded at first use.
set(previous -1)
foreach(marker IN ITEMS "begin include/fx/base/base.h" "begin generated/include/fx/base/export.h"
        "end generated/include/fx/base/export.h" "end include/fx/base/base.h"
        "begin include/fx/base/util.h" "end include/fx/base/util.h"
        "begin generated/include/opentlv/core/version.h" "begin include/fx/addon/addon.h"
        "begin generated/include/fx/addon/export.h" "end include/fx/addon/addon.h")
    find(header "/* ==== ${marker} ==== */" index)
    check("opentlv.h order at '${marker}'" index GREATER previous)
    set(previous ${index})
endforeach()
set(previous -1)
foreach(marker IN ITEMS "begin src/base/base.c" "begin src/base/internal/base_internal.h"
        "end src/base/internal/base_internal.h" "end src/base/base.c" "begin src/base/more.c"
        "begin generated/src/version.c" "begin src/addon/addon.c")
    find(source "/* ==== ${marker} ==== */" index)
    check("opentlv.c order at '${marker}'" index GREATER previous)
    set(previous ${index})
endforeach()
foreach(marker IN ITEMS "begin include/fx/base/base.h" "begin generated/include/fx/base/export.h")
    occurrences(header "${marker}" count)
    check("opentlv.h emits ${marker} ${count} times" count EQUAL 1)
endforeach()
occurrences(source "begin src/base/internal/base_internal.h" count)
check("opentlv.c emits the private header ${count} times" count EQUAL 1)
occurrences(header "#include \"" count)
check("opentlv.h keeps ${count} quoted includes" count EQUAL 0)
occurrences(source "#include \"" count)
check("opentlv.c keeps ${count} quoted includes besides opentlv.h" count EQUAL 1)
find(source "build target again.\n\n#include \"opentlv.h\"\n\n/* ==== begin src/base/base.c ==== */\n" index)
check("opentlv.c does not start with the prologue and opentlv.h" index GREATER 0)
find(header "#include <stddef.h>\n" stddef)
find(source "#include <string.h>\n" string_h)
check("standard includes are not kept" stddef GREATER 0 AND string_h GREATER 0)
find(source "include/fx/" index)
check("opentlv.c contains a public header" index EQUAL -1)
find(header "internal" index)
check("opentlv.h contains a private header" index EQUAL -1)
find(header "${macro_line}\n" index)
check("CMake list characters are not copied unchanged" index GREATER 0)
find(header "#ifdef __cplusplus\nextern \"C\" {\n#endif\n" index)
check("the C linkage block is not kept" index GREATER 0)
# Notices: one MIT identifier and the distinct copyright lines of each file's
# own inputs in first-seen order; the export block template has its own.
set(notice "\n//\n// Generated by the OpenTLV C source amalgamation (contract 1) from\n// OpenTLV 1.2.3-rc.1, commit abc1234.\n")
find(header "// SPDX-License-Identifier: MIT\n// Copyright (c) 2026 Fixture Author\n// Copyright (c) 2026 Marek Cingel${notice}" index)
check("opentlv.h prologue" index EQUAL 0)
find(source "// SPDX-License-Identifier: MIT\n// Copyright (c) 2026 Fixture Author\n// Copyright (c) 2025 Other Author${notice}" index)
check("opentlv.c prologue" index EQUAL 0)
find(header "// SPDX-License-Identifier: MIT\n// Copyright (c) 2026 Fixture Author\n\n#ifndef FX_BASE_H" index)
check("inlined files keep their notices" index GREATER 0)
# The portable export block replaces the CMake export header.
find(header "#ifndef OPENTLV_BASE_API_H\n#define OPENTLV_BASE_API_H\n#ifndef OPENTLV_BASE_API\n#define OPENTLV_BASE_API\n#endif\n#endif\n" index)
check("opentlv.h lacks the portable export block" index GREATER 0)
find(header "#error replaced" index)
check("the CMake export header was copied" index EQUAL -1)
# Metadata, with sorted keys.
foreach(text IN ITEMS "{\n  \"branch\": \"feature/\\\"quoted\\\"\",\n  \"commit\": \"abc1234\",\n"
        "  \"configuration\": {\n    \"query_frontend\": true,\n    \"query_set_operations\": false\n  },\n  \"inputs\": [\n"
        "  \"library\": \"OpenTLV\",\n  \"revision\": 4,\n  \"schema\": 1,\n  \"tag\": \"v1.2.3-rc.1\",\n  \"targets\": [\n"
        "\"export_name\": \"Base\",\n      \"key\": \"base\",\n      \"kind\": \"module\",\n      \"public_headers\": [\n        \"include/fx/base/base.h\",\n        \"include/fx/base/util.h\",\n        \"generated/include/opentlv/core/version.h\"\n      ],\n      \"sources\": [\n        \"src/base/base.c\",\n        \"src/base/more.c\",\n        \"generated/src/version.c\"\n      ]\n    },\n"
        "\"key\": \"addon\",\n      \"kind\": \"builtin\","
        "  ],\n  \"version\": \"1.2.3-rc.1\"\n}\n")
    find(metadata "${text}" index)
    check("metadata lacks: ${text}" NOT index EQUAL -1)
endforeach()
find(metadata "Facade" index)
check("a facade target was composed" index EQUAL -1)
file(READ "${valid}/source/include/fx/base/base.h" text)
string(REPLACE "\r\n" "\n" text "${text}")
string(SHA256 digest "${text}")
find(metadata "{\n      \"path\": \"include/fx/base/base.h\",\n      \"sha256\": \"${digest}\"\n    },\n    {\n      \"path\": \"generated/include/fx/base/export.h\"," index)
check("metadata inputs lack the digest or emission order of base.h" index GREATER 0)
find(metadata "\"path\": \"src/base/internal/base_internal.h\"" private_input)
find(metadata "\"path\": \"src/base/more.c\"" more_input)
check("metadata inputs do not follow emission order" private_input GREATER 0 AND more_input GREATER private_input)
# include/ tree: unchanged headers, the aggregate and C++ headers, portable export headers.
foreach(path IN ITEMS fx/base/base.h fx/base/util.h fx/addon/addon.h fx/addon/addon.hpp fx/all.h)
    file(READ "${valid}/source/include/${path}" expected)
    file(READ "${out}/include/${path}" actual)
    check("include/${path} differs from its source" expected STREQUAL actual)
endforeach()
file(READ "${out}/include/fx/base/export.h" tree_export)
find(header "/* ==== begin generated/include/fx/base/export.h ==== */\n${tree_export}/* ==== end" index)
check("include/fx/base/export.h differs from its opentlv.h block" index GREATER 0)
check("include/ lacks the generated header" EXISTS "${out}/include/opentlv/core/version.h")
file(GLOB_RECURSE tree RELATIVE "${out}/include" "${out}/include/*")
list(LENGTH tree count)
check("include/ has ${count} files instead of 8: ${tree}" count EQUAL 8)
foreach(text IN ITEMS header source metadata)
    find(${text} "${WORK_DIR}" index)
    check("${text} contains an absolute path" index EQUAL -1)
endforeach()

# Determinism: another absolute location, CRLF inputs and a second run over the
# same output give identical bytes.
set(moved "${WORK_DIR}/elsewhere/other name")
write_fixture("${moved}")
file(READ "${moved}/source/src/base/base.c" crlf)
string(REPLACE "\n" "\r\n" crlf "${crlf}")
file(WRITE "${moved}/source/src/base/base.c" "${crlf}")
generate("${moved}" code messages)
check("moved fixture: ${messages}" code EQUAL 0)
generate("${valid}" code messages)
check("second generation: ${messages}" code EQUAL 0)
foreach(path IN ITEMS opentlv.h opentlv.c opentlv-amalgamation.json include/fx/base/export.h include/fx/all.h)
    file(SHA256 "${out}/${path}" first)
    file(SHA256 "${moved}/build/amalgamation/${path}" second)
    check("${path} depends on the location or line endings" first STREQUAL second)
endforeach()
file(READ "${out}/opentlv.h" again)
check("a second generation changed opentlv.h" again STREQUAL header)
find(source "\r" carriage)
check("opentlv.c contains CR line endings" carriage EQUAL -1)

# UTF-8 outside ASCII is accepted.
set(utf8 "${WORK_DIR}/utf8")
write_fixture("${utf8}")
string(ASCII 194 lead)
string(ASCII 167 trail)
file(APPEND "${utf8}/source/src/addon/addon.c" "/* X.690 ${lead}${trail}8.1 */\n")
foreach(bytes IN ITEMS "224;160;128" "237;159;191" "238;128;128"
        "240;144;128;128" "244;143;191;191")
    string(ASCII ${bytes} scalar)
    file(APPEND "${utf8}/source/src/addon/addon.c" "/* ${scalar} */\n")
endforeach()
generate("${utf8}" code messages)
check("UTF-8 input: ${messages}" code EQUAL 0)

# --- Failures -------------------------------------------------------------------

# Registry and configuration.
failure_case(order)
write_registry("${root}" "set(OPENTLV_REGISTRY_MODULES [==[addon;base]==])")
expect_failure("addon is registered before its dependency base")

failure_case(definition)
write_registry("${root}" "set(OPENTLV_REGISTRY_base_COMPILE_DEFINITIONS [==[OPENTLV_BASE_STATIC_DEFINE;FX_FEATURE=1]==])")
expect_failure("base has the compile definition FX_FEATURE=1, which the contract does not cover")

failure_case(generated-input)
file(WRITE "${root}/build/generated/include/fx/config.h" "${mit}#ifndef FX_CONFIG_H\n#define FX_CONFIG_H\n#endif\n")
write_registry("${root}" "list(APPEND OPENTLV_REGISTRY_base_GENERATED_HEADERS [==[${root}/build/generated/include/fx/config.h]==])")
expect_failure("generated input generated/include/fx/config.h is not covered by the amalgamation contract")

failure_case(export-header)
write_registry("${root}" "set(OPENTLV_REGISTRY_base_API_MACRO)")
expect_failure("base has no registered export header")

failure_case(duplicate-file)
write_registry("${root}" "list(APPEND OPENTLV_REGISTRY_addon_PUBLIC_HEADERS [==[${src}/include/fx/base/util.h]==])")
expect_failure("include/fx/base/util.h is registered twice")

failure_case(no-modules)
write_registry("${root}" "set(OPENTLV_REGISTRY_MODULES facade)")
expect_failure("the registry has no compiled target")

failure_case(missing-file)
file(REMOVE "${src}/src/addon/addon.c")
expect_failure("src/addon/addon.c does not exist")

# Includes.
failure_case(unresolved)
file(WRITE "${src}/src/addon/addon.c" "${mit}#include \"fx/missing.h\"\n")
expect_failure("#include \"fx/missing.h\" does not resolve to a registered header")

failure_case(unregistered)
file(WRITE "${src}/include/fx/extra.h" "${mit}#ifndef FX_EXTRA_H\n#define FX_EXTRA_H\n#endif\n")
file(WRITE "${src}/src/addon/addon.c" "${mit}#include \"fx/extra.h\"\n")
expect_failure("#include \"fx/extra.h\" reaches ${src}/include/fx/extra.h, which no composed target registers")

failure_case(public-private)
file(WRITE "${src}/include/fx/base/util.h" "${mit}#ifndef FX_BASE_UTIL_H\n#define FX_BASE_UTIL_H\n#include \"../../../src/base/internal/base_internal.h\"\n#endif\n")
expect_failure("public header include/fx/base/util.h includes private file src/base/internal/base_internal.h")

failure_case(cross-module-private)
file(WRITE "${src}/src/addon/addon.c" "${mit}#include \"../base/internal/base_internal.h\"\n")
expect_failure("src/addon/addon.c of module addon includes src/base/internal/base_internal.h, private to module base")

failure_case(source-include)
file(WRITE "${src}/src/addon/addon.c" "${mit}#include \"../base/more.c\"\n")
expect_failure("src/addon/addon.c includes the source file src/base/more.c")

failure_case(unreached-public)
file(WRITE "${src}/include/fx/addon/addon.h" "${mit}#ifndef FX_ADDON_H\n#define FX_ADDON_H\nint fx_addon(void);\n#endif\n")
file(WRITE "${src}/src/addon/addon.c" "${mit}#include \"fx/addon/export.h\"\n")
expect_failure("src/addon/addon.c includes generated/include/fx/addon/export.h, which no public header of opentlv.h reaches")

failure_case(conditional)
file(WRITE "${src}/src/addon/addon.c" "${mit}#ifdef FX_OPTION\n#include \"fx/addon/addon.h\"\n#endif\n")
expect_failure("src/addon/addon.c includes \"fx/addon/addon.h\" conditionally")

failure_case(conditional-header)
file(WRITE "${src}/include/fx/addon/addon.h" "${mit}#ifndef FX_ADDON_H\n#define FX_ADDON_H\n#include \"fx/addon/export.h\"\n#if 1\n#include \"fx/base/base.h\"\n#endif\n#endif\n")
expect_failure("include/fx/addon/addon.h includes \"fx/base/base.h\" conditionally")

failure_case(nonstandard)
file(WRITE "${src}/src/addon/addon.c" "${mit}#include <windows.h>\n")
expect_failure("src/addon/addon.c includes <windows.h>, which is not a C99 standard header")

failure_case(computed-include)
file(WRITE "${src}/src/addon/addon.c" "${mit}#define FX_HEADER \"fx/addon/addon.h\"\n#include FX_HEADER\n#undef FX_HEADER\n")
expect_failure("src/addon/addon.c has an include that is not a literal header name")

# Include guards and conditionals.
failure_case(missing-guard)
file(WRITE "${src}/include/fx/addon/addon.h" "${mit}#include \"fx/addon/export.h\"\n")
expect_failure("include/fx/addon/addon.h does not start with an include guard")

failure_case(pragma-once)
file(WRITE "${src}/include/fx/addon/addon.h" "${mit}#ifndef FX_ADDON_H\n#define FX_ADDON_H\n#pragma once\n#endif\n")
expect_failure("include/fx/addon/addon.h uses #pragma once")

failure_case(guard-define)
file(WRITE "${src}/include/fx/addon/addon.h" "${mit}#ifndef FX_ADDON_H\n#define FX_OTHER_H\n#endif\n")
expect_failure("include/fx/addon/addon.h does not define its include guard FX_ADDON_H")

failure_case(duplicate-guard)
file(WRITE "${src}/include/fx/base/util.h" "${mit}#ifndef FX_BASE_H\n#define FX_BASE_H\n#endif\n")
expect_failure("include/fx/base/util.h reuses the include guard FX_BASE_H")

failure_case(outside-guard)
file(WRITE "${src}/include/fx/addon/addon.h" "${mit}#ifndef FX_ADDON_H\n#define FX_ADDON_H\n#endif\n#define FX_LEAK 1\n")
expect_failure("include/fx/addon/addon.h has a directive after its include guard")

failure_case(unbalanced)
file(WRITE "${src}/src/addon/addon.c" "${mit}#ifdef FX_OPTION\nint fx_option;\n")
expect_failure("src/addon/addon.c has unbalanced conditionals or no closing include guard")

# Source rules and notices.
failure_case(undef)
file(WRITE "${src}/src/base/base.c" "${mit}#include \"fx/base/base.h\"\n#define BASE_LOCAL 2\nint fx_base(void) { return BASE_LOCAL; }\n")
expect_failure("src/base/base.c does not #undef the macros it defines: BASE_LOCAL")

failure_case(build-macro)
file(WRITE "${src}/src/addon/addon.c" "${mit}const char* fx_file = __FILE__;\n")
expect_failure("src/addon/addon.c uses __FILE__, which depends on the build")

failure_case(license)
file(WRITE "${src}/src/addon/addon.c" "// SPDX-License-Identifier: Apache-2.0\nint fx_addon(void) { return 0; }\n")
expect_failure("src/addon/addon.c: SPDX-License-Identifier: Apache-2.0 is not covered by the amalgamation contract")

failure_case(no-license)
file(WRITE "${src}/src/addon/addon.c" "int fx_addon(void) { return 0; }\n")
expect_failure("src/addon/addon.c has no SPDX-License-Identifier")

failure_case(utf8)
string(ASCII 255 invalid)
file(WRITE "${src}/src/addon/addon.c" "${mit}/* ${invalid} */\n")
expect_failure("src/addon/addon.c is not UTF-8")

# Reject non-scalar values and non-shortest encodings, even when their
# continuation-byte counts are otherwise correct.
foreach(bytes IN ITEMS "224;128;128" "237;160;128" "240;128;128;128" "244;144;128;128")
    failure_case(utf8-scalar)
    string(ASCII ${bytes} invalid)
    file(WRITE "${src}/src/addon/addon.c" "${mit}/* ${invalid} */\n")
    expect_failure("src/addon/addon.c is not UTF-8")
endforeach()

failure_case(output)
execute_process(COMMAND "${CMAKE_COMMAND}" "-DOPENTLV_AMALGAMATION_CONFIG=${root}/build/config.cmake"
    "-DOPENTLV_AMALGAMATION_OUTPUT=${root}/build" -P "${generator}"
    RESULT_VARIABLE code OUTPUT_VARIABLE output ERROR_VARIABLE errors)
string(FIND "${output}${errors}" "the output directory must be a dedicated directory" index)
check("output: the build directory was accepted as output" NOT code EQUAL 0 AND NOT index EQUAL -1)

get_property(failures GLOBAL PROPERTY FAILURES)
if(failures GREATER 0)
    message(FATAL_ERROR "${failures} amalgamation generator checks failed")
endif()
message(STATUS "Amalgamation generator checks passed")
# REUSE-IgnoreEnd
