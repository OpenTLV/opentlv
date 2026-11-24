"""Compile the public headers and check their diagnostics with GCC or Clang."""

import argparse
import os
import shutil
import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MODES = {"c99": 199901, "c11": 201112, "c17": 201710, "c23": 202311,
         "c++11": 201103, "c++14": 201402, "c++17": 201703}
ATTRIBUTES = {
    # Macro, compiler capability, diagnostic, and annotated use.
    "TLV_NODISCARD": ("warn_unused_result", "unused-result",
                      "ATTR int read_value(void);\nvoid probe(void) { read_value(); }"),
    "TLV_MAYBE_UNUSED": ("unused", "unused-variable",
                         "void probe(void) { ATTR int value = 1; }"),
    "TLV_FALLTHROUGH": ("fallthrough", "implicit-fallthrough", """
int probe(int n) {
    switch (n) {
    case 0: ++n; ATTR;
    default: return n;
    }
}
"""),
}
UMBRELLA = "#include <opentlv/tlv.h>\n"
LANGUAGE_CONTRACT = r"""
#if TLV_C99 != 199901L || TLV_C11 != 201112L || \
    TLV_C17 != 201710L || TLV_C23 != 202311L
#error "Incorrect public C version constants"
#endif
#if TLV_C_VERSION != EXPECTED_C_VERSION
#error "Incorrect active language detection"
#endif
#if TLV_HAS_C11 != (EXPECTED_C_VERSION >= TLV_C11) || \
    TLV_HAS_C17 != (EXPECTED_C_VERSION >= TLV_C17) || \
    TLV_HAS_C23 != (EXPECTED_C_VERSION >= TLV_C23)
#error "Incorrect C feature detection"
#endif
TLV_NODISCARD int probe(void);
"""
EMPTY_FALLBACK = """
#include <opentlv/core/compiler.h>
#undef TLV_HAS_ATTRIBUTE
#define TLV_HAS_ATTRIBUTE(x) 0
#include <opentlv/core/attributes.h>
#define SPELL_IMPL(x) #x
#define SPELL(x) SPELL_IMPL(x)
typedef char check_empty[sizeof(SPELL(TLV_NODISCARD) SPELL(TLV_MAYBE_UNUSED)
                                SPELL(TLV_FALLTHROUGH)) == 1 ? 1 : -1];
"""


def check_mode(build, headers, compiler, mode, version, clang_cl):
    cpp = mode.startswith("c++")
    filename = "check.cpp" if cpp else "check.c"

    def compile_source(source, *flags):
        (build / filename).write_text(source.rstrip() + "\n", encoding="utf-8")
        options = ["-std=" + mode_flag, "-pedantic-errors", "-Wall", "-Wextra", "-Werror", *flags]
        if clang_cl:
            command = [compiler, "/nologo", "/Iinclude", "/c", filename]
            command.extend("/clang:" + option for option in options)
        else:
            command = [compiler, *options, "-Iinclude", "-c", filename]
        return subprocess.run(
            command, cwd=build, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            text=True, errors="replace", env={**os.environ, "LC_ALL": "C"},
        )

    # An optional mode is skipped only for a rejected -std option or a draft
    # language version. Errors in OpenTLV headers always fail the check.
    language_version = "__cplusplus" if cpp else "__STDC_VERSION__"
    mode_probe = f"""
#if {language_version} != {version}L
#error TEST_MODE_UNAVAILABLE
#endif
int probe(void);
"""
    for mode_flag in ([mode, "c2x"] if mode == "c23" else [mode]):
        result = compile_source(mode_probe)
        if result.returncode == 0:
            break
        unsupported = "TEST_MODE_UNAVAILABLE" in result.stdout or (
            "-std=" + mode_flag in result.stdout and
            any(word in result.stdout.lower() for word in ("invalid", "unrecognized", "unknown")))
        if not unsupported or mode in ("c99", "c++17"):
            raise SystemExit(f"{mode} compiler probe failed\n{result.stdout}")
    else:
        print(f"SKIP {mode}: compiler does not provide the final language mode")
        return

    def expect(label, source, *flags, diagnostic=None):
        result = compile_source(source, *flags)
        if diagnostic is None:
            passed = result.returncode == 0
        else:
            passed = result.returncode != 0 and all(word in result.stdout for word in diagnostic)
        if not passed:
            expected = "success" if diagnostic is None else "an error containing " + ", ".join(diagnostic)
            raise SystemExit(f"{mode} failed {label}: expected {expected}\n{result.stdout}")

    if mode in ("c++11", "c++14"):
        expect("C++ umbrella minimum standard", "#include <opentlv/tlv.hpp>",
               diagnostic=("requires C++17",))
        print(f"PASS {mode}: C++ umbrella requires C++17")
        return

    for header in headers:
        expect(header, f"#include <{header}>\n" * 2 + "int probe(void);\n")
    c_version = 199901 if cpp else version
    contract = UMBRELLA + f"#define EXPECTED_C_VERSION {c_version}L\n" + LANGUAGE_CONTRACT
    if clang_cl:
        contract += "#if !defined(_MSC_VER) || !defined(_WIN32)\n#error Expected MSVC target\n#endif\n"
    expect("umbrella", contract)
    if mode == "c99":
        expect("empty attribute fallback", EMPTY_FALLBACK)

    for macro, (attribute, warning, use) in ATTRIBUTES.items():
        if mode in ("c99", "c++17"):
            expect(
                macro + " predefinition", f"#define {macro}\n" + UMBRELLA + "int probe(void);\n",
                diagnostic=("OpenTLV reserves", macro),
            )

        # Before standard attributes, the header uses compiler feature detection.
        # Probe the compiler independently so an accidental empty macro fails.
        if mode not in ("c23", "c++17"):
            capability = f"""
#ifndef __has_attribute
#define __has_attribute(x) 0
#endif
#if !__has_attribute({attribute})
#error TEST_NO_ATTRIBUTE
#endif
int probe(void);
"""
            result = compile_source(capability)
            if result.returncode:
                if "TEST_NO_ATTRIBUTE" not in result.stdout:
                    raise SystemExit(f"{mode} failed {attribute} detection\n{result.stdout}")
                print(f"SKIP {mode} {macro} diagnostic: no compiler attribute")
                continue

        warning_flag = "-Werror=" + warning
        adds_warning = macro == "TLV_NODISCARD"
        expect(macro + " control", UMBRELLA + use.replace("ATTR", ""), warning_flag,
               diagnostic=None if adds_warning else (warning,))
        expect(macro, UMBRELLA + use.replace("ATTR", macro), warning_flag,
               diagnostic=(warning,) if adds_warning else None)

    print(f"PASS {mode}: {len(headers)} public headers and attribute contracts")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--clang-cl", metavar="COMPILER",
                        help="use clang-cl with the Windows MSVC target")
    args = parser.parse_args()
    with tempfile.TemporaryDirectory() as directory:
        build = Path(directory)
        include = build / "include"
        shutil.copytree(ROOT / "include" / "opentlv", include / "opentlv")
        c_headers = sorted(path.relative_to(include).as_posix() for path in include.rglob("*.h"))
        cpp_headers = sorted(path.relative_to(include).as_posix() for path in include.rglob("*.hpp"))
        for mode, version in MODES.items():
            headers = c_headers + cpp_headers if mode == "c++17" else c_headers
            variable, default = ("CXX", "c++") if mode.startswith("c++") else ("CC", "cc")
            compiler = os.environ.get(variable, default)
            check_mode(build, headers, args.clang_cl or compiler, mode, version, bool(args.clang_cl))


if __name__ == "__main__":
    main()
