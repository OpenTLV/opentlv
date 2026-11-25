"""Validate full Google Benchmark JSON reports against the checked fixture catalog."""

import argparse
import json
import math
import subprocess
from collections import defaultdict
from pathlib import Path


def bytes_per_operation(workload):
    _, _, operation, layout, *dimensions = workload.split("/")
    if "complete" not in dimensions or operation in ("header-size", "element-size"):
        return 0
    width = int(layout[2:])
    if operation in ("field-read", "field-write"):
        return width
    payload = next(part for part in dimensions if part.startswith("payload"))
    return 2 + width + int(payload[len("payload"):])  # two-byte tag, length and value


def check_measurement(row, workload):
    if row.get("time_unit") != "ns":
        raise ValueError("expected nanoseconds")
    if type(row.get("iterations")) is not int or row["iterations"] < 1:
        raise ValueError("expected a positive iteration count")
    for key in ("real_time", "cpu_time", "bytes_per_operation", "bytes_per_second", "items_per_second"):
        value = row.get(key)
        if type(value) not in (float, int) or not math.isfinite(value) or value < 0:
            raise ValueError(f"expected a finite nonnegative {key}, got {value!r}")
    # Aggregate counters are statistics, including deviations and coefficients of variation.
    if row.get("run_type") == "aggregate":
        return
    if row.get("run_type") != "iteration":
        raise ValueError("unknown run_type")
    size = bytes_per_operation(workload)
    if row["bytes_per_operation"] != size:
        raise ValueError(f"expected {size} bytes per operation")
    if row["real_time"] <= 0 or not math.isclose(row["items_per_second"], 1e9 / row["real_time"]):
        raise ValueError("operation rate disagrees with wall time per operation")
    if not math.isclose(row["bytes_per_second"], size * row["items_per_second"]):
        raise ValueError("byte rate disagrees with bytes per operation")


def check_report(path, catalog):
    with path.open(encoding="utf-8") as stream:
        report = json.load(stream)
    if not isinstance(report, dict) or not isinstance(report.get("benchmarks"), list):
        raise ValueError("missing benchmarks array")
    seen = defaultdict(set)
    repetitions = None
    for row in report["benchmarks"]:
        if not isinstance(row, dict):
            raise ValueError("expected a benchmark row")
        name = row.get("run_name")
        if not isinstance(name, str) or not name.endswith("/real_time"):
            raise ValueError(f"unexpected run name {name!r}")
        workload = name.rsplit("/", 1)[0]
        if workload not in catalog:
            raise ValueError(f"unknown workload {workload}")
        if row.get("error_occurred") or row.get("skipped"):
            raise ValueError(f"{name}: {row.get('error_message') or row.get('skip_message') or 'skipped'}")
        try:
            check_measurement(row, workload)
        except ValueError as error:
            raise ValueError(f"{name}: {error}") from error
        count = row.get("repetitions")
        if type(count) is not int or count < 1 or (repetitions is not None and count != repetitions):
            raise ValueError(f"{name}: invalid or inconsistent repetition count {count!r}")
        repetitions = count
        if row["run_type"] == "iteration":
            index = row.get("repetition_index")
            if type(index) is not int or not 0 <= index < count or index in seen[workload]:
                raise ValueError(f"{name}: invalid or duplicate repetition index {index!r}")
            seen[workload].add(index)

    for workload in sorted(catalog):
        if not seen[workload] or len(seen[workload]) != repetitions:
            raise ValueError(f"{workload}: missing iteration data")
    print(f"{path}: validated {len(catalog)} workload IDs, {repetitions} repetition(s) each")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fixtures", type=Path, required=True, help="path to opentlv_check_fixtures")
    parser.add_argument("reports", type=Path, nargs="+", help="full JSON reports with individual repetitions")
    args = parser.parse_args()
    try:
        ids = subprocess.check_output([args.fixtures.resolve(), "--list"], text=True).splitlines()
    except (OSError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"{args.fixtures}: {error}\n")
    if not ids:
        parser.exit(1, "Fixture checker returned an empty catalog\n")
    catalog = set(ids)
    for path in args.reports:
        try:
            check_report(path, catalog)
        except (OSError, ValueError) as error:
            parser.exit(1, f"{path}: {error}\n")
