# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Marek Cingel
"""Negative tests for the release equivalence gate's evidence checks."""

from collections import Counter
import copy
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import check_distribution_equivalence as gate


class EvidenceTests(unittest.TestCase):
    def test_configuration_mismatches_fail(self):
        config = dict(VERSION="0.9.0", REVISION="2", COMMIT="abcdef12", BRANCH="main", TAG="")
        metadata = {key.lower(): value for key, value in config.items()}
        metadata.update(schema=1, library="OpenTLV", configuration={"query_frontend": True,
                        "query_set_operations": False}, targets=[{"key": "core"}])
        modules = [{"key": "core", "type": "STATIC_LIBRARY"}]
        gate.check_metadata(metadata, config, modules, True, False)
        for field in ("version", "revision", "commit", "branch", "tag", "configuration", "targets"):
            with self.subTest(field=field):
                changed = copy.deepcopy(metadata)
                changed[field] = [] if field == "targets" else "different"
                with self.assertRaises(ValueError):
                    gate.check_metadata(changed, config, modules, True, False)
        with self.assertRaises(ValueError):
            gate.check_metadata(dict(metadata, selection={"requested": ["Core"]}), config, modules,
                                True, False)

    def test_missing_extra_and_duplicate_scenarios_fail(self):
        reference = Counter({"valid": 1, "invalid": 2})
        for current in (Counter(valid=1, invalid=1), Counter(valid=1, invalid=2, extra=1),
                        Counter(valid=2, invalid=2)):
            with self.assertRaisesRegex(ValueError, "scenario inventory differs"):
                gate.equal_scenarios(reference, current, "amalgamation")

    def test_failed_skipped_and_empty_ctest_fail(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "ctest.xml"
            for content in ('', '<testcase name="bad" status="fail"><failure/></testcase>',
                            '<testcase name="skip" status="notrun"><skipped/></testcase>'):
                path.write_text(f"<testsuite>{content}</testsuite>")
                with self.assertRaises(ValueError):
                    gate.scenarios(path)
            path.write_text('<testsuite><testcase name="ok" status="run"/></testsuite>')
            self.assertEqual(gate.scenarios(path), Counter(ok=1))

    def test_changed_diagnostics_and_corpus_bytes_fail_with_variant(self):
        reference = {"diagnostic_strings": "invalid tag", "generator_bytes": {"0.bin": "abc"}}
        gate.equal_observations(reference, reference, "installed")
        for field in reference:
            changed = dict(reference, **{field: "different"})
            with self.assertRaisesRegex(ValueError, f"amalgamation:.*{field}"):
                gate.equal_observations(reference, changed, "amalgamation")

    def test_effective_flags_include_configuration_and_environment_flags(self):
        with tempfile.TemporaryDirectory() as directory:
            build = Path(directory)
            (build / "CMakeCache.txt").write_text(
                "CMAKE_C_FLAGS:STRING=-fsanitize=address,undefined\n"
                "CMAKE_C_FLAGS_RELEASE:STRING=-O3 -DNDEBUG\n"
                "CMAKE_CXX_FLAGS:STRING=/DTEST\n"
                "CMAKE_CXX_FLAGS_RELEASE:STRING=/O2 /DNDEBUG\n")
            self.assertEqual(gate.compiler_flags(build, "Release"), {
                "C": "-fsanitize=address,undefined -O3 -DNDEBUG", "CXX": "/DTEST /O2 /DNDEBUG"})

    def test_duplicate_external_definitions_fail(self):
        with patch.object(gate, "run", return_value=b"000 T tlv_read\n001 T tlv_read\n"):
            with self.assertRaisesRegex(ValueError, "duplicate"):
                gate.symbols("nm", ["one.a", "two.a"], Path("symbols.log"))
        with patch.object(gate, "run", return_value=b"000 T tlv_read\n000 U tlv_write\n"):
            self.assertEqual(gate.symbols("nm", ["one.a"], Path("symbols.log")),
                             Counter(tlv_read=1))


if __name__ == "__main__":
    unittest.main()
