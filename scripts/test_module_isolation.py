#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Marek Cingel
"""Exercise module infrastructure with real libraries and rejected boundary mutations."""

import argparse
import json
from pathlib import Path
import subprocess
import tempfile
import unittest

from check_module_isolation import ROOT, check_metadata, check_symbols, verify


def fixture(root):
    files = {
        "include/fixture/base.h": '#include "tlv/base/export.h"\n'
            'OPENTLV_BASE_API int tlv_fixture_base(void);\n',
        "include/fixture/util.h": '#include "tlv/util/export.h"\n'
            'OPENTLV_UTIL_API int tlv_fixture_util(void);\n',
        "include/fixture/addon.h": '#include "tlv/addon/export.h"\n'
            'OPENTLV_ADDON_API int tlv_fixture_addon(void);\n',
        "include/fixture/base.hpp": 'extern "C" {\n#include "fixture/base.h"\n}\n'
            'inline int fixture_value() { return tlv_fixture_base(); }\n',
        "base/private.h": '#include "tlv/base/export.h"\n'
            'OPENTLV_BASE_API int tlv_fixture_owner(void);\n',
        "base/base.c": '#include "fixture/base.h"\n#include "private.h"\n'
            'int tlv_fixture_hidden(void) { return 40; }\n'
            'int tlv_fixture_base(void) { return tlv_fixture_hidden() + 2; }\n'
            'int tlv_fixture_owner(void) { return 1; }\n',
        "util.c": '#include "fixture/util.h"\nint tlv_fixture_util(void) { return 2; }\n',
        "addon.c": '#include "fixture/addon.h"\n#include "fixture/base.h"\n'
            '#include "fixture/util.h"\n#include "base/private.h"\n'
            'int tlv_fixture_addon(void) { return tlv_fixture_base() + tlv_fixture_util()'
            ' + tlv_fixture_owner(); }\n',
        "base-consumer.c": '#include "fixture/base.h"\n'
            'int main(void) { return tlv_fixture_base() != 42; }\n',
        "util-consumer.c": '#include "fixture/util.h"\n'
            'int main(void) { return tlv_fixture_util() != 2; }\n',
        "addon-consumer.c": '#include "fixture/addon.h"\n'
            'int main(void) { return tlv_fixture_addon() != 45; }\n',
        "cpp-consumer.cpp": '#include "fixture/base.hpp"\n'
            'static_assert(__cplusplus >= 201103L, "facade must propagate C++11");\n'
            'int main() { return fixture_value() != 42; }\n',
    }
    for name, text in files.items():
        path = root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")
    (root / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.16)
project(OpenTLV VERSION 1.0.0 LANGUAGES C)
include("{ROOT.as_posix()}/cmake/modules.cmake")
opentlv_add_module(base EXPORT_NAME FixtureBase INCLUDE_DIR include
    SOURCES base/base.c PUBLIC_HEADERS include/fixture/base.h
    PRIVATE_HEADERS base/private.h PUBLIC_SYMBOLS tlv_fixture_base
    OWNER_SYMBOLS tlv_fixture_owner CONSUMER base-consumer.c)
opentlv_add_module(util EXPORT_NAME FixtureUtil INCLUDE_DIR include
    SOURCES util.c PUBLIC_HEADERS include/fixture/util.h
    PUBLIC_SYMBOLS tlv_fixture_util CONSUMER util-consumer.c)
opentlv_add_module(addon OWNER base EXPORT_NAME FixtureAddon INCLUDE_DIR include
    SOURCES addon.c PUBLIC_HEADERS include/fixture/addon.h
    PUBLIC_DEPS base PRIVATE_DEPS util PUBLIC_SYMBOLS tlv_fixture_addon
    CONSUMER addon-consumer.c)
opentlv_add_module(base-cxx FACADE OWNER base EXPORT_NAME FixtureCpp INCLUDE_DIR include
    PUBLIC_HEADERS include/fixture/base.hpp PUBLIC_DEPS base CONSUMER cpp-consumer.cpp)
if(OPENTLV_BUILD_TESTS)
    enable_testing()
    opentlv_add_module_test(base-test OWNER base SOURCES base-consumer.c)
endif()
opentlv_finalize_modules()
include(CMakePackageConfigHelpers)
configure_package_config_file("{ROOT.as_posix()}/cmake/OpenTLVConfig.cmake.in"
    "${{CMAKE_CURRENT_BINARY_DIR}}/OpenTLVConfig.cmake"
    INSTALL_DESTINATION lib/cmake/OpenTLV)
