#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Marek Cingel
"""Prove the real CMake target reports diagnostics in public C and C++ headers."""

import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
TIDY = shutil.which("clang-tidy-18") or shutil.which("clang-tidy")
TOOLS_AVAILABLE = TIDY and shutil.which("cmake") and shutil.which("ninja")


@unittest.skipUnless(TOOLS_AVAILABLE, "requires CMake, Ninja and clang-tidy")
class HeaderDiagnosticsTests(unittest.TestCase):
    def test_public_headers_fail_the_aggregate_target(self):
        with tempfile.TemporaryDirectory(prefix="opentlv-tidy-") as temporary:
            # Exercise both shell quoting and regex escaping of the checkout path.
            source = Path(temporary) / "source [1]+"
            build = Path(temporary) / "build"
            files = {
                ".clang-tidy": "Checks: '-*,readability-braces-around-statements'\n"
                "WarningsAsErrors: '*'\n",
                "include/opentlv/probe.h": self.header("c_probe", broken=False),
                "include/opentlv/probe.hpp": self.header("cpp_probe", broken=False),
                # A diagnostic outside the public tree must remain filtered out.
                "private/probe.h": self.header("private_probe", broken=True),
                "probe.c": '#include "opentlv/probe.h"\n#include "private/probe.h"\n'
                'int probe(void) { return c_probe(1) + private_probe(1); }\n',
                "tests/unit/test_tlvpp.cpp": '#include "opentlv/probe.hpp"\n'
                'int cpp_use() { return cpp_probe(1); }\n',
                "CMakeLists.txt": f'''cmake_minimum_required(VERSION 3.16)
project(OpenTLV LANGUAGES C CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(OPENTLV_BUILD_CXX ON)
set(OPENTLV_BUILD_TESTS ON)
set(OPENTLV_BUILD_UNIT_TESTS ON)
add_library(probe OBJECT probe.c tests/unit/test_tlvpp.cpp)
target_include_directories(probe PRIVATE "${{CMAKE_CURRENT_SOURCE_DIR}}/include"
    "${{CMAKE_CURRENT_SOURCE_DIR}}")
set_property(TARGET probe PROPERTY INTERFACE_OPENTLV_SOURCES probe.c)
set_property(GLOBAL PROPERTY OPENTLV_MODULES probe)
set_property(GLOBAL PROPERTY OPENTLV_MODULE_probe probe)
include("{ROOT.as_posix()}/cmake/clang_tidy.cmake")
opentlv_add_clang_tidy_targets()
''',
            }
            for name, content in files.items():
                path = source / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(content, encoding="utf-8")
            configured = self.run_command("cmake", "-S", str(source), "-B", str(build),
                                          "-G", "Ninja",
                                          f"-DOPENTLV_CLANG_TIDY_EXECUTABLE={TIDY}")
            self.assertEqual(configured.returncode, 0, configured.stdout)
            command = ("cmake", "--build", str(build), "--target", "clang-tidy")
            clean = self.run_command(*command)
            self.assertEqual(clean.returncode, 0, clean.stdout)
            self.assertIn("Running clang-tidy static analysis on tlv++", clean.stdout)

            for filename, function in (("probe.h", "c_probe"), ("probe.hpp", "cpp_probe")):
                with self.subTest(header=filename):
                    header = source / "include/opentlv" / filename
                    header.write_text(self.header(function, broken=True), encoding="utf-8")
                    result = self.run_command(*command)
                    self.assertNotEqual(result.returncode, 0, result.stdout)
                    self.assertIn(f"{filename}:2:", result.stdout)
                    self.assertIn("[readability-braces-around-statements", result.stdout)
                    header.write_text(self.header(function, broken=False), encoding="utf-8")
            restored = self.run_command(*command)
            self.assertEqual(restored.returncode, 0, restored.stdout)

    @staticmethod
    def header(function, *, broken):
        branch = "if (value) return 1;" if broken else "if (value) { return 1; }"
        return f"static inline int {function}(int value) {{\n  {branch}\n  return 0;\n}}\n"

    @staticmethod
    def run_command(*command):
        return subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                              text=True, check=False)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--require-tools", action="store_true")
    args, remaining = parser.parse_known_args()
    if args.require_tools and not TOOLS_AVAILABLE:
        parser.error("CMake, Ninja and clang-tidy must be installed")
    unittest.main(argv=[__file__, *remaining])
