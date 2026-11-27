"""Exercise release evidence comparison through the contributor quality CLI."""

from contextlib import ExitStack, redirect_stderr
import io
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

import run_quality


CHECKOUT = run_quality.ROOT
BASELINE = "a" * 40
CANDIDATE = "b" * 40


class QualityPerformanceTests(unittest.TestCase):
    def setUp(self):
        stack = ExitStack()
        self.addCleanup(stack.close)
        self.root = Path(stack.enter_context(tempfile.TemporaryDirectory())).resolve()
        self.reports = self.root / "collected reports"
        self.output = self.root / "comparison output"
        self.pin = self.root / "benchmarks/performance-baseline.txt"
        self.pin.parent.mkdir()
        self.pin.write_text(BASELINE + "\n", encoding="ascii")
        stack.enter_context(patch.object(run_quality, "ROOT", self.root))
        self.git = stack.enter_context(patch.object(subprocess, "check_output", return_value=CANDIDATE + "\n"))

    def compare(self, *options):
        argv = ["run_quality.py", "compare-performance", "--reports", str(self.reports),
                "--output", str(self.output), *options]
        with patch.object(sys, "argv", argv):
            run_quality.main()

    def assert_comparison(self, child, baseline, candidate):
        child.assert_called_once_with(
            (sys.executable, "-B", "benchmarks/compare_performance.py",
             "--baseline", self.reports / "baseline", f"--baseline-revision={baseline}",
             "--candidate", self.reports / "candidate", f"--candidate-revision={candidate}",
             "--output", self.output),
            cwd=self.root, check=True,
        )

    def test_pin_and_git_head_are_forwarded(self):
        with patch.object(subprocess, "run") as child:
            self.compare()
        self.git.assert_called_once_with(["git", "rev-parse", "HEAD"], cwd=self.root, text=True)
        self.assert_comparison(child, BASELINE, CANDIDATE)

    def test_explicit_candidate_skips_git(self):
        revision = "c" * 40
        with patch.object(subprocess, "run") as child:
            self.compare("--candidate-revision", revision)
        self.git.assert_not_called()
        self.assert_comparison(child, BASELINE, revision)

    def test_failed_head_lookup_stops_before_comparison(self):
        self.git.side_effect = subprocess.CalledProcessError(128, ["git", "rev-parse", "HEAD"])
        with patch.object(subprocess, "run") as child, self.assertRaises(subprocess.CalledProcessError) as failure:
            self.compare()
        self.assertEqual(failure.exception.returncode, 128)
        child.assert_not_called()

    def test_unreadable_pin_reports_error_and_forwards_empty_revision(self):
        self.pin.unlink()
        for contents, diagnostic in ((None, self.pin.name), (b"\xff", "ascii")):
            with self.subTest(contents=contents), patch.object(subprocess, "run") as child:
                if contents is not None:
                    self.pin.write_bytes(contents)
                with redirect_stderr(io.StringIO()) as errors:
                    self.compare("--candidate-revision", CANDIDATE)
                self.assertIn(diagnostic, errors.getvalue())
                self.assert_comparison(child, "", CANDIDATE)

    def test_invalid_revisions_publish_failing_comparison(self):
        with patch.object(sys, "path", [str(CHECKOUT / "benchmarks"), *sys.path]):
            from test_compare_performance import write_evidence

        write_evidence(self.reports / "baseline", BASELINE, ns=20)
        # Give the actual CLI and comparator their own checkout and editable pin.
        runner = self.root / "tests/run_quality.py"
        runner.parent.mkdir()
        shutil.copyfile(CHECKOUT / "tests/run_quality.py", runner)
        for script in (CHECKOUT / "benchmarks").glob("*.py"):
            shutil.copyfile(script, self.pin.parent / script.name)

        for baseline, candidate, role in (("short", CANDIDATE, "baseline"), ("", CANDIDATE, "baseline"),
                                          (BASELINE, "g" * 40, "candidate"), (BASELINE, "", "candidate")):
            with self.subTest(baseline=baseline, candidate=candidate):
                self.pin.write_text(baseline, encoding="ascii")
                result = subprocess.run(
                    [sys.executable, "-B", runner, "compare-performance", "--reports", self.reports,
                     "--output", self.output, "--candidate-revision", candidate],
                    cwd=self.root, capture_output=True, encoding="utf-8",
                    env={**os.environ, "PYTHONIOENCODING": "utf-8"},
                )
                self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
                report = json.loads((self.output / "comparison.json").read_text(encoding="utf-8"))
                self.assertEqual(report["status"], "invalid")
                self.assertEqual(report["baseline_revision"], baseline)
                self.assertEqual(report["candidate_revision"], candidate)
                self.assertIn(f"{role}: expected a full Git revision", report["error"])
                self.assertIn(report["error"], result.stdout)
                self.assertIn(report["error"], (self.output / "summary.md").read_text(encoding="utf-8"))
