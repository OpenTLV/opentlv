"""Regression fixtures for the symbol tools used by the package audit."""

import unittest
from pathlib import Path
from unittest.mock import patch

from check_symbols import mingw_exports, msvc_symbols, nm_symbols


MSVC_DLL = """\
           1 ordinal base
           2 number of functions
           2 number of names

    ordinal hint RVA      name

          1    0 00001070 tlv_read
          2    1 00001130 tlv_write

  Summary
        1000 .text
"""

MSVC_ARCHIVE = """\
    3 public symbols

      BB8 _tlv_read
     1050 ??_C@_07IPNMCAN@success@
     1ED0 _tlv_write

FILE HEADER VALUES
             14C machine (x86)
"""

MINGW = """\
Number in:
        Export Address Table            00000002
        [Name Pointer/Ordinal] Table    00000002
Table Addresses
        Export Address Table            00003000

Export Address Table -- Ordinal Base 1
        [   0] +base[   1] 00001070 Export RVA
        [   1] +base[   2] 00001130 Export RVA

[Ordinal/Name Pointer] Table
        [   0] tlv_read
        [   1] tlv_write

PE File Base Relocations (interpreted .reloc section contents)
"""


class SymbolTables(unittest.TestCase):
    def test_nm_archive_members_optional_size_and_data(self):
        output = "lib.a[first.o]:\ntlv_read T 0 28\n\nlib.a[second.o]:\ntlv_write T 0 \nleak D A0 4\n"
        with patch("check_symbols.tool_output", return_value=output):
            self.assertEqual(nm_symbols(Path("lib.a"), "nm", False, "objdump"),
                             {"tlv_read", "tlv_write", "leak (D)"})

    def test_nm_elf_runtime_hook_requires_matching_address(self):
        output = "tlv_read T 1030 18\n_init T 1000 \n_fini T 1010 \n_extra T 1020 4\n"
        dynamic = "Dynamic Section:\n  INIT                 0x0000000000001000\n  FINI  0x0000000000001100\n"
        with patch("check_symbols.tool_output", side_effect=[output, dynamic]):
            self.assertEqual(nm_symbols(Path("lib.so"), "nm", True, "objdump"),
                             {"tlv_read", "_fini", "_extra"})

    def test_nm_rejects_empty_or_malformed_output(self):
        for output in ("archive.a[member.o]:\n", "tlv_read T 10 2\nleak T invalid\n",
                       "tlv_read T 10 2\nbad format:", "tlv_read T 10 2 extra field\n"):
            with self.subTest(output=output), patch("check_symbols.tool_output", return_value=output):
                with self.assertRaises(SystemExit):
                    nm_symbols(Path("lib.a"), "nm", False, "objdump")

    def test_elf_rejects_unrecognized_dynamic_section_or_runtime_entry(self):
        for dynamic in ("unrecognized format", "Dynamic Section:\n  INIT 0x1000\n  FINI invalid\n"):
            with self.subTest(dynamic=dynamic), patch("check_symbols.tool_output",
                                                     side_effect=["tlv_read T 10 2\n", dynamic]):
                with self.assertRaises(SystemExit):
                    nm_symbols(Path("lib.so"), "nm", True, "objdump")

    def test_msvc_exports_and_archive_decoration(self):
        x64 = MSVC_ARCHIVE.replace("14C machine (x86)", "8664 machine (x64)")
        for output, shared, expected in (
                (MSVC_DLL, True, {"tlv_read", "tlv_write"}),
                (MSVC_DLL.replace("tlv_read", "tlv_read = _tlv_read"), True, {"tlv_read", "tlv_write"}),
                (MSVC_ARCHIVE, False, {"tlv_read", "tlv_write"}),
                (x64, False, {"_tlv_read", "_tlv_write"})):
            with self.subTest(output=output), patch("check_symbols.tool_output", return_value=output):
                self.assertEqual(msvc_symbols(Path("library"), "link", shared), expected)

    def test_msvc_rejects_missing_truncated_or_malformed_tables(self):
        for output, shared in (
                ("File Type: DLL", True), ("File Type: LIBRARY", False),
                (MSVC_DLL.replace("00001070", "no-rva"), True),
                (MSVC_DLL.replace("1    0 00001070 tlv_read", "1    00001070 [NONAME]"), True),
                (MSVC_DLL.replace("2 number of names", "3 number of names"), True),
                (MSVC_DLL.replace("\n  Summary", "\n  unparsable row\n\n  Summary"), True),
                (MSVC_ARCHIVE.replace("BB8 _tlv_read", "bogus _tlv_read"), False),
                (MSVC_ARCHIVE.replace("3 public symbols", "4 public symbols"), False)):
            with self.subTest(output=output), patch("check_symbols.tool_output", return_value=output):
                with self.assertRaises(SystemExit):
                    msvc_symbols(Path("library"), "link", shared)

    def test_mingw_binutils_name_table_layouts(self):
        modern = MINGW.replace("Ordinal Base 1\n", "Ordinal Base 1\n                 Ordinal  Address  Type\n")
        modern = modern.replace("[Ordinal/Name Pointer] Table\n",
                                "[Ordinal/Name Pointer] Table -- Ordinal Base 1\n                 Ordinal   Hint Name\n")
        modern = modern.replace("0] tlv_read", "0] +base[   1]  0000 tlv_read")
        modern = modern.replace("1] tlv_write", "1] +base[   2]  0001 tlv_write")
        for output in (MINGW, modern):
            with self.subTest(output=output), patch("check_symbols.tool_output", return_value=output):
                self.assertEqual(mingw_exports(Path("library.dll"), "objdump"), {"tlv_read", "tlv_write"})

    def test_pe_export_ordinal_gaps(self):
        msvc = MSVC_DLL.replace("2 number of functions", "3 number of functions").replace("2    1", "3    1")
        mingw = MINGW.replace("Export Address Table            00000002", "Export Address Table            00000003")
        mingw = mingw.replace("[   1] +base[   2]", "[   2] +base[   3]").replace("[   1] tlv", "[   2] tlv")
        with patch("check_symbols.tool_output", side_effect=[msvc, mingw]):
            self.assertEqual(msvc_symbols(Path("lib.dll"), "link", True), {"tlv_read", "tlv_write"})
            self.assertEqual(mingw_exports(Path("lib.dll"), "objdump"), {"tlv_read", "tlv_write"})

    def test_mingw_rejects_unknown_rows_missing_tables_and_unnamed_ordinals(self):
        for output in ("file format pei-x86-64", MINGW.partition("[Ordinal/Name Pointer]")[0],
                       MINGW.replace("00001070 Export RVA", "invalid Export RVA"),
                       MINGW.replace("Export RVA\n\n", "Export RVA\n\n        unexpected row\n\n"),
                       MINGW.replace("[   0] tlv_read", "invalid tlv_read"),
                       MINGW.replace("[   0] tlv_read", "[   0] extra column tlv_read"),
                       MINGW.replace("[   0] tlv_read", "[   1] tlv_read"),
                       MINGW.replace("        [   0] tlv_read\n", "")):
            with self.subTest(output=output), patch("check_symbols.tool_output", return_value=output):
                with self.assertRaises(SystemExit):
                    mingw_exports(Path("library.dll"), "objdump")
