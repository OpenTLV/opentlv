#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Marek Cingel
"""Regression cases for the functional module contract check."""
import copy
import json
from pathlib import Path
import tempfile
import unittest
import shutil
import subprocess
from check_cmake_integration import check_example_tests, expected_example_tests
from check_module_contracts import (BUILTIN, CONTRACT, GENERIC, MAPPING, ROOT, check,
                                    check_baseline_mapping, check_examples, cycles)

FILES = {
    "include/opentlv/core/result.h": "typedef enum tlv_result {\n    TLV_OK = 0, /**< ok */\n"
                                "    TLV_ERR_X\n} tlv_result_t;\n",
    "src/core/result.c": '#include "opentlv/core/result.h"\n',
    "include/opentlv/io/io.h": '#include "opentlv/core/result.h"\n',
    "src/io/io.c": '#include "opentlv/io/io.h"\n#include "internal/io_internal.h"\n',
    "src/io/internal/io_internal.h": '#include "opentlv/io/io.h"\n',
    "src/document/document.c": '#include "opentlv/core/result.h"\n',
    "src/query/query.c": 'static int f(void* d) { return query_limit(d, "work", 1, 0, 0); }\n',
    "include/opentlv/builtin/nfc/nfc.hpp": '#include "opentlv/core/result.h"\n',
}
OWNERS = {
    "include/opentlv/core/result.h": "core",
    "src/core/result.c": "core",
    "include/opentlv/io/io.h": "io",
    "src/io/io.c": "io",
    "src/io/internal/io_internal.h": "io",
    "src/document/document.c": "document",
    "src/query/query.c": "query",
    "include/opentlv/builtin/nfc/nfc.hpp": "nfc",
}


def contract():
    modules = {name: {"kind": "generic" if name in GENERIC else "builtin", "public_target": f"OpenTLV::{name}"}
               for name in GENERIC + BUILTIN}
    targets = {name: {"module": name, "role": "module", "public": [] if name == "core" else ["core"]}
               for name in modules}
    targets["io-tools"] = {"module": "io", "role": "integration", "public": ["io"], "public_target": "OpenTLV::IOTools"}
    targets["nfc"]["facade"] = ["io"]
    return {"modules": modules, "targets": targets, "generated": {}, "files": dict(OWNERS),
            "api": {}, "resolutions": {}, "exceptions": []}


def mapping():
    return {
        "ranges": {"generic": [1, 255], "builtin": [256, 511]},
        "domains": {"core": {"id": 1, "module": "core"}, "query": {"id": 4, "module": "query"}},
        "families": [
            {"family": "core.result", "id": 1, "domain": "core", "header": "include/opentlv/core/result.h",
             "typedef": "tlv_result", "codes": [code("TLV_OK", 0), code("TLV_ERR_X", 1)]},
            {"family": "query.limit", "id": None, "domain": "query",
             "codes": [code("work", None, "query")]},
        ],
    }


def code(name, value, module="core", new=None):
    return {"previous": name, "value": value, "new": new or name, "module": module,
            "meaning": "Meaning.", "change": "None."}


