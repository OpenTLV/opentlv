"""Regression checks for the per-workload Callgrind evidence."""

import tempfile
import unittest
from pathlib import Path

from run_callgrind import VERSION, compare_runs, read_counts, workload_ids


CASE = "v1/field-write/le4/offset0/complete"
C = "c99/" + CASE
CPP = "cpp17/" + CASE


def profile(trigger, total):
    return (f"# callgrind format\nversion: 1\ncreator: callgrind-{VERSION}\n"
            f"desc: Trigger: {trigger}\nevents: Ir\nsummary: {total}\n\n"
            f"totals: {total}\n")


class CallgrindReportTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.directory = Path(temporary.name)
        self.client = self.directory / "callgrind.out.2"
        self.client.write_text(profile("Client Request: " + C, 75))
        (self.directory / "callgrind.out.1").write_text(profile("Client Request: " + CPP, 82))
        self.termination = self.directory / "callgrind.out"
        self.termination.write_text(profile("Program termination", 0))

    def counts(self):
        return read_counts(self.directory, {C, CPP})

    def test_reads_named_workloads_independent_of_dump_order(self):
        self.assertEqual(self.counts(), {C: 75, CPP: 82})

    def test_requires_nonempty_unique_paired_catalog(self):
        listing = f"{C}\n{CPP}\n"
        self.assertEqual(workload_ids(listing), {C, CPP})
        for invalid in ("", C, listing + CPP, f"cpp/{CASE}\n"):
            with self.subTest(invalid=invalid), self.assertRaises(ValueError):
                workload_ids(invalid)

    def test_rejects_unexpected_duplicate_and_missing_workloads(self):
        for workload in (CPP, C.replace("le4", "le7")):
            self.client.write_text(profile("Client Request: " + workload, 75))
            with self.subTest(workload=workload), self.assertRaisesRegex(ValueError, "unexpected or duplicate"):
                self.counts()
        self.client.unlink()
        with self.assertRaisesRegex(ValueError, "missing workload"):
            self.counts()

    def test_rejects_wrong_tool_event_or_count(self):
        valid = self.client.read_text()
        for before, after in (("events: Ir", "events: Dr"), ("events: Ir", "events: Ir Dr"),
                              ("creator: callgrind", "creator: cachegrind"),
                              ("totals: 75", "totals: 0"), ("totals: 75", "totals: -1"),
                              ("totals: 75", "totals: 1e6"), ("totals: 75", ""),
                              ("totals: 75", "totals: 75\ntotals: 15")):
            self.client.write_text(valid.replace(before, after))
            with self.subTest(after=after), self.assertRaises(ValueError):
                self.counts()

    def test_checks_final_dump_and_collection_boundary(self):
        self.termination.write_text(profile("Program termination", 340))
        with self.assertRaisesRegex(ValueError, "unscoped instructions"):
            self.counts()
        self.termination.unlink()
        with self.assertRaisesRegex(ValueError, "missing program termination"):
            self.counts()

    def test_reports_drift_by_workload(self):
        first = self.counts()
        compare_runs(first, dict(first))
        with self.assertRaisesRegex(ValueError, f"{CPP}: 82 -> 83"):
            compare_runs(first, {C: 75, CPP: 83})
