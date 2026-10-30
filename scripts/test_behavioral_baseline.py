#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Marek Cingel
"""Reject incomplete, altered or misattributed baseline evidence."""
import copy
import io
from pathlib import Path
import tarfile
import tempfile
import unittest
from unittest.mock import patch

import behavioral_baseline as baseline


class BaselineTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.release = {"tag_name": baseline.TAG, "published_at": "2026-10-27T13:18:59Z",
                        "draft": False, "prerelease": False, "html_url": "release"}
        self.run = {"id": int(baseline.RUN), "head_sha": baseline.COMMIT,
                    "conclusion": "success", "status": "completed", "html_url": "ci"}
        self.manifest = {"commit": baseline.COMMIT, "candidate": {"tag": baseline.TAG},
                         "quality": {"commit": baseline.COMMIT, "tier": "full", "run_id": baseline.RUN,
                                     "checks": {name: "success" for name in (
                                         "clang", "msvc", "static", "consumers", "codeql", "gcc",
                                         "properties", "query", "fuzz", "capabilities", "abi", "memcheck")}}}

    def save(self):
        for name, data in (("release.json", self.release), ("ci-run.json", self.run),
                           ("release-manifest.json", self.manifest)):
            baseline.write(self.root / name, data)

    def test_release_does_not_approve_baseline(self):
        self.save()
        self.assertEqual("pending", baseline.check_provenance(self.root)["approval"]["status"])

    def test_rejects_unpublished_reference(self):
        for key, value in (("draft", True), ("prerelease", True), ("published_at", None)):
            original = self.release.copy()
            self.release[key] = value
            self.save()
            with self.assertRaises(ValueError):
                baseline.check_provenance(self.root)
            self.release = original

    def test_rejects_wrong_commit_or_ci(self):
        for key, value in (("head_sha", "0" * 40), ("conclusion", "failure"), ("id", 1)):
            original = self.run.copy()
            self.run[key] = value
            self.save()
            with self.assertRaises(ValueError):
                baseline.check_provenance(self.root)
            self.run = original

    def test_rejects_missing_or_failed_full_gate(self):
        original = copy.deepcopy(self.manifest)
        for change in ("missing", "failure", "skipped", "main"):
            self.manifest = copy.deepcopy(original)
            quality = self.manifest["quality"]
            if change == "missing":
                quality["checks"].pop("fuzz")
            elif change == "main":
                quality["tier"] = "main"
            else:
                quality["checks"]["fuzz"] = change
            self.save()
            with self.assertRaises(ValueError):
                baseline.check_provenance(self.root)

    def test_ctest_must_have_passing_scenarios(self):
        xml = self.root / "ctest.xml"
        for body in ("", '<testcase name="x" status="notrun" time="0"/>',
                     '<testcase name="x" status="run" time="0"><failure/></testcase>',
                     '<testcase name="x" status="run" time="0"><skipped/></testcase>'):
            xml.write_text(f"<testsuite>{body}</testsuite>")
            with self.assertRaises(ValueError):
                baseline.collect_ctest(xml)
        xml.write_text('<testsuite><testcase name="x" status="run" time="0.1"/></testsuite>')
        self.assertEqual("passed", baseline.collect_ctest(xml)[0]["status"])

    def test_source_inventory_is_byte_exact(self):
        source = self.root / "source"
        source.mkdir()
        (source / "wire.bin").write_bytes(b"\0\r\n\xff")
        with patch.object(baseline, "archive", return_value={"wire.bin": b"\0\n\xff"}):
            with self.assertRaisesRegex(ValueError, "differs"):
                baseline.prepare_source(self.root, source)

    def test_archive_ignores_checkout_line_ending_preferences(self):
        buffer = io.BytesIO()
        with tarfile.open(fileobj=buffer, mode="w") as archive:
            member = tarfile.TarInfo("source.c")
            member.size = 2
            archive.addfile(member, io.BytesIO(b"x\n"))
        with patch.object(baseline, "command", side_effect=[baseline.COMMIT.encode(), buffer.getvalue()]) as run:
            self.assertEqual({"source.c": b"x\n"}, baseline.archive(self.root))
        self.assertEqual(["git", "-c", "core.autocrlf=false", "-c", "core.eol=lf"],
                         run.call_args.args[0][:5])

    def test_generator_requires_all_domains(self):
        with self.assertRaisesRegex(ValueError, "ten property"):
            baseline.collect_generator(self.root, self.root / "output")

    def test_reference_diagnostics_exclude_explicit_post_baseline_families(self):
        import check_module_contracts as contracts
        original = {"family": "core.result", "header": "result.h", "typedef": "result",
                    "codes": [{"previous": "OK", "value": 0}]}
        added = {"family": "query.conversion", "baseline": False}
        with patch.object(baseline, "read", return_value={"families": [original, added]}), \
                patch.object(contracts, "parse_enum", return_value=[("OK", 0)]) as parse:
            baseline.collect_diagnostics(self.root, self.root / "output")
        parse.assert_called_once_with(self.root, "result.h", "result")
        self.assertEqual([{"family": "core.result", "header": "result.h", "codes": {"OK": 0}}],
                         baseline.read(self.root / "output/diagnostic-codes.json"))

    def test_performance_requires_complete_successful_repetitions(self):
        with self.assertRaises(ValueError):
            baseline.check_performance({"benchmarks": []})
        with self.assertRaises(ValueError):
            baseline.check_performance({"benchmarks": [{"error_occurred": True}]})
        with self.assertRaises(ValueError):
            baseline.check_performance({"benchmarks": [{"run_type": "iteration",
                "run_name": "reader_decode/fixed_tlv", "repetition_index": 0, "iterations": 1}]})

    def test_archive_metadata_overrides_parent_checkout(self):
        with patch.object(baseline, "archive", return_value={"source.c": b"source"}):
            files = baseline.prepare_source(self.root, self.root / "source")
        self.assertEqual(f"version=0.8.0\ncommit={baseline.COMMIT}\n".encode("ascii"),
                         files["opentlv-source.txt"])
        header = self.root / "generated/include/tlv/version.h"
        header.parent.mkdir(parents=True)
        header.write_text('#define OPENTLV_VERSION_STRING "0.8.0"\n'
                          '#define OPENTLV_VERSION_REVISION 2\n')
        with self.assertRaisesRegex(ValueError, "incorrect generated reference version"):
            baseline.check_version(self.root)

    def test_checksum_catches_mutation_and_extra_files(self):
        self.save()
        baseline.write(self.root / "reference.json", baseline.check_provenance(self.root))
        baseline.write(self.root / "source-inventory.json", baseline.inventory({
            "opentlv-source.txt": f"version=0.8.0\ncommit={baseline.COMMIT}\n".encode("ascii")}))
        baseline.write(self.root / "checksums.json", baseline.inventory({
            p.name: p.read_bytes() for p in self.root.iterdir()}))
        with patch.object(baseline, "archive", return_value={}):
            baseline.verify(self.root, self.root)
            (self.root / "extra.json").write_text("{}")
            with self.assertRaisesRegex(ValueError, "checksum"):
                baseline.verify(self.root, self.root)
            (self.root / "extra.json").unlink()
            nested = self.root / "nested/checksums.json"
            baseline.write(nested, {})
            with self.assertRaisesRegex(ValueError, "checksum"):
                baseline.verify(self.root, self.root)
            nested.unlink()
            (self.root / "ci-run.json").write_text((self.root / "ci-run.json").read_text() + " ")
            with self.assertRaisesRegex(ValueError, "checksum"):
                baseline.verify(self.root, self.root)


if __name__ == "__main__":
    unittest.main()
