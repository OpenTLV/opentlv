# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Marek Cingel
"""Exercise the performance comparison CLI with complete synthetic probe reports."""

import itertools
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from check_results import bytes_per_operation
from compare_performance import FORMAT_ONLY_PROTOCOLS, FORMATTED_REFERENCE
from performance_build import TARGETS
from run_callgrind import OPTIONS, VERSION


CLI = Path(__file__).with_name("compare_performance.py")
BASELINE = FORMATTED_REFERENCE
CANDIDATE = "b" * 40


def workload_cases():
    for order, width in itertools.product(("be", "le"), range(1, 9)):
        layout = f"{order}{width}"
        for outcome in ("complete", "invalid"):
            yield f"v1/header-size/{layout}/{outcome}"
        for payload in (0, 3, 255 if width == 1 else 513):
            for outcome in ("complete", "invalid"):
                yield f"v1/element-size/{layout}/payload{payload}/{outcome}"
            for op, offset, outcome, mode in itertools.product(
                ("read", "next", "write", "append"), (0, 1), ("complete", "short"), ("plain", "diagnostic")
            ):
                yield f"v1/{op}/{layout}/offset{offset}/payload{payload}/{outcome}/{mode}"
        for op, offset, outcome in itertools.product(("field-read", "field-write"), (0, 1), ("complete", "short")):
            yield f"v1/{op}/{layout}/offset{offset}/{outcome}"


def paired_ids(cases):
    return [f"{language}/{case}" for case in cases for language in ("c99", "cpp17")]


def counted_cases():
    for result in ("complete", "invalid"):
        yield f"v1/header-size/be2/{result}"
        yield f"v1/element-size/le8/payload513/{result}"
    for operation, layout, result in itertools.product(
        ("field-read", "field-write"), ("be3/offset0", "le8/offset1"), ("complete", "short")
    ):
        yield f"v1/{operation}/{layout}/{result}"
    io_inputs = ("read/be1/offset0/payload0", "next/le3/offset1/payload3",
                 "write/le8/offset1/payload513", "append/be2/offset0/payload3")
    for path, result, mode in itertools.product(io_inputs, ("complete", "short"), ("plain", "diagnostic")):
        yield f"v1/{path}/{result}/{mode}"


TIMING_IDS = paired_ids(workload_cases())
COUNT_IDS = paired_ids(counted_cases())
ROLES = ("core", "io", "c99-write", "cpp17-write")
COUNT_REPORTS = Path("callgrind/callgrind-reports")
COUNT_METADATA = Path("callgrind/callgrind-build/benchmarks/RelWithDebInfo/metadata.json")


def build_metadata(revision, configuration, linkage, timing=False):
    compiler_flags = "-O2 -DNDEBUG -march=x86-64 -mtune=generic"
    if configuration == "RelWithDebInfo":
        compiler_flags = "-O2 -g -DNDEBUG -march=x86-64 -mtune=generic"
    tools = {"cmake": "3.28.3", "git": "2.43.0"}
    if timing:
        tools["google_benchmark"] = "1.9.4"
    metadata = {
        "schema_version": 1, "revision": revision, "dirty": False, "library_linkage": linkage,
        "configuration": configuration, "generator": "Ninja", "system": "Linux",
        "processor": "x86_64", "pointer_bits": 64, "generator_platform": "", "osx_architectures": "",
        "interprocedural_optimization": False, "test_coverage": False, "tools": tools,
        "compilers": {lang: {"id": "GNU", "version": "13.3.0", "path": f"/usr/bin/{binary}",
                             "target": "", "architecture": "", "flags": "", "configuration_flags": compiler_flags}
                      for lang, binary in (("C", "gcc-13"), ("CXX", "g++-13"))},
    }
    for prefix in ("", "configuration_"):
        for kind in ("", "static_", "shared_"):
            metadata[f"{prefix}{kind}linker_flags"] = ""
    return metadata


def write_json(path, report):
    path.write_text(json.dumps(report), encoding="utf-8")


def read_json(path):
    return json.loads(path.read_text(encoding="utf-8"))


