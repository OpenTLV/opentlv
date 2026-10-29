#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Marek Cingel
"""Regression cases for the functional module contract check."""
import copy
import json
from pathlib import Path
import tempfile
import unittest
from check_module_contracts import BUILTIN, CONTRACT, GENERIC, MAPPING, ROOT, check, cycles

FILES = {
    "tlv/include/tlv/result.h": "typedef enum tlv_result {\n    TLV_OK = 0, /**< ok */\n"
                                "    TLV_ERR_X\n} tlv_result_t;\n",
    "tlv/src/result.c": '#include "tlv/result.h"\n',
    "tlv/include/tlv/io.h": '#include "tlv/result.h"\n',
    "tlv/src/io/io.c": '#include "tlv/io.h"\n#include "io_internal.h"\n',
    "tlv/src/io/io_internal.h": '#include "tlv/io.h"\n',
    "tlv/src/document/document.c": '#include "tlv/result.h"\n',
    "tlv/src/query/query.c": 'static int f(void* d) { return query_limit(d, "work", 1, 0, 0); }\n',
    "tlv++/include/tlv++/nfc.hpp": '#include "tlv/result.h"\n',
}
OWNERS = {
    "tlv/include/tlv/result.h": "core",
    "tlv/src/result.c": "core",
    "tlv/include/tlv/io.h": "io",
    "tlv/src/io/io.c": "io",
    "tlv/src/io/io_internal.h": "io",
    "tlv/src/document/document.c": "document",
    "tlv/src/query/query.c": "query",
    "tlv++/include/tlv++/nfc.hpp": "nfc",
}


def contract():
    modules = {name: {"kind": "generic" if name in GENERIC else "builtin"}
               for name in GENERIC + BUILTIN}
    targets = {name: {"module": name, "role": "module", "public": [] if name == "core" else ["core"]}
               for name in modules}
    targets["io-tools"] = {"module": "io", "role": "integration", "public": ["io"]}
    targets["nfc"]["facade"] = ["io"]
    return {"modules": modules, "targets": targets, "generated": {}, "files": dict(OWNERS),
            "api": {}, "resolutions": {}, "exceptions": []}


def mapping():
    return {
        "ranges": {"generic": [1, 255], "builtin": [256, 511]},
        "domains": {"core": {"id": 1, "module": "core"}, "query": {"id": 4, "module": "query"}},
        "families": [
            {"family": "core.result", "id": 1, "domain": "core", "header": "tlv/include/tlv/result.h",
             "typedef": "tlv_result", "codes": [code("TLV_OK", 0), code("TLV_ERR_X", 1)]},
            {"family": "query.limit", "id": None, "domain": "query",
             "codes": [code("work", None, "query")]},
        ],
    }


def code(name, value, module="core", new=None):
    return {"previous": name, "value": value, "new": new or name, "module": module,
            "meaning": "Meaning.", "change": "None."}


class ModuleContractTests(unittest.TestCase):
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
        self.write("tlv/src/extra.c", "")
        self.assert_error("assign an owner to tlv/src/extra.c")
        (self.root / "tlv/src/extra.c").unlink()
        self.contract["files"]["tlv/src/gone.c"] = "core"
        self.assert_error("remove stale tlv/src/gone.c")

    def test_undeclared_dependency_needs_a_recorded_resolution(self):
        self.write("tlv/src/document/document.c", '#include "tlv/io.h"\n')
        self.assert_error('dependency violation including "tlv/io.h"')
        self.contract["resolutions"]["move"] = {"issue": 1, "summary": "Move it."}
        self.contract["exceptions"].append({"kind": "dependency", "include": "tlv/io.h",
                                            "resolution": "move",
                                            "files": ["tlv/src/document/document.c"]})
        self.assertEqual([], self.errors())
        self.write("tlv/src/document/document.c", '#include "tlv/result.h"\n')
        self.assert_error("remove resolved dependency exception")

    def test_declared_dependency_and_comments_are_accepted(self):
        self.contract["targets"]["document"]["public"] = ["io"]
        self.write("tlv/src/document/document.c",
                   '#include "tlv/io.h"\n// #include "../query/missing.h"\n'
                   '/* #include "tlv/missing.h" */\n')
        self.assertEqual([], self.errors())

    def test_private_headers_stay_inside_their_module(self):
        self.contract["targets"]["document"]["public"] = ["io"]
        self.write("tlv/src/document/document.c", '#include "../io/io_internal.h"\n')
        self.assert_error('private-header violation including "../io/io_internal.h"')
        self.write("tlv/src/document/document.c", "")
        self.write("tlv/src/io/tools.c", '#include "io_internal.h"\n')
        self.contract["files"]["tlv/src/io/tools.c"] = "io-tools"
        self.assertEqual([], self.errors())

    def test_facade_dependencies_apply_only_to_cxx_headers(self):
        self.write("tlv++/include/tlv++/nfc.hpp", '#include "tlv/io.h"\n')
        self.assertEqual([], self.errors())
        self.write("tlv/src/nfc.c", '#include "tlv/io.h"\n')
        self.contract["files"]["tlv/src/nfc.c"] = "nfc"
        self.assert_error("tlv/src/nfc.c: dependency violation")

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
        self.write("tlv/include/tlv/result.h",
                   "typedef enum tlv_result { TLV_OK = 0, TLV_ERR_X, TLV_ERR_Y } tlv_result_t;\n")
        self.assert_error("core.result: map TLV_ERR_Y = 2")
        self.write("tlv/include/tlv/result.h", "typedef enum tlv_result { TLV_OK = 0 } tlv_result_t;\n")
        self.assert_error("core.result: remove or correct stale TLV_ERR_X = 1")
        self.write("tlv/include/tlv/result.h", FILES["tlv/include/tlv/result.h"])
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
        self.write("tlv/src/query/query.c", 'int f(void* d) { return edit_limit(d, "depth", 1); }\n')
        self.assert_error('map limit name "depth"')
        self.assert_error('remove stale limit name "work"')

    def test_cycles_report_strongly_connected_components(self):
        self.assertEqual([["a", "b"], ["c"]],
                         cycles({"a": {"b"}, "b": {"a", "c"}, "c": {"c"}, "d": {"a"}}))


if __name__ == "__main__":
    unittest.main()
