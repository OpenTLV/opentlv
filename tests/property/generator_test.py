#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Marek Cingel
"""Exercise the native property adapter without any CLI dependency."""

from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

GENERATOR = str(Path(sys.argv.pop(1)).resolve())


class NativeGenerator(unittest.TestCase):
    def generate(self, directory, seed="0", count="8", case_size="4096"):
        return subprocess.run(
            [GENERATOR, "fixed", str(directory), seed, count, "0", "32", "512",
             case_size, "1", "1", "big"], capture_output=True, timeout=30)

    def test_seed_zero_replays_exact_bytes(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            result = self.generate(root)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(result.stdout, b"")
            self.assertEqual(result.stderr, b"")
            before = {p.name: p.read_bytes() for p in root.glob("*.bin")}
            self.assertEqual(sorted(before), [f"{i:06d}.bin" for i in range(8)])
            self.assertTrue(all(before.values()))
            result = self.generate(root)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(before, {p.name: p.read_bytes() for p in root.glob("*.bin")})
            result = self.generate(root, seed=str(2**64 - 1))
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertNotEqual(before, {p.name: p.read_bytes() for p in root.glob("*.bin")})

    def test_invalid_inputs_and_impossible_limits_leave_no_cases(self):
        for options in ({"seed": "-1"}, {"seed": str(2**64)}, {"count": "0"},
                        {"case_size": "1"}):
            with self.subTest(options=options), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                result = self.generate(root, **options)
                self.assertNotEqual(result.returncode, 0)
                self.assertEqual(list(root.iterdir()), [])


if __name__ == "__main__":
    unittest.main()