install(FILES "${{CMAKE_CURRENT_BINARY_DIR}}/OpenTLVConfig.cmake" DESTINATION lib/cmake/OpenTLV)
''', encoding="utf-8")


class IsolationTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        fixture(self.root)
        self.build = self.root / "build"

    def change(self, name, old, new):
        path = self.root / name
        text = path.read_text(encoding="utf-8")
        self.assertIn(old, text)
        path.write_text(text.replace(old, new), encoding="utf-8")

    def configure(self, error=None, *extra):
        args = ["cmake", "-S", str(self.root), "-B", str(self.build),
                "-DCMAKE_BUILD_TYPE=Release", "-DOPENTLV_BUILD_SHARED_LIBS=OFF", *extra]
        result = subprocess.run(args, capture_output=True, text=True)
        if error:
            self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn(error, result.stdout + result.stderr)
            return None
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        return json.loads((self.build / "modules-Release.json").read_text(encoding="utf-8"))

    def test_valid_contract_and_facade_without_cxx_compiler(self):
        entries = self.configure(None, "-DCMAKE_CXX_COMPILER=/no/cxx/compiler")
        self.assertEqual(len(check_metadata(entries)), 4)
        self.assertFalse(list(self.build.glob("CMakeFiles/*/CMakeCXXCompiler.cmake")))

    def test_builtin_selection_options_are_rejected(self):
        for family in ("FORMAT_ASN1", "FORMAT_BER", "FORMAT_DER", "FORMAT_CER",
                       "EMV", "BLUETOOTH", "NFC", "DHCP", "LLDP"):
            for value in ("ON", "OFF"):
                with self.subTest(family=family, value=value):
                    option = "OPENTLV_" + family
                    result = subprocess.run(
                        ["cmake", "-S", str(ROOT), "-B", str(self.build / (family + value)),
                         f"-D{option}={value}"], capture_output=True, text=True)
                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn(f"{option} was removed", result.stdout + result.stderr)
                    self.assertIn("OpenTLV::ASN1", result.stdout + result.stderr)

    def test_cycle(self):
        self.change("CMakeLists.txt", "OWNER_SYMBOLS tlv_fixture_owner",
                    "PUBLIC_DEPS addon OWNER_SYMBOLS tlv_fixture_owner")
        self.configure("Module dependency cycle")

    def test_unknown_dependency(self):
        self.change("CMakeLists.txt", "PRIVATE_DEPS util", "PRIVATE_DEPS absent")
        self.configure("Undeclared module dependency")

    def test_unregistered_library(self):
        library = self.root / "tlv"
        library.mkdir()
        (library / "CMakeLists.txt").write_text(
            "add_library(rogue STATIC ../util.c)\n", encoding="utf-8")
        self.change("CMakeLists.txt", "opentlv_finalize_modules()",
                    "add_subdirectory(tlv)\nopentlv_finalize_modules()")
        self.configure("Unregistered module library rogue")

    def test_bypassed_dependency_declaration(self):
        self.change("CMakeLists.txt", "opentlv_finalize_modules()",
                    "target_link_libraries(opentlv_base PRIVATE opentlv_util)\nopentlv_finalize_modules()")
        self.configure("declare links with PUBLIC_DEPS")

    def test_duplicate_test_owner(self):
        self.change("CMakeLists.txt", "opentlv_finalize_modules()",
                    "opentlv_add_module_test(other OWNER util SOURCES base-consumer.c)\n"
                    "opentlv_finalize_modules()")
        self.configure("already belongs to", "-DOPENTLV_BUILD_TESTS=ON")

    def test_bypassed_source_declaration(self):
        self.change("CMakeLists.txt", "opentlv_finalize_modules()",
                    "target_sources(opentlv_base PRIVATE util.c)\nopentlv_finalize_modules()")
        self.configure("declare all sources")

    def test_test_ownership(self):
        self.configure(None, "-DOPENTLV_BUILD_TESTS=ON")
        result = subprocess.check_output(["ctest", "--test-dir", str(self.build),
                                          "--show-only=json-v1"], text=True)
        tests = json.loads(result)["tests"]
        labels = next(p["value"] for p in tests[0]["properties"] if p["name"] == "LABELS")
        self.assertIn("module:base", labels)

    def test_private_header(self):
        self.change("util.c", '#include "fixture/util.h"',
                    '#include "fixture/util.h"\n#include "base/private.h"')
        with self.assertRaisesRegex(ValueError, "cross-module private header"):
            check_metadata(self.configure())

    def test_public_header_leaks_private_dependency(self):
        self.change("include/fixture/addon.h", '#include "tlv/addon/export.h"',
                    '#include "tlv/addon/export.h"\n#include <fixture/util.h>')
        with self.assertRaisesRegex(ValueError, "undeclared direct dependency"):
            check_metadata(self.configure())

    def test_private_include_directory_cannot_hide_foreign_header(self):
        self.change("util.c", '#include "fixture/util.h"',
                    '#include "fixture/util.h"\n#include "private.h"')
        self.change("CMakeLists.txt", "opentlv_finalize_modules()",
                    "target_include_directories(opentlv_util PRIVATE base)\n"
                    "opentlv_finalize_modules()")
        with self.assertRaisesRegex(ValueError, "cross-module private header"):
            check_metadata(self.configure())

    def test_duplicate_source_owner(self):
        self.change("CMakeLists.txt", "SOURCES util.c", "SOURCES util.c base/base.c")
        with self.assertRaisesRegex(ValueError, "Duplicate file ownership"):
            check_metadata(self.configure())

    def test_missing_dependency_in_metadata(self):
        entries = self.configure()
        entries[2]["private_deps"] = []
        with self.assertRaisesRegex(ValueError, "undeclared direct dependency"):
            check_metadata(entries)

    def test_architecture_contract_and_feature_disabled_source(self):
        entries = self.configure()
        contract = {"targets": {}, "files": {}}
        for entry in entries:
            key = entry["key"]
            contract["targets"][key] = {
                "module": entry["owner"][0], "public": entry["public_deps"],
                "private": entry["private_deps"],
            }
            for kind in ("sources", "public_headers", "private_headers"):
                for name in entry[kind]:
                    contract["files"][Path(name).relative_to(self.root).as_posix()] = key
        (self.root / "optional.c").write_text("/* Feature-disabled source. */\n", encoding="utf-8")
        contract["files"]["optional.c"] = "base"
        check_metadata(entries, contract, self.root)
        # A not-yet-extracted C++ facade retains logical ownership, but is
        # not part of the C module registration. Native headers still are.
        (self.root / "future.hpp").write_text("// Future facade.\n", encoding="utf-8")
        contract["files"]["future.hpp"] = "base"
        check_metadata(entries, contract, self.root)
        (self.root / "missing.h").write_text("/* Missing C header. */\n", encoding="utf-8")
        contract["files"]["missing.h"] = "base"
        with self.assertRaisesRegex(ValueError, "contract file missing from registration"):
            check_metadata(entries, contract, self.root)
        del contract["files"]["missing.h"]
        generated = self.root / "build/generated/src/config.c"
        generated.parent.mkdir(parents=True, exist_ok=True)
        generated.write_text("/* Generated configuration. */\n", encoding="utf-8")
        entries[0]["sources"].append(str(generated))
        with self.assertRaisesRegex(ValueError, "file ownership differs from contract"):
            check_metadata(entries, contract, self.root)
        contract["generated"] = {"src/config.c": "base"}
        check_metadata(entries, contract, self.root)
        contract["targets"]["base"]["public"] = ["util"]
        with self.assertRaisesRegex(ValueError, "dependencies differ from contract"):
            check_metadata(entries, contract, self.root)


def integration(build_root):
    source = build_root / "fixture"
    source.mkdir(parents=True, exist_ok=True)
    fixture(source)
    for shared in ("OFF", "ON"):
        work = verify(source.resolve(), build_root / shared, shared, "Ninja", "nm",
                      ("-DCMAKE_CXX_COMPILER=/no/cxx/compiler",))
        entries = json.loads((work / "complete/modules-Release.json").read_text(encoding="utf-8"))
        # Real nm output must reject export leakage, undeclared references, and
        # owner-internal symbols used by a foreign integration.
        for mutation, message in (("exports", "exported symbols differ"),
                                  ("dependency", "undeclared symbol dependency"),
                                  ("private", "private symbol")):
            changed = json.loads(json.dumps(entries))
            if mutation == "exports":
                if shared != "ON":
                    continue
                changed[0]["public_symbols"] = []
            elif mutation == "dependency":
                changed[2]["private_deps"] = []
            else:
                changed[2]["owner"] = ["addon"]
            try:
                check_symbols(changed, shared == "ON", "nm")
            except ValueError as error:
                if message not in str(error):
                    raise
            else:
                raise AssertionError(f"Symbol gate accepted {mutation}")
    print("PASS static/shared fixture isolation and symbol mutations")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--integration", action="store_true")
    parser.add_argument("--build-root", type=Path, default=ROOT / "build/module-fixture")
    args, rest = parser.parse_known_args()
    if args.integration:
        integration(args.build_root)
    else:
        unittest.main(argv=[__file__, *rest])
