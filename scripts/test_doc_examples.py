#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Marek Cingel
"""Check build registration discovery after grouping examples by module."""

from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import check_doc_examples


class RegistrationTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        patcher = patch.object(check_doc_examples, "ROOT", self.root)
        patcher.start()
        self.addCleanup(patcher.stop)

    def write(self, relative, content=""):
        path = self.root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content, encoding="utf-8")
        return path

    def test_module_and_nested_builtin_registrations(self):
        for folder, filename in (("io", "write.cpp"), ("builtins/asn1", "ber.c"),
                                 ("builtins/nfc/type2", "type2.c")):
            with self.subTest(folder=folder):
                source = self.write(f"examples/{folder}/{filename}")
                self.write(f"examples/{folder}/CMakeLists.txt",
                           f"opentlv_example(c module demo SOURCES {filename} LINK OpenTLV::IO)")
                self.assertTrue(check_doc_examples.is_built(source))

    def test_parent_registration_can_list_multiple_sources(self):
        source = self.write("examples/builtins/emv/detail/decode.c")
        self.write("examples/builtins/emv/CMakeLists.txt",
                   "opentlv_example(c emv demo SOURCES main.c detail/decode.c LINK OpenTLV::EMV)")
        self.assertTrue(check_doc_examples.is_built(source))

    def test_another_module_does_not_register_same_filename(self):
        source = self.write("examples/io/write.c")
        self.write("examples/core/CMakeLists.txt", "SOURCES write.c")
        self.assertFalse(check_doc_examples.is_built(source))

    def test_repository_root_is_not_an_example_registration(self):
        source = self.write("examples/io/write.c")
        self.write("CMakeLists.txt", "examples/io/write.c")
        self.assertFalse(check_doc_examples.is_built(source))
        self.assertFalse(check_doc_examples.is_built(self.write("tests/write.c")))


if __name__ == "__main__":
    unittest.main()
