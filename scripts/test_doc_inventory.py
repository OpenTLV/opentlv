#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Marek Cingel

"""Negative fixtures for documentation inventory drift detection."""

import tempfile
import unittest
from pathlib import Path

from check_doc_inventory import check


class InventoryTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        files = {
            "CMakeLists.txt": 'option(OPENTLV_FORMAT_BER "BER" ON)\n',
            "tlv/resources/config.h.in": "#cmakedefine01 OPENTLV_FORMAT_BER\n",
            "docs/concepts/architecture.md": "| `OPENTLV_FORMAT_BER` | BER |\n",
            "tlv/include/tlv/builtins/asn1/ber.h": "extern TLV_API const tlv_format_t tlv_format_ber;\n",
            "tlv++/include/tlv++/builtins/asn1/ber.hpp": "// fixture\n",
            "docs/formats/support.md": "| BER | `tlv_format_ber` | component | [ber.hpp](../../tlv++/include/tlv++/builtins/asn1/ber.hpp) |\n",
            "bindings/wasm/examples/parse.mjs": "// example\n",
            "bindings/common/query_schema.h": "/* shared native ownership helper */\n",
            "docs/development/webassembly.md": "# WebAssembly\n",
            "docs/guides/cxx-examples.md": "# C++ examples\n",
            "docs/guides/examples.md": "<!-- example-inventory:start -->\n[WebAssembly](../../bindings/wasm/examples/parse.mjs)\n<!-- example-inventory:end -->\n",
            "tools/docs/Doxyfile.in": 'INPUT = "@PROJECT_SOURCE_DIR@/tlv/include/tlv"\nRECURSIVE = YES\nFILE_PATTERNS = *.h *.dox\n',
            "tools/docs/Doxyfile.cxx.in": 'INPUT = "@PROJECT_SOURCE_DIR@/tlv++/include/tlv++"\nRECURSIVE = YES\nFILE_PATTERNS = *.hpp *.dox\n',
        }
        for path, text in files.items():
            self.write(path, text)

    def write(self, path, text):
        file = self.root / path
        file.parent.mkdir(parents=True, exist_ok=True)
        file.write_text(text, encoding="utf-8")

    def assert_drift(self, text):
        self.assertTrue(any(text in error for error in check(self.root)), check(self.root))

    def test_consistent_inventory(self):
        self.assertFalse((self.root / "mkdocs.yml").exists())
        self.assertEqual(check(self.root), [])

    def test_added_component_needs_option_and_documentation(self):
        self.write("tlv/resources/config.h.in", "#cmakedefine01 OPENTLV_FORMAT_BER\n#cmakedefine01 OPENTLV_NEW\n")
        self.assert_drift("OPENTLV_NEW: public configuration has no root CMake option")
        self.assert_drift("OPENTLV_NEW: add a build-configuration row")

    def test_added_and_removed_native_descriptors(self):
        self.write("tlv/include/tlv/builtins/asn1/ber.h", "TLV_API extern const tlv_format_t tlv_format_new;\n")
        self.assert_drift("document tlv_format_new")
        self.assert_drift("stale tlv_format_ber")

    def test_new_component_option_requires_public_config(self):
        self.write("CMakeLists.txt", 'option(OPENTLV_FORMAT_BER "BER" ON)\noption(OPENTLV_NEW "New" ON)\n')
        self.assert_drift("public component configuration: document OPENTLV_NEW")

    def test_added_cxx_family(self):
        self.write("tlv++/include/tlv++/builtins/new/format.hpp", "// fixture\n")
        self.assert_drift("C++ built-in families: document new")

    def test_binding_addition_requires_guide(self):
        self.write("bindings/new/README.md", "# New\n")
        self.assert_drift("binding new: add docs/guides/new.md")

    def test_missing_cxx_guide(self):
        (self.root / "docs/guides/cxx-examples.md").unlink()
        self.assert_drift("C++: add docs/guides/cxx-examples.md")

    def test_example_addition_and_removal(self):
        (self.root / "bindings/wasm/examples/parse.mjs").unlink()
        self.write("bindings/wasm/examples/new.mjs", "// example\n")
        self.assert_drift("document bindings/wasm/examples/new.mjs")
        self.assert_drift("stale bindings/wasm/examples/parse.mjs")

    def test_missing_example_markers(self):
        self.write("docs/guides/examples.md", "# Examples\n")
        self.assert_drift("missing example-inventory markers")

    def test_api_input_must_be_recursive_and_include_header_extension(self):
        self.write("tools/docs/Doxyfile.cxx.in", 'INPUT = "other"\nSTRIP_FROM_PATH = "@PROJECT_SOURCE_DIR@/tlv++/include/tlv++"\nRECURSIVE = NO\nFILE_PATTERNS = *.dox\n')
        self.assert_drift("INPUT must include tlv++/include/tlv++")
        self.assert_drift("enable recursive public-header discovery")
        self.assert_drift("FILE_PATTERNS must include *.hpp")


if __name__ == "__main__":
    unittest.main()