class ModuleContractTests(unittest.TestCase):
    def test_removed_baseline_family_cannot_disappear_with_its_header(self):
        errors = []
        check_baseline_mapping([{"family": "removed", "codes": {"OLD": 7}}], mapping(), errors)
        self.assertTrue(any("removed/OLD" in error for error in errors))

    def test_baseline_rejects_value_drift_duplicates_and_opt_out(self):
        for mutation in ("value", "duplicate", "opt-out"):
            with self.subTest(mutation=mutation):
                current = mapping()
                family = current["families"][0]
                if mutation == "value":
                    family["codes"][0]["value"] = 99
                elif mutation == "duplicate":
                    family["codes"].append(copy.deepcopy(family["codes"][0]))
                else:
                    family["baseline"] = False
                errors = []
                check_baseline_mapping([{"family": "core.result", "codes": {"TLV_OK": 0}}],
                                       current, errors)
                self.assertTrue(errors)

    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.root = Path(self.directory.name)
        for relative, text in FILES.items():
            self.write(relative, text)
        self.contract, self.mapping = contract(), mapping()

    def tearDown(self):
        self.directory.cleanup()

    def write(self, relative, text):
        path = self.root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")

    def errors(self):
        self.write(CONTRACT, json.dumps(self.contract))
        self.write(MAPPING, json.dumps(self.mapping))
        return check(self.root)[0]

    def assert_error(self, fragment):
        errors = self.errors()
        self.assertTrue(any(fragment in error for error in errors), errors)

    def test_repository_contracts_hold(self):
        self.assertEqual([], check(ROOT)[0])

    def test_complete_fixture_passes(self):
        self.assertEqual([], self.errors())

    def test_every_file_needs_exactly_one_current_owner(self):
        self.write("src/extra.c", "")
        self.assert_error("assign an owner to src/extra.c")
        (self.root / "src/extra.c").unlink()
        self.contract["files"]["src/gone.c"] = "core"
        self.assert_error("remove stale src/gone.c")

    def test_undeclared_dependency_needs_a_recorded_resolution(self):
        self.write("src/document/document.c", '#include "opentlv/io/io.h"\n')
        self.assert_error('dependency violation including "opentlv/io/io.h"')
        self.contract["resolutions"]["move"] = {"issue": 1, "summary": "Move it."}
        self.contract["exceptions"].append({"kind": "dependency", "include": "opentlv/io/io.h",
                                            "resolution": "move",
                                            "files": ["src/document/document.c"]})
        self.assertEqual([], self.errors())
        self.write("src/document/document.c", '#include "opentlv/core/result.h"\n')
        self.assert_error("remove resolved dependency exception")

    def test_declared_dependency_and_comments_are_accepted(self):
        self.contract["targets"]["document"]["public"] = ["io"]
        self.write("src/document/document.c",
                   '#include "opentlv/io/io.h"\n// #include "../query/missing.h"\n'
                   '/* #include "tlv/missing.h" */\n')
        self.assertEqual([], self.errors())

    def test_private_headers_stay_inside_their_module(self):
        self.contract["targets"]["document"]["public"] = ["io"]
        self.write("src/document/document.c", '#include "../io/internal/io_internal.h"\n')
        self.assert_error('private-header violation including "../io/internal/io_internal.h"')
        self.write("src/document/document.c", "")
        self.write("src/io/tools.c", '#include "internal/io_internal.h"\n')
        self.contract["files"]["src/io/tools.c"] = "io-tools"
        self.assertEqual([], self.errors())

    def test_facade_dependencies_apply_only_to_cxx_headers(self):
        self.write("include/opentlv/builtin/nfc/nfc.hpp", '#include "opentlv/io/io.h"\n')
        self.assertEqual([], self.errors())
        self.write("src/builtin/nfc/nfc.c", '#include "opentlv/io/io.h"\n')
        self.contract["files"]["src/builtin/nfc/nfc.c"] = "nfc"
        self.assert_error("src/builtin/nfc/nfc.c: dependency violation")

    def test_relative_and_angle_private_header_bypasses_fail(self):
        self.contract["targets"]["document"]["public"] = ["io"]
        for name in ("../io/internal/io_internal.h",
                     "../query/../io/internal/io_internal.h",
                     "src/io/internal/io_internal.h"):
            for opening, closing in (('"', '"'), ('<', '>')):
                with self.subTest(name=name, delimiter=opening):
                    self.write("src/document/document.c", f'#include {opening}{name}{closing}\n')
                    self.assert_error("private-header violation")

    def test_builtin_cannot_include_generic_private_header(self):
        self.contract["targets"]["nfc"]["public"] = ["io"]
        self.contract["targets"]["nfc"]["facade"] = []
        self.write("src/builtin/nfc/nfc.c", '#include "../../io/internal/io_internal.h"\n')
        self.contract["files"]["src/builtin/nfc/nfc.c"] = "nfc"
        self.assert_error("private-header violation")

    def test_builtins_depend_only_on_generic_modules(self):
        self.contract["targets"]["emv"]["private"] = ["asn1"]
        self.assert_error("a builtin depends on another builtin asn1")

    def test_public_header_cannot_expose_own_private_header(self):
        self.write("include/opentlv/io/io.h", '#include "../../../src/io/internal/io_internal.h"\n')
        self.assert_error("private-header violation")

    def test_private_header_must_live_in_owned_internal_directory(self):
        path = "src/document/internal/foreign.h"
        self.write(path, "")
        self.contract["files"][path] = "io"
        self.assert_error("must belong under src/io/internal/")

    def test_historical_root_is_rejected(self):
        self.write("tlv/include/tlv/forward.h", "")
        self.assert_error("historical source root tlv must not exist")

    def test_macro_include_cannot_bypass_ownership(self):
        self.write("src/document/document.c", '#define HEADER "../io/internal/io_internal.h"\n'
                   '#include HEADER\n')
        self.assert_error("use a literal header name")

    def test_target_graph_must_be_acyclic_and_layered(self):
        cyclic = copy.deepcopy(self.contract)
        cyclic["targets"]["core"]["public"] = ["io"]
        self.contract = cyclic
        self.assert_error("dependency cycle core -> io")
        self.contract = contract()
        self.contract["targets"]["query"]["public"] = ["core", "asn1"]
        self.assert_error("generic target depends on builtin asn1")
        self.contract = contract()
        self.contract["targets"]["io-tools"]["public"] = ["core"]
        self.assert_error("an integration depends on its owning module")
        self.contract = contract()
        del self.contract["modules"]["lldp"]
        self.assert_error("seven generic and six builtin")

    def test_api_moves_and_resolutions_stay_current(self):
        self.contract["api"]["tlv_missing"] = {"target": "io", "resolution": "move"}
        self.assert_error("unknown resolution move")
        self.contract["resolutions"]["move"] = {"issue": 1, "summary": "Move it."}
        self.assert_error("remove stale tlv_missing")
        self.contract["api"] = {}
        self.assert_error("remove move, which no exception or API move uses")

    def test_mapping_covers_every_enumerator_without_merging(self):
        self.write("include/opentlv/core/result.h",
                   "typedef enum tlv_result { TLV_OK = 0, TLV_ERR_X, TLV_ERR_Y } tlv_result_t;\n")
        self.assert_error("core.result: map TLV_ERR_Y = 2")
        self.write("include/opentlv/core/result.h", "typedef enum tlv_result { TLV_OK = 0 } tlv_result_t;\n")
        self.assert_error("core.result: remove or correct stale TLV_ERR_X = 1")
        self.write("include/opentlv/core/result.h", FILES["include/opentlv/core/result.h"])
        self.mapping["families"][0]["codes"][1]["new"] = "TLV_OK"
        self.assert_error("TLV_ERR_X and TLV_OK merge into TLV_OK")

    def test_mapping_entries_need_owner_meaning_and_change(self):
        del self.mapping["families"][0]["codes"][0]["change"]
        self.assert_error("TLV_OK needs exactly")
        self.mapping = mapping()
        self.mapping["families"][0]["codes"][0]["module"] = "io"
        self.assert_error("TLV_OK cannot belong to io")
        self.mapping = mapping()
        self.mapping["families"][1]["codes"][0]["module"] = "schema"
        self.assertEqual([], self.errors())

    def test_domains_are_unique_and_inside_their_range(self):
        self.mapping["domains"]["query"]["id"] = 1
        self.assert_error("reuses id 0x0001")
        self.mapping = mapping()
        self.mapping["domains"]["query"]["id"] = 0x101
        self.assert_error("outside its range")
        self.mapping = mapping()
        self.mapping["families"][1]["id"] = 1
        self.mapping["families"][1]["domain"] = "core"
        self.assert_error("reuses family id 1")

    def test_limit_names_are_complete(self):
        self.write("src/query/query.c", 'int f(void* d) { return edit_limit(d, "depth", 1); }\n')
        self.assert_error('map limit name "depth"')
        self.assert_error('remove stale limit name "work"')

    def test_cycles_report_strongly_connected_components(self):
        self.assertEqual([["a", "b"], ["c"]],
                         cycles({"a": {"b"}, "b": {"a", "c"}, "c": {"c"}, "d": {"a"}}))


class ExampleContractTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.contract = contract()
        for path, text in FILES.items():
            self.write(path, text)
        self.write("examples/io/main.c", '#include <opentlv/io/io.h>\n')
        self.entry = {"target": "example-c-io-main", "language": "c", "sources": ["io/main.c"],
                      "test": "example.c.io.main", "query_frontend": False,
                      "link": ["OpenTLV::io"], "umbrella": False, "native_interop": False}

    def write(self, relative, text):
        path = self.root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")

    def errors(self, entries=None):
        registry = self.root / "examples.jsonl"
        registry.write_text("\n".join(json.dumps(entry) for entry in
                                     ([self.entry] if entries is None else entries)), encoding="utf-8")
        return check_examples(self.root, registry, self.contract)

    def assert_error(self, fragment):
        errors = self.errors()
        self.assertTrue(any(fragment in error for error in errors), errors)

    def test_only_direct_includes_need_links(self):
        # io.h includes Core; that exported dependency is deliberately not repeated.
        self.assertEqual([], self.errors())
        self.entry["link"].append("OpenTLV::core")
        self.assert_error("unused LINK OpenTLV::core")

    def test_transitive_link_cannot_hide_a_missing_direct_owner(self):
        self.contract["targets"]["document"]["public"].append("io")
        self.entry["link"] = ["OpenTLV::document"]
        self.assert_error("direct public includes require LINK OpenTLV::io")

    def test_integration_uses_its_own_public_target(self):
        self.contract["files"]["include/opentlv/io/tools.hpp"] = "io-tools"
        self.write("include/opentlv/io/tools.hpp", '#include <opentlv/io/io.h>\n')
        self.write("examples/io/main.c", '#include <opentlv/io/tools.hpp>\n')
        self.assert_error("require LINK OpenTLV::IOTools")
        self.entry["link"] = ["OpenTLV::IOTools"]
        self.assertEqual([], self.errors())

    def test_multiple_sources_and_local_header_dependencies(self):
        self.write("examples/io/extra.c", '#include "local.h"\n')
        self.write("examples/io/local.h", '#include "opentlv/core/result.h"\n')
        self.entry["sources"].append("io/extra.c")
        self.assert_error("require LINK OpenTLV::core")
        self.entry["link"].append("OpenTLV::core")
        self.assertEqual([], self.errors())

    def test_generated_public_headers_use_existing_ownership(self):
        self.contract["generated"]["opentlv/io/export.h"] = "io"
        self.write("examples/io/main.c", '#include <opentlv/io/export.h>\n')
        self.assertEqual([], self.errors())

    def test_public_headers_cannot_use_build_tree_relative_paths(self):
        self.write("examples/io/main.c", '#include "../../include/opentlv/io/io.h"\n')
        self.assert_error("public include spelling opentlv/io/io.h")

    def test_public_target_names_must_be_complete_and_unique(self):
        del self.contract["modules"]["io"]["public_target"]
        self.assert_error("declare its public_target")
        self.contract["modules"]["io"]["public_target"] = "OpenTLV::core"
        self.assert_error("duplicate public target")

    def test_comments_and_string_literals_do_not_add_dependencies(self):
        self.write("examples/io/main.c", '#include <opentlv/io/io.h>\n'
                   '// #include "opentlv/core/result.h"\n'
                   'const char* example = R"(\n#include "opentlv/core/result.h"\n)";\n')
        self.assertEqual([], self.errors())

    def test_private_missing_and_dynamic_includes_fail(self):
        self.contract["files"]["include/opentlv/io/detail/hidden.hpp"] = "io"
        self.write("include/opentlv/io/detail/hidden.hpp", "")
        for header, fragment in (("../../src/io/internal/io_internal.h", "private or non-public"),
                                 ("opentlv/io/detail/hidden.hpp", "private or non-public"),
                                 ("opentlv/missing.h", "cannot resolve")):
            with self.subTest(header=header):
                self.write("examples/io/main.c", f'#include "{header}"\n')
                self.assert_error(fragment)
        self.write("examples/io/main.c", '#include HEADER\n')
        self.assert_error("literal header")

    def test_registry_must_cover_every_source_once(self):
        self.write("examples/io/unregistered.cpp", "")
        self.assert_error("absent from the CMake registry")
        (self.root / "examples/io/unregistered.cpp").unlink()
        errors = self.errors([self.entry, self.entry])
        self.assertTrue(any("duplicate target" in error for error in errors))
        self.assertTrue(any("duplicate test" in error for error in errors))
        self.assertTrue(any("duplicate example source" in error for error in errors))
        self.entry["sources"] = ["../src/io/io.c"]
        self.assert_error("stay inside examples/")

    def test_unknown_duplicate_and_stale_declarations_fail(self):
        self.entry["link"] = ["OpenTLV::Typo"]
        self.assert_error("unknown public LINK")
        self.entry["link"] = ["OpenTLV::io", "OpenTLV::io"]
        self.assert_error("duplicate LINK")
        self.entry["link"] = ["OpenTLV::io"]
        self.entry["umbrella"] = True
        self.assert_error("stale umbrella")
        self.entry["umbrella"] = False
        self.entry["language"] = "cpp"
        self.assert_error("does not match language")

    def test_composite_header_owners_come_from_public_includes(self):
        self.contract["targets"]["aggregate"] = {"module": None, "role": "aggregate",
                                                  "public_target": "OpenTLV::tlv", "public": ["io", "core"]}
        for header in ("native.hpp", "tlv.hpp"):
            self.contract["files"]["include/opentlv/" + header] = "aggregate"
            self.write("include/opentlv/" + header,
                       '#include <opentlv/io/io.h>\n#include <opentlv/core/result.h>\n')
        self.write("examples/io/main.c", '#include <opentlv/native.hpp>\n')
        self.assert_error("module-specific headers")
        self.entry["native_interop"] = True
        self.assert_error("require LINK OpenTLV::core")
        self.entry["link"].append("OpenTLV::core")
        self.assertEqual([], self.errors())
        self.write("examples/io/main.c", '#include <opentlv/tlv.hpp>\n')
        self.entry["native_interop"] = False
        self.assert_error("module-specific headers")
        self.entry["umbrella"] = True
        self.entry["link"] = ["OpenTLV::tlv"]
        self.assertEqual([], self.errors())

    def test_empty_malformed_and_missing_registries_fail(self):
        self.assertTrue(any("empty" in error for error in self.errors([])))
        self.assertTrue(any("malformed" in error for error in self.errors([{}])))
        self.assertTrue(any("cannot load" in error for error in
                            check_examples(self.root, self.root / "missing.jsonl", self.contract)))
        self.write("bad.jsonl", "not JSON")
        self.assertTrue(any("cannot load" in error for error in
                            check_examples(self.root, self.root / "bad.jsonl", self.contract)))

    def test_test_metadata_is_required_and_typed(self):
        for key, value in (("test", None), ("test", ""), ("query_frontend", "OFF")):
            with self.subTest(key=key, value=value):
                entry = dict(self.entry, **{key: value})
                self.assertTrue(any("malformed" in error for error in self.errors([entry])))

    @unittest.skipUnless(shutil.which("cmake"), "CMake is required for registry emission")
    def test_cmake_emits_skipped_declarations_and_resets_registry(self):
        # No compiler is needed: both declarations must be recorded before their skip.
        self.write("examples/io/part.cpp", "")
        self.write("examples/io/main.cpp", "")
        helper = (ROOT / "examples/cmake/Example.cmake").as_posix()
        self.write("examples/CMakeLists.txt", f'''cmake_minimum_required(VERSION 3.16)
project(RegistryTest LANGUAGES NONE)
include("{helper}")
set(OPENTLV_EXAMPLE_SOURCE_DIR "${{CMAKE_CURRENT_SOURCE_DIR}}")
set(OPENTLV_EXAMPLE_REGISTRY "${{CMAKE_CURRENT_BINARY_DIR}}/examples.jsonl")
file(WRITE "${{OPENTLV_EXAMPLE_REGISTRY}}" "")
set(OPENTLV_BUILD_CXX OFF)
set(OPENTLV_QUERY_FRONTEND OFF)
set(public_links OpenTLV::io OpenTLV::core)
opentlv_example(cpp io skipped SOURCES io/main.cpp io/part.cpp LINK ${{public_links}})
opentlv_example(c query skipped SOURCES io/main.c LINK OpenTLV::query QUERY_FRONTEND)
''')
        build = self.root / "build with spaces"
        command = ["cmake", "-S", str(self.root / "examples"), "-B", str(build)]
        for _ in range(2):
            result = subprocess.run(command, text=True, capture_output=True, check=False)
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)
            entries = [json.loads(line) for line in (build / "examples.jsonl").read_text().splitlines()]
            self.assertEqual(2, len(entries))
            self.assertEqual(["io/main.cpp", "io/part.cpp"], entries[0]["sources"])
            self.assertEqual(["OpenTLV::io", "OpenTLV::core"], entries[0]["link"])
            self.assertEqual("example.cpp.io.skipped", entries[0]["test"])
            self.assertFalse(entries[0]["query_frontend"])
            self.assertEqual("example.c.query.skipped", entries[1]["test"])
            self.assertTrue(entries[1]["query_frontend"])
            self.assertIn("OPENTLV_BUILD_CXX=OFF", result.stdout)
            self.assertIn("OPENTLV_QUERY_FRONTEND=OFF", result.stdout)


