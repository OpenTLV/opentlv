# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Marek Cingel
"""Regression checks for archive aggregation and section-tool formats."""

import unittest

from report import dumpbin_sections, sysv_sections


class SectionReportTests(unittest.TestCase):
    def test_sysv_combines_members_and_preserves_section_names(self):
        output = """a.o   (ex lib.a):
section                       size    addr
.text                           10       0
.rodata.long section name        2       0
Total                           12

b.o   (ex lib.a):
section     size   addr
.text         15      0
.bss           0      0
Total         15
"""
        self.assertEqual(sysv_sections(output), {".text": 25, ".rodata.long section name": 2,
                                                 ".bss": 0})

    def test_sysv_rejects_empty_incomplete_and_mismatched_tables(self):
        partial = "lib.so  :\nsection    size    addr\n.text        10     256\n"
        for invalid in ("", partial, partial + "Total 9\n"):
            with self.subTest(invalid=invalid), self.assertRaises(ValueError):
                sysv_sections(invalid)

    def test_dumpbin_reads_hexadecimal_summary(self):
        output = "Dump of file core.lib\nFile Type: LIBRARY\n  Summary\n\n   2A .text\n    0 .bss\n"
        self.assertEqual(dumpbin_sections(output), {".text": 42, ".bss": 0})
        with self.assertRaises(ValueError):
            dumpbin_sections("File Type: LIBRARY")
