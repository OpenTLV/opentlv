#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Marek Cingel
"""Failure cases for final baseline scenario/corpus coverage."""

from pathlib import Path
import tempfile
import unittest

from behavioral_baseline import digest
from verify_modular_baseline import collect_gtest, corpus_coverage, scenario_coverage


class CoverageTests(unittest.TestCase):
    def test_opaque_parameter_names_preserve_case_multiplicity(self):
        old = [{"name": "Nfc.Decode/32-byte object <AA>"},
               {"name": "Nfc.Decode/32-byte object <BB>"}]
        new = [{"name": "Nfc.Decode/32-byte object <CC>"}]
        with self.assertRaisesRegex(ValueError, "expected_count"):
            scenario_coverage(old, new, {})
        new.append({"name": "Nfc.Decode/32-byte object <DD>"})
        self.assertEqual(2, scenario_coverage(old, new, {})["baseline"])

    def test_gtest_output_is_required_and_skips_fail(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            with self.assertRaisesRegex(ValueError, "missing"):
                collect_gtest(root)
            path = root / "suite.xml"
            path.write_text('<testsuites><testcase classname="Nfc" name="Decode/0" '
                            'value_param="32-byte object &lt;AA&gt;" status="run" '
                            'result="completed"/></testsuites>')
            self.assertEqual("Nfc.Decode/32-byte object <AA>", collect_gtest(root)[0]["name"])
            path.write_text(path.read_text().replace('result="completed"', 'result="skipped"'))
            with self.assertRaisesRegex(ValueError, "did not pass"):
                collect_gtest(root)

    def test_new_tests_cannot_hide_removed_scenarios(self):
        with self.assertRaisesRegex(ValueError, "old"):
            scenario_coverage([{"name": "old"}], [{"name": "new"}], {})

    def test_renamed_test_requires_existing_replacement_and_reason(self):
        old, new = [{"name": "old"}], [{"name": "new"}]
        self.assertEqual(1, scenario_coverage(old, new, {
            "old": {"tests": ["new"], "reason": "Moved to owning module."}})["baseline"])
        for migration in ({"tests": ["absent"], "reason": "Renamed"},
                          {"tests": ["new"], "reason": ""}, {"tests": [], "reason": "Removed"}):
            with self.assertRaises(ValueError):
                scenario_coverage(old, new, {"old": migration})

    def test_stale_migrations_fail(self):
        with self.assertRaisesRegex(ValueError, "stale"):
            scenario_coverage([], [], {"old": {"tests": ["new"], "reason": "Moved"}})

    def test_shared_replacements_require_combined_multiplicity(self):
        old = [{"name": "first"}, {"name": "first"}, {"name": "second"}]
        migrations = {name: {"tests": ["shared", "other"], "reason": "Moved to owning module."}
                      for name in ("first", "second")}
        for shared_count, other_count in ((2, 3), (3, 2), (3, 3)):
            with self.subTest(shared=shared_count, other=other_count):
                current = ([{"name": "shared"}] * shared_count
                           + [{"name": "other"}] * other_count)
                if min(shared_count, other_count) < 3:
                    with self.assertRaisesRegex(ValueError, '"expected_count": 3'):
                        scenario_coverage(old, current, migrations)
                else:
                    self.assertEqual(3, scenario_coverage(old, current, migrations)["baseline"])

    def test_replacements_cannot_reuse_unchanged_scenario_observations(self):
        old = [{"name": "renamed"}, {"name": "retained"}]
        current = [{"name": "retained"}]
        migrations = {"renamed": {"tests": ["retained"], "reason": "Moved to owning module."}}
        with self.assertRaisesRegex(ValueError, '"expected_count": 2'):
            scenario_coverage(old, current, migrations)
        current.append({"name": "retained"})
        self.assertEqual(2, scenario_coverage(old, current, migrations)["baseline"])

    def test_migration_of_still_present_scenario_fails(self):
        with self.assertRaisesRegex(ValueError, "invalid scenario migration"):
            scenario_coverage([{"name": "old"}], [{"name": "old"}, {"name": "new"}], {
                "old": {"tests": ["new"], "reason": "Moved to owning module."}})

    def test_fuzz_seed_paths_may_move_but_bytes_must_survive(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            seed = root / "tests/fuzz/new/corpus/case"
            seed.parent.mkdir(parents=True)
            seed.write_bytes(b"\0\xff\n")
            reference = {"old/case": {"sha256": digest(seed.read_bytes())}}
            self.assertEqual(1, corpus_coverage(root, reference)["baseline_seeds"])
            seed.write_bytes(b"\0\xff\r\n")
            with self.assertRaisesRegex(ValueError, "missing"):
                corpus_coverage(root, reference)


if __name__ == "__main__":
    unittest.main()