class ExampleInventoryTests(unittest.TestCase):
    def setUp(self):
        self.entries = [
            {"test": "example.c.io.write", "language": "c", "query_frontend": False, "umbrella": False},
            {"test": "example.cpp.io.quick-start", "language": "cpp", "query_frontend": False, "umbrella": True},
            {"test": "example.c.query.compile", "language": "c", "query_frontend": True, "umbrella": False},
            {"test": "example.cpp.query.compile", "language": "cpp", "query_frontend": True, "umbrella": False},
        ]

    def test_configuration_selects_declared_tests(self):
        for cxx in (False, True):
            for frontend in (False, True):
                for aggregate in (False, True):
                    with self.subTest(cxx=cxx, frontend=frontend, aggregate=aggregate):
                        expected = {"example.c.io.write"}
                        if cxx and aggregate:
                            expected.add("example.cpp.io.quick-start")
                        if frontend:
                            expected.add("example.c.query.compile")
                            if cxx:
                                expected.add("example.cpp.query.compile")
                        self.assertEqual(expected, expected_example_tests(
                            self.entries, cxx=cxx, query_frontend=frontend, aggregate=aggregate))

    def test_new_declaration_is_automatically_expected(self):
        self.entries.append({"test": "example.c.future.new-name", "language": "c",
                             "query_frontend": False, "umbrella": False})
        self.assertIn("example.c.future.new-name", expected_example_tests(
            self.entries, cxx=False, query_frontend=False, aggregate=False))

    def test_exact_inventory_accepts_different_order(self):
        check_example_tests({"tests": [{"name": "second"}, {"name": "first"}]}, {"first", "second"})

    def test_missing_unexpected_renamed_and_duplicate_tests_fail(self):
        for names, message in ((["first"], "missing=['second']"),
                               (["first", "second", "extra"], "unexpected=['extra']"),
                               (["first", "renamed"], "missing=['second'], unexpected=['renamed']"),
                               (["first", "second", "second"], "duplicates=['second']")):
            with self.subTest(names=names):
                with self.assertRaises(RuntimeError) as raised:
                    check_example_tests({"tests": [{"name": name} for name in names]}, {"first", "second"})
                self.assertIn(message, str(raised.exception))


