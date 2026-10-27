# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Marek Cingel

"""Regression checks for the centralized source license policy."""

import tempfile
import unittest
from pathlib import Path

import check_license_headers as policy


# REUSE-IgnoreStart
# License strings inside fixtures are test data, not this file's licensing.
class LicenseHeadersTest(unittest.TestCase):
    def test_coverage_and_exclusions(self):
        for name in ("tlv/a.c", "tlv/a.h", "tlv++/a.hpp", "tests/a.cpp",
                     "tlv/resources/config.h.in", "tests/cases.inc", "docs/a.js"):
            self.assertEqual(policy.comment_prefix(name), "//")
        self.assertEqual(policy.comment_prefix("bindings/a.py"), "#")
        self.assertEqual(policy.comment_prefix("bindings/a.lua"), "--")
        for name in ("vendor/a.c", "third_party/lib/a.h", "generated/a.cpp",
                     "LICENSE", "data.bin", "CMakeLists.txt"):
            self.assertIsNone(policy.comment_prefix(name))

    def test_fix_preserves_preamble_bytes_and_is_idempotent(self):
        cases = [("a.c", b"#include <stdio.h>\r\n"),
                 ("a.py", b"#!/usr/bin/python3\n# coding: utf-8\nprint('ok')\n"),
                 ("a.py", b"# comment\n# coding: utf-8\nprint('ok')\n"),
                 ("a.lua", b"#!/usr/bin/lua\nreturn {}\n"),
                 ("a.hpp", b"\xef\xbb\xbf#pragma once\n")]
        with tempfile.TemporaryDirectory() as directory:
            for name, original in cases:
                with self.subTest(name=name, original=original):
                    path = Path(directory) / name
                    path.write_bytes(original)
                    self.assertFalse(policy.check_file(path, name))
                    self.assertTrue(policy.check_file(path, name, fix=True))
                    updated = path.read_bytes()
                    offset = policy.header_offset(original, name)
                    self.assertEqual(updated[:offset], original[:offset])
                    self.assertTrue(updated.endswith(original[offset:]))
                    self.assertTrue(policy.check_file(path, name, fix=True))
                    self.assertEqual(path.read_bytes(), updated)
                    if b"\r\n" in original:
                        self.assertNotIn(b"\n", updated.replace(b"\r\n", b""))

    def test_conflicting_notice_is_rejected_without_modification(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "a.c"
            for original in (b"// SPDX-License-Identifier: Apache-2.0\n",
                             b"// Copyright someone else\n"):
                path.write_bytes(original)
                self.assertFalse(policy.check_file(path, "a.c", fix=True))
                self.assertEqual(path.read_bytes(), original)

    def test_contributor_notices_and_years_are_preserved(self):
        notices = [
            b"// Copyright (c) 2025-2026 Another Contributor\n",
            b"// SPDX-FileCopyrightText: 2024 Another Contributor\n",
            b"// Copyright (c) 2026 Marek Cingel\n// Copyright 2027 Another Contributor\n",
        ]
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "a.c"
            for notice in notices:
                original = b"// SPDX-License-Identifier: MIT\n" + notice + b"\nint x;\n"
                path.write_bytes(original)
                self.assertTrue(policy.check_file(path, "a.c", fix=True))
                self.assertEqual(path.read_bytes(), original)

    def test_empty_or_nonleading_copyright_does_not_pass(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "a.c"
            for tail in (b"// Copyright (c)\n", b"// SPDX-FileCopyrightText:\n",
                         b"int x;\n// Copyright 2026 Someone\n"):
                path.write_bytes(b"// SPDX-License-Identifier: MIT\n" + tail)
                self.assertFalse(policy.check_file(path, "a.c"))
# REUSE-IgnoreEnd


if __name__ == "__main__":
    unittest.main()
