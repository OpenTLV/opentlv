"""Check the quality CLI's build, test and analysis commands without launching tools."""

import errno
import os
from pathlib import Path
import runpy
import subprocess
import sys
import unittest
from unittest.mock import call, patch


SCRIPT = Path(__file__).resolve().with_name("run_quality.py")
ROOT = SCRIPT.parents[1]


def run_cli(*args):
    with patch.object(sys, "argv", [str(SCRIPT), *args]):
        runpy.run_path(str(SCRIPT), run_name="__main__")


def command(*args):
    return call(args, cwd=ROOT, check=True)


class QualityRunner(unittest.TestCase):
    def setUp(self):
        child = patch("subprocess.run")
        self.child = child.start()
        self.addCleanup(child.stop)

    def test_format_runs_only_the_format_check(self):
        run_cli("format")
        self.assertEqual(self.child.call_args_list, [
            command(sys.executable, "-B", "tests/check_source_quality.py", "--check", "format"),
        ])

    def test_contracts_build_and_check_each_configuration(self):
        work = Path("contract work").resolve()
        build = work / "opentlv-tests"
        archive = str(work / "GoogleTest source.tar.gz")
        with patch.dict(os.environ, OPENTLV_GOOGLETEST_URL=archive, OPENTLV_GOOGLE_BENCHMARK_URL="unused"):
            run_cli("contracts", "--work-dir", "contract work")
        expected = [command("cmake", "-S", ROOT, "-B", build, "-G", "Ninja Multi-Config",
                            "-DBUILD_TESTING=ON", "-DCMAKE_C_COMPILER=gcc-13", "-DCMAKE_CXX_COMPILER=g++-13",
                            f"-DOPENTLV_GOOGLETEST_URL={archive}")]
        for config in ("Debug", "Release"):
            expected.extend([
                command("cmake", "--build", build, "--config", config),
                command(sys.executable, "-B", "tests/run_ctest.py", build, config,
                        work / "ctest-reports" / config, "--require-x64"),
            ])
        self.assertEqual(self.child.call_args_list, expected)

    def test_analysis_forwards_benchmark_headers_and_runs_quality_tests(self):
        work = Path("analysis work").resolve()
        build = work / "quality-headers"
        gtest = str(work / "GoogleTest source.tar.gz")
        benchmark = str(work / "Benchmark source.tar.gz")
        with patch.dict(os.environ, OPENTLV_GOOGLETEST_URL=gtest, OPENTLV_GOOGLE_BENCHMARK_URL=benchmark):
            run_cli("analysis", "--work-dir", "analysis work")
        self.assertEqual(self.child.call_args_list, [
            command("cmake", "-S", ROOT, "-B", build, "-G", "Ninja", "-DBUILD_TESTING=ON",
                    "-DCMAKE_C_COMPILER=gcc-13", "-DCMAKE_CXX_COMPILER=g++-13", "-DOPENTLV_BUILD_BENCHMARKS=ON",
                    f"-DOPENTLV_GOOGLETEST_URL={gtest}", f"-DOPENTLV_GOOGLE_BENCHMARK_URL={benchmark}"),
            command(sys.executable, "-B", "tests/check_source_quality.py",
                    "--clang-arg=--gcc-install-dir=/usr/lib/gcc/x86_64-linux-gnu/13",
                    "--system-include", build / "_deps/googletest-src/googletest/include",
                    "--system-include", build / "_deps/googlebenchmark-src/include"),
            command(sys.executable, "-B", "-m", "unittest", "discover", "-s", "tests", "-p", "test_*quality*.py"),
        ])

    def test_default_build_locations_without_archive_overrides(self):
        for action, directory in (("contracts", "opentlv-tests"), ("analysis", "quality-headers")):
            with self.subTest(action=action), patch.dict(os.environ, clear=True):
                self.child.reset_mock()
                run_cli(action)
                configure = self.child.call_args_list[0].args[0]
                self.assertEqual(configure[configure.index("-B") + 1], ROOT / "build" / "quality" / directory)
                overrides = ("-DOPENTLV_GOOGLETEST_URL=", "-DOPENTLV_GOOGLE_BENCHMARK_URL=")
                self.assertFalse(any(str(arg).startswith(overrides) for arg in configure))

    def test_failed_debug_ctest_returns_its_status_and_stops_before_release(self):
        self.child.side_effect = [None, None, subprocess.CalledProcessError(23, "ctest")]
        with self.assertRaises(SystemExit) as failure:
            run_cli("contracts")
        self.assertEqual(failure.exception.code, 23)
        self.assertEqual(self.child.call_count, 3)

    def test_missing_tool_names_the_executable_and_stops(self):
        self.child.side_effect = FileNotFoundError(errno.ENOENT, "executable missing", "cmake")
        with self.assertRaisesRegex(SystemExit, "cmake") as failure:
            run_cli("analysis")
        # Python prints a string exit reason on stderr and returns status 1.
        self.assertIsInstance(failure.exception.code, str)
        self.child.assert_called_once()
