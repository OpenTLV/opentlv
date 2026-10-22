#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Marek Cingel
"""Check standalone output selection without requiring installed bindings."""

from contextlib import redirect_stderr, redirect_stdout
import io
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import generate_diagnostic_vocabulary as generator


class VocabularyTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name) / "core"
        self.root.mkdir()
        self.families = generator.load()
        self.patch_root = patch.object(generator, "ROOT", self.root)
        self.patch_root.start()
        self.addCleanup(self.patch_root.stop)
        self.patch_load = patch.object(generator, "load", return_value=self.families)
        self.patch_load.start()
        self.addCleanup(self.patch_load.stop)
        for path, _ in generator.OUTPUTS:
            (self.root / path).parent.mkdir(parents=True, exist_ok=True)

    def run_generator(self, *args):
        with redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
            return generator.main(list(args))

    def test_default_generation_and_check_need_no_standalone_checkout(self):
        self.assertEqual(self.run_generator(), 0)
        self.assertEqual(self.run_generator("--check"), 0)
        for option, *_ in generator.STANDALONE:
            self.assertFalse((self.root / "bindings" / option).exists())
        output = self.root / generator.OUTPUTS[0][0]
        output.write_text("stale", encoding="utf-8")
        self.assertEqual(self.run_generator("--check"), 1)
        self.assertEqual(output.read_text(encoding="utf-8"), "stale")

    def test_explicit_external_output_and_read_only_drift_check(self):
        for option, _, outputs, _ in generator.STANDALONE:
            with self.subTest(option=option):
                destination = Path(self.temp.name) / option
                for path, _ in outputs:
                    (destination / path).parent.mkdir(parents=True, exist_ok=True)
                args = (f"--{option}-output-root", str(destination))
                self.assertEqual(self.run_generator(*args), 0)
                for path, render in outputs:
                    self.assertEqual((destination / path).read_bytes(),
                                     render(self.families).encode("utf-8"))
                self.assertEqual(self.run_generator("--check", *args), 0)
                output = destination / outputs[0][0]
                output.write_text("stale", encoding="utf-8")
                self.assertEqual(self.run_generator("--check", *args), 1)
                self.assertEqual(output.read_text(encoding="utf-8"), "stale")
                output.unlink()
                self.assertEqual(self.run_generator("--check", *args), 1)
                self.assertFalse(output.exists())
                self.assertFalse((self.root / "bindings" / option).exists())

    def test_reject_core_and_incomplete_destinations_before_writing(self):
        for option, *_ in generator.STANDALONE:
            for destination in (self.root, self.root / "nested", Path(self.temp.name) / "missing"):
                with self.subTest(option=option, destination=destination), \
                        self.assertRaises(SystemExit) as error:
                    self.run_generator(f"--{option}-output-root", str(destination))
                self.assertEqual(error.exception.code, 2)
        for path, _ in generator.OUTPUTS:
            self.assertFalse((self.root / path).exists())


if __name__ == "__main__":
    unittest.main()