def write_evidence(directory, revision, ns):
    callgrind = directory / COUNT_REPORTS
    timing = directory / "timing"
    footprint = directory / "footprint"
    for path in (callgrind, (directory / COUNT_METADATA).parent, timing, footprint):
        path.mkdir(parents=True)
    (directory / "protocol.sha256").write_text("f" * 64 + "\n", encoding="utf-8")
    compilation = {"language": "C", "flags": "-O2", "defines": ["NDEBUG"],
                   "includes": [{"path": "<source>/include", "system": False}]}
    signature = {"compile": [compilation], "link": []}
    write_json(directory / "build-settings.json",
               {mode: dict.fromkeys(targets, signature) for mode, targets in TARGETS.items()})
    write_json(directory / COUNT_METADATA, build_metadata(revision, "RelWithDebInfo", "static"))
    write_json(callgrind / "runtime.json", {
        "valgrind": f"valgrind-{VERSION}", "options": list(OPTIONS), "LD_BIND_NOW": "1",
        "probe_sha256": revision[0] * 64, "host": {
            "machine": "x86_64", "kernel": "6.8.0", "libc": ["glibc", "2.39"],
            "cpu": {"model name": "Test CPU", "flags": "sse2"}, "os_release": ["ID=ubuntu"],
            "dpkg_versions": {"gcc-13": "13.3.0", "g++-13": "13.3.0", "libc6": "2.39",
                              "libstdc++6": "14.2.0", "valgrind": "1:3.22.0-0ubuntu3"},
        },
    })
    (callgrind / "workloads.txt").write_text("\n".join(COUNT_IDS) + "\n", encoding="utf-8")
    counts = "workload_id\tIr\n" + "".join(f"{name}\t120\n" for name in COUNT_IDS)
    for run in ("run-1", "run-2"):
        (callgrind / run).mkdir()
        (callgrind / run / "counts.tsv").write_text(counts, encoding="utf-8")
    write_json(timing / "metadata.json", build_metadata(revision, "Release", "static", timing=True))
    (timing / "workloads.txt").write_text("\n".join(TIMING_IDS) + "\n", encoding="utf-8")
    rows = []
    for name in TIMING_IDS:
        size = bytes_per_operation(name)
        rows.append({
            "name": name + "/real_time", "run_name": name + "/real_time", "run_type": "iteration",
            "time_unit": "ns", "iterations": 32, "repetitions": 1, "repetition_index": 0, "threads": 1,
            "real_time": ns, "cpu_time": ns, "bytes_per_operation": size,
            "items_per_second": 1e9 / ns, "bytes_per_second": size * 1e9 / ns,
        })
    context = {"json_schema_version": 1, "library_version": "v1.9.4", "library_build_type": "release",
               "host_name": "test-host", "num_cpus": 2}
    write_json(timing / "results.json", {"context": context, "benchmarks": rows})
    for linkage in ("static", "shared"):
        write_json(footprint / f"{linkage}.json", {
            "build": build_metadata(revision, "Release", linkage),
            "section_tool": {"path": "/usr/bin/size", "method": "size --format=sysv --radix=10",
                             "version": "GNU size 2.42"},
            "artifacts": {role: {"file": role, "file_bytes": 4096, "section_bytes": {".text": 512, ".bss": 0}}
                          for role in ROLES},
        })


class PerformanceComparisonTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        root = Path(temporary.name)
        self.baseline = root / "baseline"
        self.candidate = root / "candidate"
        self.output = root / "comparison"
        write_evidence(self.baseline, BASELINE, ns=20)
        write_evidence(self.candidate, CANDIDATE, ns=2000)

    def compare(self, status, baseline_revision=BASELINE):
        result = subprocess.run(
            [sys.executable, "-B", CLI, "--baseline", self.baseline, "--candidate", self.candidate,
             "--baseline-revision", baseline_revision, "--candidate-revision", CANDIDATE, "--output", self.output],
            capture_output=True, text=True, check=False,
        )
        self.assertEqual(result.returncode == 0, status == "pass", result.stdout + result.stderr)
        report = read_json(self.output / "comparison.json")
        self.assertEqual(report["status"], status, report.get("error"))
        self.assertEqual(report["schema_version"], 1)
        self.assertEqual(report["baseline_revision"], baseline_revision)
        self.assertEqual(report["candidate_revision"], CANDIDATE)
        summary = (self.output / "summary.md").read_text(encoding="utf-8")
        self.assertIn(status, summary.lower())
        return report

    def change_counts(self, new_count, repeats=("run-1", "run-2")):
        for run in repeats:
            path = self.candidate / COUNT_REPORTS / run / "counts.tsv"
            counts = path.read_text(encoding="utf-8")
            path.write_text(counts.replace(f"{COUNT_IDS[0]}\t120\n", f"{COUNT_IDS[0]}\t{new_count}\n"),
                            encoding="utf-8")

    def set_protocols(self, baseline, candidate):
        (self.baseline / "protocol.sha256").write_text(baseline, encoding="ascii")
        (self.candidate / "protocol.sha256").write_text(candidate, encoding="ascii")

    def test_format_only_protocol_transition_preserves_raw_hashes(self):
        self.set_protocols(*FORMAT_ONLY_PROTOCOLS)
        report = self.compare("pass")
        self.assertEqual(report["protocol_sha256"], dict(zip(("baseline", "candidate"), FORMAT_ONLY_PROTOCOLS)))
        self.assertEqual(report["configuration"]["protocol_sha256"], FORMAT_ONLY_PROTOCOLS[0])

    def test_format_only_protocol_transition_requires_the_reviewed_revision(self):
        other_revision = "a" * 40
        self.baseline = self.baseline.with_name("other-reference")
        write_evidence(self.baseline, other_revision, ns=20)
        self.set_protocols(*FORMAT_ONLY_PROTOCOLS)
        self.assertIn("protocol", self.compare("invalid", baseline_revision=other_revision)["error"])

    def test_faster_counts_and_slower_timings_pass(self):
        self.change_counts(119)
        report = self.compare("pass")
        self.assertEqual(report["regressions"], [])
        self.assertIn({"baseline": 120, "candidate": 119, "delta": -1}, report["metrics"].values())
        observations = report["timing_median_ns"]
        self.assertEqual(observations.keys(), set(TIMING_IDS))
        self.assertEqual(observations[TIMING_IDS[0]], {"baseline": 20, "candidate": 2000, "delta": 1980})

    def test_one_instruction_or_one_byte_increase_is_reported(self):
        self.set_protocols(*FORMAT_ONLY_PROTOCOLS)
        self.change_counts(121)
        for linkage, role, measurement in (("static", "c99-write", "file_bytes"), ("shared", "io", ".text")):
            path = self.candidate / "footprint" / f"{linkage}.json"
            footprint = read_json(path)
            artifact = footprint["artifacts"][role]
            if measurement == "file_bytes":
                artifact[measurement] += 1
            else:
                artifact["section_bytes"][measurement] += 1
            write_json(path, footprint)
        report = self.compare("regression")
        self.assertEqual(len(report["regressions"]), 3)
        for metric in report["regressions"]:
            self.assertEqual(report["metrics"][metric]["delta"], 1)
            self.assertIn(metric, (self.output / "summary.md").read_text(encoding="utf-8"))

    def test_missing_evidence_replaces_a_passing_verdict(self):
        self.compare("pass")
        (self.candidate / "footprint" / "shared.json").unlink()
        self.assertIn("shared.json", self.compare("invalid")["error"])

    def test_different_compiler_is_incomparable(self):
        self.set_protocols(*FORMAT_ONLY_PROTOCOLS)
        path = self.candidate / COUNT_METADATA
        metadata = read_json(path)
        metadata["compilers"]["CXX"]["version"] = "14.0.0"
        write_json(path, metadata)
        self.assertIn("CXX.version", self.compare("invalid")["error"])

    def test_changed_target_flags_with_identical_global_flags_is_incomparable(self):
        path = self.candidate / "build-settings.json"
        settings = read_json(path)
        settings["static"]["opentlv_io"]["compile"][0]["flags"] += " -fno-inline"
        write_json(path, settings)
        self.assertIn("opentlv_io", self.compare("invalid")["error"])

    def test_changed_workload_protocol_is_incomparable(self):
        self.set_protocols(FORMAT_ONLY_PROTOCOLS[0], "c" * 64)
        self.assertIn("protocol", self.compare("invalid")["error"])
        self.set_protocols(*reversed(FORMAT_ONLY_PROTOCOLS))
        self.assertIn("protocol", self.compare("invalid")["error"])

    def test_substituted_count_workloads_are_incomparable(self):
        callgrind = self.candidate / COUNT_REPORTS
        for path in (callgrind / "workloads.txt", callgrind / "run-1/counts.tsv", callgrind / "run-2/counts.tsv"):
            contents = path.read_text(encoding="utf-8")
            contents = contents.replace("header-size/be2/complete", "header-size/be4/complete")
            path.write_text(contents, encoding="utf-8")
        self.assertIn("header-size/be4", self.compare("invalid")["error"])

    def test_truncated_timing_catalog_on_both_sides_fails(self):
        for side in (self.baseline, self.candidate):
            path = side / "timing" / "workloads.txt"
            path.write_text("\n".join(TIMING_IDS[:-2]) + "\n", encoding="utf-8")
            path = side / "timing" / "results.json"
            report = read_json(path)
            report["benchmarks"] = report["benchmarks"][:-2]
            write_json(path, report)
        self.assertIn("3584", self.compare("invalid")["error"])

    def test_disagreement_between_count_repeats_fails(self):
        self.change_counts(119, repeats=("run-2",))
        self.assertIn(COUNT_IDS[0], self.compare("invalid")["error"])

    def test_malformed_timing_sample_fails(self):
        path = self.candidate / "timing" / "results.json"
        results = read_json(path)
        results["benchmarks"][0]["real_time"] = -1
        write_json(path, results)
        self.assertIn("real_time", self.compare("invalid")["error"])

    def test_different_timing_thread_count_is_incomparable(self):
        path = self.candidate / "timing" / "results.json"
        results = read_json(path)
        results["benchmarks"][0]["threads"] = 2
        write_json(path, results)
        self.assertIn("threads", self.compare("invalid")["error"])

    def test_wrong_revision_fails(self):
        path = self.baseline / "footprint" / "static.json"
        footprint = read_json(path)
        footprint["build"]["revision"] = CANDIDATE
        write_json(path, footprint)
        self.assertIn("revision", self.compare("invalid")["error"])