@unittest.skipUnless(shutil.which("cmake"), "CMake is required for standalone package checks")
class StandaloneExampleTests(unittest.TestCase):
    def configure(self, package, declaration):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "OpenTLVConfig.cmake").write_text(package, encoding="utf-8")
            helper = (ROOT / "examples/cmake/Example.cmake").as_posix()
            (root / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.16)
project(Consumer LANGUAGES NONE)
set(OPENTLV_EXAMPLES_STANDALONE ON)
set(OPENTLV_BUILD_CXX ON)
set(OPENTLV_EXAMPLE_SOURCE_DIR "${{CMAKE_CURRENT_SOURCE_DIR}}")
set(OPENTLV_EXAMPLE_REGISTRY "${{CMAKE_CURRENT_BINARY_DIR}}/examples.jsonl")
include("{helper}")
{declaration}
''', encoding="utf-8")
            result = subprocess.run(["cmake", "-S", str(root), "-B", str(root / "build"),
                                     f"-DOpenTLV_DIR={root.as_posix()}",
                                     "-DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF",
                                     "-DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=OFF"],
                                    text=True, capture_output=True, check=False)
            return result.returncode, result.stdout + result.stderr

    def test_unavailable_optional_aggregate_skips_explicitly(self):
        code, output = self.configure("set(OpenTLV_tlv_FOUND FALSE)\n",
                                     "opentlv_example(cpp io quick-start SOURCES main.cpp LINK OpenTLV::tlv UMBRELLA)")
        self.assertEqual(0, code, output)
        self.assertIn("Skipping example-cpp-io-quick-start: optional OpenTLV::tlv component is unavailable", output)

    def test_advertised_optional_component_must_export_its_target(self):
        code, output = self.configure("set(OpenTLV_tlv_FOUND TRUE)\n",
                                     "opentlv_example(cpp io quick-start SOURCES main.cpp LINK OpenTLV::tlv UMBRELLA)")
        self.assertNotEqual(0, code, output)
        self.assertIn("missing expected exported target OpenTLV::tlv", output)

    def test_required_component_cannot_skip(self):
        code, output = self.configure("set(OpenTLV_FOUND FALSE)\n",
                                     "opentlv_example(c io main SOURCES main.c LINK OpenTLV::IO)")
        self.assertNotEqual(0, code, output)
        self.assertIn("OpenTLV_FOUND", output)
        self.assertNotIn("Skipping", output)

    def test_missing_required_export_fails_even_when_package_reports_success(self):
        code, output = self.configure("set(OpenTLV_IO_FOUND TRUE)\n",
                                     "opentlv_example(c io main SOURCES main.c LINK OpenTLV::IO)")
        self.assertNotEqual(0, code, output)
        self.assertIn("missing expected exported target OpenTLV::IO", output)

    def test_missing_export_fails_before_a_frontend_feature_probe_or_skip(self):
        code, output = self.configure("set(OpenTLV_QueryDocument_FOUND TRUE)\n",
                                     "opentlv_example(c query main SOURCES main.c LINK OpenTLV::QueryDocument QUERY_FRONTEND)")
        self.assertNotEqual(0, code, output)
        self.assertIn("missing expected exported target", output)
        self.assertIn("OpenTLV::QueryDocument", output)
        self.assertNotIn("Skipping", output)


if __name__ == "__main__":
    unittest.main()
