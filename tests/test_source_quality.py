"""Check that the source-quality command catches real diagnostics in C99 and C++17."""

import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
RUNNER = Path("tests/check_source_quality.py")


class SourceQuality(unittest.TestCase):
    def test_clean_and_failing_sources(self):
        for name in ("src/probe.c", "tests/probe.cpp", "include/standalone.h"):
            with self.subTest(name=name), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                for relative in (RUNNER, Path(".clang-format"), Path(".clang-tidy")):
                    (root / relative).parent.mkdir(parents=True, exist_ok=True)
                    shutil.copyfile(ROOT / relative, root / relative)
                probe = root / name
                probe.parent.mkdir(parents=True, exist_ok=True)
                # A build tree may contain generated sources and fetched dependencies.
                excluded = probe.parent / "_deps/example-src/broken.cpp"
                excluded.parent.mkdir(parents=True)
                excluded.write_text("#error out of scope\n", encoding="utf-8")

                def check(*args):
                    return subprocess.run([sys.executable, "-B", root / RUNNER, *args], cwd=root,
                                          stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                          text=True, errors="replace")

                probe.write_text("int probe(void);\n", encoding="utf-8")
                clean = check()
                self.assertEqual(clean.returncode, 0, clean.stdout)
                probe.write_text("int      probe(    void );\n", encoding="utf-8")
                formatting = check("--check", "format")
                self.assertNotEqual(formatting.returncode, 0, formatting.stdout)
                self.assertIn("clang-format-violations", formatting.stdout)

                null = "0" if probe.suffix == ".c" else "nullptr"
                body = f"int probe(void)\n{{\n    int *p = {null};\n    return *p;\n}}\n"
                # The .h probe only fails when analyzed as a C++17 consumer would see it.
                mode = "__STDC_VERSION__ == 199901L" if probe.suffix == ".c" else "__cplusplus == 201703L"
                probe.write_text(f"#if {mode}\n{body}#endif\n", encoding="utf-8")
                for tool, diagnostic in (("tidy", "clang-analyzer-core.NullDereference"),
                                         ("cppcheck", "nullPointer")):
                    with self.subTest(tool=tool):
                        result = check("--check", tool)
                        self.assertNotEqual(result.returncode, 0, result.stdout)
                        self.assertIn(diagnostic, result.stdout)
                        self.assertIn(probe.name, result.stdout)


if __name__ == "__main__":
    unittest.main()
