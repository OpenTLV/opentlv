"""Regression checks for aggregate rows in Google Benchmark JSON reports."""

import json
import math
import tempfile
import unittest
from pathlib import Path

from check_results import check_report


WORKLOAD = "c99/v1/field-write/le4/offset0/complete"
RUN_NAME = WORKLOAD + "/real_time"


def report_rows():
    common = {"run_name": RUN_NAME, "repetitions": 3, "time_unit": "ns", "cpu_time": 0.0,
              "family_index": 0, "per_family_instance_index": 0, "threads": 1}
    rows = []
    for index, ns in enumerate((20, 20, 80)):
        rate = 1e9 / ns
        rows.append(dict(common, name=RUN_NAME, run_type="iteration", repetition_index=index,
                         iterations=32, real_time=ns, items_per_second=rate,
                         bytes_per_operation=4, bytes_per_second=4 * rate))

    # The three rates are 50, 50 and 12.5 million calls/s. Relative variation
    # is identical for the call and byte rates. Zero CPU times give a zero CV.
    root3 = math.sqrt(3)
    for stat, ns, rate, byte_count in (
        ("mean", 40, 37.5e6, 4),
        ("median", 20, 50e6, 4),
        ("stddev", 20 * root3, 12.5e6 * root3, 0),
        ("cv", root3 / 2, root3 / 3, 0),
    ):
        rows.append(dict(common, name=RUN_NAME + "_" + stat, run_type="aggregate",
                         aggregate_name=stat, aggregate_unit="percentage" if stat == "cv" else "time",
                         iterations=3, real_time=ns, items_per_second=rate, bytes_per_operation=byte_count,
                         bytes_per_second=rate if stat == "cv" else 4 * rate))
    return rows


class AggregateReportTests(unittest.TestCase):
    def check_rows(self, rows):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "report.json"
            path.write_text(json.dumps({"benchmarks": rows}), encoding="utf-8")
            check_report(path, {WORKLOAD})

    def test_repeated_measurements_with_all_four_statistics(self):
        self.check_rows(report_rows())

    def test_single_repetition_has_no_aggregates(self):
        rows = report_rows()
        only = dict(rows[0], repetitions=1)
        self.check_rows([only])
        with self.assertRaises(ValueError):
            self.check_rows([only, dict(rows[3], iterations=1, repetitions=1)])

    def test_accepts_variance_rounded_to_zero_for_nearly_constant_times(self):
        rows = report_rows()
        for row, value in zip(rows, (100 - 1e-8, 100, 100 + 1e-8, 100, 100, 0, 0)):
            row["cpu_time"] = value
        self.check_rows(rows)

    def test_rejects_unknown_missing_and_duplicate_aggregates(self):
        rows = report_rows()
        unknown = dict(rows[-1], name=RUN_NAME + "_p95", aggregate_name="p95", aggregate_unit="time")
        for problem, changed in (("unknown", rows + [unknown]), ("missing", rows[:-1]),
                                 ("duplicate", rows + [rows[-1]])):
            with self.subTest(problem=problem), self.assertRaises(ValueError):
                self.check_rows(changed)

    def test_rejects_inconsistent_aggregate_metadata(self):
        for stat, key, value in (("cv", "aggregate_unit", "time"), ("stddev", "aggregate_unit", "percentage"),
                                 ("median", "name", RUN_NAME + "_mean"), ("mean", "iterations", 32)):
            with self.subTest(stat=stat, key=key), self.assertRaises(ValueError):
                rows = report_rows()
                row = next(row for row in rows if row.get("aggregate_name") == stat)
                row[key] = value
                self.check_rows(rows)

    def test_checks_statistics_against_individual_repetitions(self):
        for stat, key, value in (("mean", "cpu_time", 1), ("median", "real_time", 40),
                                 ("mean", "bytes_per_second", 1e8), ("stddev", "bytes_per_operation", 4),
                                 ("stddev", "items_per_second", 0), ("cv", "real_time", 50 * math.sqrt(3))):
            with self.subTest(stat=stat, key=key), self.assertRaises(ValueError):
                rows = report_rows()
                row = next(row for row in rows if row.get("aggregate_name") == stat)
                row[key] = value
                self.check_rows(rows)
