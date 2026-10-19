#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Marek Cingel

"""Regression tests for complete, immutable release performance evidence."""

import copy
import io
import json
from pathlib import Path
import shutil
import tarfile
import tempfile
import unittest

import core_performance as performance


def candidate_record():
    return {"commit": "b" * 40, "tag": "v0.8.0", "repository": "OpenTLV/opentlv",
            "run_id": "123", "run_attempt": "2", "baseline": {
                "release_id": 7, "tag": "0.7.1", "version": "0.7.1", "commit": "a" * 40,
                "url": "https://github.com/OpenTLV/opentlv/releases/tag/0.7.1",
                "published_at": "2026-10-12T00:00:00Z"}}


def performance_fixture(directory, record, keep_raw=False):
    """Write synthetic three-asset evidence through the production report builder.

    This is a parser/assembly fixture, never hosted performance evidence.
    Callers may place unrelated release assets in the same directory.
    """
    directory = Path(directory)
    if not keep_raw:
        directory.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="opentlv-performance-fixture-") as temporary:
            report = performance_fixture(temporary, record, keep_raw=True)
            for name in performance.ASSETS:
                shutil.copyfile(Path(temporary) / name, directory / name)
        return report
    raw = directory / "raw"
    raw.mkdir(parents=True, exist_ok=True)
    expected = performance.identity(record["commit"], record["tag"], record["repository"],
                                    record["run_id"], record["run_attempt"], record["baseline"])
    performance.write_json(raw / "identity.json", expected)
    harness = raw / "harness"
    for name in performance.HARNESS_FILES:
        path = harness / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text("// Synthetic parser fixture: " + name + "\n", encoding="utf-8")
    (harness / "CMakeLists.txt").write_text(performance.harness_cmake(), encoding="utf-8")
    files = performance.file_manifest(harness)
    metadata = {"created_at": "2026-10-19T00:00:00+00:00", "baseline_mode": "remeasured-stable-release",
                "harness_commit": expected["baseline"]["commit"], "harness_files": files,
                "harness_sha256": performance.object_digest(files), "configuration": performance.CONFIGURATION,
                "benchmark_dependency_commit": performance.BENCHMARK_COMMIT,
                "toolchain": {key: "synthetic-test-fixture" for key in
                              ("cc", "cxx", "cc_target", "cxx_target", "cmake", "ninja", "valgrind", "libc", "python")},
                "runner": {"platform": "synthetic-fixture", "machine": "x86_64", "cpu_model": "fixture", "cpu_affinity": [0]}}
    performance.write_json(raw / "metadata.json", metadata)
    for side in performance.SIDES:
        source_files = {"benchmarks/" + name: files[name] for name in performance.HARNESS_FILES}
        performance.write_json(raw / "sources" / f"{side}.json", {
            "commit": expected["commit"] if side == "candidate" else expected["baseline"]["commit"],
            "git_tree": "d" * 40, "archive_sha256": "e" * 64, "files": source_files,
            "files_sha256": performance.object_digest(source_files)})
        build = raw / "build-metadata" / side
        build.mkdir(parents=True)
        cache = {"CMAKE_BUILD_TYPE": "Release", "CMAKE_C_FLAGS": "", "CMAKE_CXX_FLAGS": "",
                 "CMAKE_C_FLAGS_RELEASE": performance.CONFIGURATION["c_flags"],
                 "CMAKE_CXX_FLAGS_RELEASE": performance.CONFIGURATION["cxx_flags"],
                 "CMAKE_EXE_LINKER_FLAGS": "", "OPENTLV_BUILD_SHARED_LIBS": "OFF", "OPENTLV_BUILD_CXX": "ON"}
        (build / "CMakeCache.txt").write_text("".join(f"{key}:STRING={value}\n" for key, value in cache.items()), encoding="utf-8")
        (build / "config.h").write_text("/* synthetic all-capability config */\n", encoding="utf-8")
        performance.write_json(build / "compile_commands.json", [{"file": "fixture.c", "command": "cc -c fixture.c"}])
        performance.write_json(build / "binaries.json", {name: "f" * 64 for name in ("benchmark-tlv", "opentlv-perf-workload")})
        (raw / f"benchmark-inventory-{side}.txt").write_text("reader/16\nwriter/16\n", encoding="utf-8")
        for repetition in range(performance.CONFIGURATION["benchmark_repetitions"]):
            performance.write_json(raw / "benchmarks" / f"{side}-{repetition}.json", {
                "context": {"library_build_type": "release"}, "benchmarks": [
                    {"name": name, "run_type": "iteration", "iterations": 100,
                     "cpu_time": 100 + repetition + (50 if side == "candidate" else 0),
                     "real_time": 101 + repetition, "time_unit": "ns"} for name in ("reader/16", "writer/16")]})
        for workload in performance.WORKLOADS:
            stem = raw / "callgrind" / f"{side}-{workload}"
            performance.write_json(stem.with_suffix(".json"), {
                "workload": workload, "iterations": performance.CONFIGURATION["callgrind_iterations"],
                "elapsed_ns": 100000, "checksum": 42, "input_bytes": 16, "input_id": "synthetic-v1"})
            count = 200 if side == "candidate" else 100
            stem.with_suffix(".out.1").write_text(
                f"version: 1\nevents: Ir\nsummary: {count}\nfn=fixture\n1 {count}\ntotals: {count}\n", encoding="utf-8")
    return performance.package_report(directory, expected, metadata)


class PerformanceTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.directory = Path(self.temporary.name)
        self.record = candidate_record()

    def validate(self, **changes):
        record = {**self.record, **changes}
        return performance.validate_report(self.directory, record["commit"], record["tag"], record["repository"],
                                           record["run_id"], record["run_attempt"], record["baseline"])

    def fixture(self):
        return performance_fixture(self.directory, self.record, keep_raw=True)

    def repackage(self):
        raw = self.directory / "raw"
        return performance.package_report(self.directory, performance.load_json(raw / "identity.json"),
                                          performance.load_json(raw / "metadata.json"))

    def test_complete_assets_validate_and_regressions_remain_advisory(self):
        report = self.fixture()
        (self.directory / "unrelated-native-package.zip").write_bytes(b"package")
        self.assertEqual(self.validate(), report)
        self.assertTrue(report["advisory"])
        self.assertEqual(len(report["comparison"]["instructions"]), 16)
        self.assertEqual(report["comparison"]["timings"][0]["classification"], "regression")
        self.assertEqual(report["comparison"]["instructions"][0]["classification"], "regression")

    def test_distribution_overlap_is_noise_and_direction_is_measured(self):
        compare = performance.timing_comparison
        self.assertEqual(compare([10] * 7, [9] * 7)["classification"], "improvement")
        self.assertEqual(compare([10] * 7, [11] * 7)["classification"], "regression")
        self.assertEqual(compare([1, 2, 3, 4, 5, 6, 1000], [8] * 7)["classification"], "noise")
        self.assertEqual(compare([10] * 7, [10] * 7)["classification"], "noise")
        for values in ([1] * 6, [1] * 6 + [0], [1] * 6 + [float("nan")], [1] * 6 + [True]):
            with self.subTest(values=values), self.assertRaises(ValueError):
                compare(values, [1] * 7)

    def test_identity_and_baseline_lock_are_exact(self):
        self.fixture()
        for changes in ({"commit": "c" * 40}, {"tag": "v0.8.1"}, {"run_id": "124"},
                        {"run_attempt": "3"}, {"repository": "Other/opentlv"},
                        {"baseline": {**self.record["baseline"], "release_id": 8}}):
            with self.subTest(changes=changes), self.assertRaises(ValueError):
                self.validate(**changes)

    def test_report_and_markdown_tampering_is_rejected(self):
        report = self.fixture()
        changed = copy.deepcopy(report)
        changed["comparison"]["timings"][0]["classification"] = "noise"
        performance.write_json(self.directory / "performance.json", changed)
        with self.assertRaisesRegex(ValueError, "comparisons"):
            self.validate()
        performance.write_json(self.directory / "performance.json", report)
        (self.directory / "performance.md").write_text("All faster", encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "Markdown"):
            self.validate()

    def test_archive_bytes_and_missing_export_are_rejected(self):
        self.fixture()
        archive = self.directory / "performance-data.tar.gz"
        archive.write_bytes(archive.read_bytes() + b"changed")
        with self.assertRaisesRegex(ValueError, "archive hash"):
            self.validate()
        archive.unlink()
        with self.assertRaises(OSError):
            self.validate()

    def test_missing_sample_or_profile_fails(self):
        self.fixture()
        sample = self.directory / "raw" / "benchmarks" / "baseline-6.json"
        original = sample.read_bytes()
        sample.unlink()
        with self.assertRaisesRegex(ValueError, "sample files"):
            self.repackage()
        sample.write_bytes(original)
        (self.directory / "raw" / "callgrind" / "candidate-reader.out.1").unlink()
        with self.assertRaises(OSError):
            self.repackage()

    def test_invalid_benchmark_measurements_fail_closed(self):
        self.fixture()
        path = self.directory / "raw" / "benchmarks" / "baseline-0.json"
        original = performance.load_json(path)
        changes = ({"cpu_time": 0}, {"cpu_time": True}, {"real_time": -1}, {"iterations": 0},
                   {"error_occurred": True}, {"error_message": "skipped"}, {"run_type": "aggregate"},
                   {"time_unit": "minutes"}, {"name": "unexpected"})
        for change in changes:
            value = copy.deepcopy(original)
            value["benchmarks"][0].update(change)
            performance.write_json(path, value)
            with self.subTest(change=change), self.assertRaises(ValueError):
                self.repackage()
        for text in ('{"context":{},"benchmarks":NaN}', '{"context":{},"context":{}}', '{"extra":1e999}'):
            path.write_text(text, encoding="utf-8")
            with self.subTest(text=text), self.assertRaises(ValueError):
                self.repackage()

    def test_incomplete_duplicate_and_different_inventories_fail(self):
        self.fixture()
        path = self.directory / "raw" / "benchmarks" / "baseline-0.json"
        original = performance.load_json(path)
        for entries in ([], original["benchmarks"][:1], original["benchmarks"] + original["benchmarks"][:1]):
            performance.write_json(path, {**original, "benchmarks": entries})
            with self.subTest(entries=len(entries)), self.assertRaises(ValueError):
                self.repackage()
        performance.write_json(path, original)
        for listing in ("", "reader/16\nreader/16\n", "reader/16\n"):
            (self.directory / "raw" / "benchmark-inventory-candidate.txt").write_text(listing, encoding="utf-8")
            with self.subTest(listing=listing), self.assertRaises(ValueError):
                self.repackage()

    def test_callgrind_inputs_and_complete_profiles_are_required(self):
        self.fixture()
        path = self.directory / "raw" / "callgrind" / "candidate-reader.json"
        original = performance.load_json(path)
        for change in ({"checksum": 43}, {"input_id": "different"}, {"input_bytes": 17}, {"iterations": 1}):
            performance.write_json(path, {**original, **change})
            with self.subTest(change=change), self.assertRaises(ValueError):
                self.repackage()
        path.write_text(json.dumps({**original, "extra": float("nan")}), encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "Nonfinite"):
            self.repackage()
        performance.write_json(path, original)
        profile = path.with_suffix(".out.1")
        for text in ("events: Ir\nfn=test\n1 10\n", "events: Ir\nfn=test\n1 0\ntotals: 0\n"):
            profile.write_text(text, encoding="utf-8")
            with self.subTest(text=text), self.assertRaises(ValueError):
                self.repackage()

    def test_harness_and_capabilities_cannot_change(self):
        self.fixture()
        path = self.directory / "raw" / "harness" / performance.HARNESS_FILES[0]
        path.write_text("changed source", encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "source hashes"):
            self.repackage()
        # Even updating local harness hashes must match the stable source manifest.
        metadata_path = self.directory / "raw" / "metadata.json"
        metadata = performance.load_json(metadata_path)
        metadata["harness_files"] = performance.file_manifest(path.parent.parent)
        metadata["harness_sha256"] = performance.object_digest(metadata["harness_files"])
        performance.write_json(metadata_path, metadata)
        with self.assertRaisesRegex(ValueError, "stable release source"):
            self.repackage()

    def test_build_flags_and_generated_capabilities_are_checked(self):
        self.fixture()
        build = self.directory / "raw" / "build-metadata" / "candidate"
        cache = build / "CMakeCache.txt"
        original = cache.read_text(encoding="utf-8")
        cache.write_text(original.replace("-O3", "-O0"), encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "build flags"):
            self.repackage()
        cache.write_text(original, encoding="utf-8")
        (build / "config.h").write_text("minimal", encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "capabilities"):
            self.repackage()

    def test_archive_rejects_traversal_links_duplicates_and_extras(self):
        archive = self.directory / "bad.tar"
        cases = (("../escape", tarfile.REGTYPE), ("/absolute", tarfile.REGTYPE),
                 ("raw\\escape", tarfile.REGTYPE), ("C:/escape", tarfile.REGTYPE),
                 ("raw/link", tarfile.SYMTYPE), ("raw/link", tarfile.LNKTYPE))
        for name, kind in cases:
            with tarfile.open(archive, "w") as target:
                entry = tarfile.TarInfo(name)
                entry.type = kind
                entry.linkname = "outside"
                target.addfile(entry, io.BytesIO(b""))
            with self.subTest(name=name, kind=kind), self.assertRaises(ValueError):
                performance.safe_extract(archive, self.directory / "out", {"raw/file": "hash"})
        with tarfile.open(archive, "w") as target:
            for _ in range(2):
                target.addfile(tarfile.TarInfo("raw/file"), io.BytesIO(b""))
        with self.assertRaisesRegex(ValueError, "Duplicate"):
            performance.safe_extract(archive, self.directory / "out", {"raw/file": "hash"})
        with self.assertRaisesRegex(ValueError, "Unexpected"):
            performance.safe_extract(archive, self.directory / "out", {"raw/other": "hash"})

    def test_configuration_is_shared_and_dependency_is_immutable(self):
        command = performance.build_command(Path("harness"), Path("source"), Path("build"), "cc", "c++")
        self.assertIn("-DCMAKE_C_FLAGS_RELEASE=-O3 -g -DNDEBUG", command)
        self.assertIn("-DCMAKE_CXX_FLAGS_RELEASE=-O3 -g -DNDEBUG", command)
        self.assertIn("-DCMAKE_C_STANDARD=99", command)
        self.assertIn("-DCMAKE_CXX_STANDARD=11", command)
        self.assertRegex(performance.BENCHMARK_COMMIT, "^[0-9a-f]{40}$")
        self.assertIn(performance.BENCHMARK_COMMIT, performance.harness_cmake())


if __name__ == "__main__":
    unittest.main()
