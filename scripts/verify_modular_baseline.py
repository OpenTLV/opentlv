#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Marek Cingel
"""Run the final modular behavioral gate; preserve observations and failures."""

import argparse
from collections import Counter
from datetime import datetime, timezone
import json
from pathlib import Path
import platform
import re
import subprocess
import sys
from types import SimpleNamespace
import xml.etree.ElementTree as ET

import behavioral_baseline as baseline
import check_module_contracts as contracts
from check_module_isolation import check_metadata

ROOT = Path(__file__).resolve().parents[1]
REMOVED_OPTIONS = ("READER", "WRITER", "DOCUMENT", "QUERY", "CODEC", "SCHEMA",
                   "FORMAT_ASN1", "FORMAT_BER", "FORMAT_DER", "FORMAT_CER",
                   "EMV", "BLUETOOTH", "NFC", "DHCP", "LLDP")


def check_removed_options(source, build, output):
    results = []
    for option in REMOVED_OPTIONS:
        for value in ("ON", "OFF"):
            name = "OPENTLV_" + option
            label = option + "-" + value
            result = subprocess.run(["cmake", "-S", str(source), "-B", str(build / label),
                                     "-G", "Ninja", f"-D{name}={value}"],
                                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT, check=False)
            output.mkdir(parents=True, exist_ok=True)
            (output / (label + ".log")).write_bytes(result.stdout)
            baseline.checked(result.returncode != 0 and f"{name} was removed".encode() in result.stdout,
                             f"obsolete module selector was not rejected: {name}={value}")
            results.append(name + "=" + value)
    return results


def scenario_coverage(reference, current, migrations):
    """Every old scenario must still run, or name explicit replacement scenarios."""
    def identity(name):
        # CTest's old pretty names contained heap addresses for NFC vectors.
        # Keep their multiplicity; addresses are not scenario identities.
        return re.sub(r"\d+-byte object <[^>]+>", "<opaque-parameter>", name)

    names = Counter(identity(row["name"]) for row in current)
    old_names = Counter(identity(row["name"]) for row in reference)
    baseline.checked(not (migrations.keys() - old_names.keys()), "stale scenario migration")
    missing = []
    for name in sorted(old_names):
        replacement = migrations.get(name)
        if replacement is not None:
            baseline.checked(name not in names and replacement.get("reason")
                             and replacement.get("tests"), f"invalid scenario migration: {name}")
        expected = replacement["tests"] if replacement else [name]
        if any(names[test] < old_names[name] for test in expected):
            missing.append({"baseline": name, "expected_count": old_names[name],
                            "observed": {test: names[test] for test in expected}})
    baseline.checked(not missing, "baseline scenarios missing: " + json.dumps(missing))
    return {"baseline": sum(old_names.values()), "current_observations": sum(names.values()),
            "migrations": migrations}


def collect_gtest(directory):
    results = []
    for path in sorted(directory.glob("*.xml")):
        for case in ET.parse(path).findall(".//testcase"):
            baseline.checked(case.get("status") == "run" and case.get("result") == "completed"
                             and case.find("failure") is None and case.find("skipped") is None,
                             f"GoogleTest scenario did not pass: {case.get('name')}")
            name = case.attrib["name"]
            if "value_param" in case.attrib:
                name = name.rsplit("/", 1)[0] + "/" + case.attrib["value_param"]
            results.append({"name": case.attrib["classname"] + "." + name, "status": "passed"})
    baseline.checked(results, "GoogleTest scenario evidence missing")
    return results


def corpus_coverage(source, reference):
    # Tests may move, but every original binary seed must remain available.
    hashes = {baseline.digest(p.read_bytes()) for p in (source / "tests/fuzz").rglob("*")
              if p.is_file() and "corpus" in p.parts}
    missing = [name for name, entry in reference.items() if entry["sha256"] not in hashes]
    baseline.checked(not missing, "baseline fuzz seeds missing: " + ", ".join(missing))
    return {"baseline_seeds": len(reference), "missing": missing}


def verify(args):
    output, build = args.output.resolve(), args.build.resolve()
    baseline.checked(not output.exists(), "use a new evidence directory")
    output.mkdir(parents=True)
    reference = ROOT / "tests/baseline/v0.8.0"
    report = {"status": "failed", "created_at": datetime.now(timezone.utc).isoformat(),
              "reference_commit": baseline.COMMIT, "environment": platform.platform(),
              "candidate_commit": baseline.command(["git", "-C", args.repository,
                                                      "rev-parse", "HEAD"]).decode().strip(),
              "candidate_status": baseline.command(["git", "-C", args.repository,
                                                      "status", "--porcelain"]).decode(),
              "closure": "requires dated epic start gate and exact-candidate Full CI evidence"}
    try:
        inputs = baseline.candidate_inventory(ROOT)
        baseline.write(output / "candidate-files.json", inputs)
        report["candidate_files_sha256"] = baseline.digest(json.dumps(inputs, sort_keys=True).encode())
        baseline.verify(reference, args.repository)
        errors, graph = contracts.check(ROOT)
        baseline.checked(not errors, "\n".join(errors))
        report["dependency_graph"] = {key: sorted(value) for key, value in graph.items()}
        errors = []
        report["diagnostics"] = contracts.check_baseline_mapping(
            baseline.read(reference / "diagnostic-codes.json"),
            baseline.read(ROOT / contracts.MAPPING), errors)
        baseline.checked(not errors, "\n".join(errors))
        report["fuzz_corpus"] = corpus_coverage(ROOT, baseline.read(reference / "fuzz-corpus.json"))
        report["rejected_module_options"] = check_removed_options(
            ROOT, build / "obsolete-options", output / "obsolete-options")
        options = [*baseline.OPTIONS, "-DOPENTLV_BUILD_BENCHMARKS=OFF", *args.cmake_arg]
        report["configure_options"] = options
        baseline.command(["cmake", "-S", ROOT, "-B", build, *options],
                         log=output / "configure.log")
        entries = baseline.read(build / "modules-Release.json")
        contract = baseline.read(ROOT / contracts.CONTRACT)
        baseline.checked(not contract["exceptions"], "unresolved module boundary exceptions")
        modules = check_metadata(entries, contract, ROOT)
        expected = {key for key, entry in contract["targets"].items() if entry["role"] != "aggregate"}
        baseline.checked(expected == modules.keys(), "missing or unexpected module/integration")
        report["modules"] = entries
        baseline.command(["cmake", "--build", build, "--parallel", str(args.jobs)],
                         log=output / "build.log")
        baseline.command(["ctest", "--test-dir", build, "--output-on-failure", "--no-tests=error",
                          "--parallel", str(args.jobs), "--output-junit", output / "ctest.xml"],
                         log=output / "ctest.log", env={"GTEST_OUTPUT": "xml:" + str(output / "gtest") + "/"})
        tests = baseline.collect_ctest(output / "ctest.xml")
        report["ctest_suites"] = len(tests)
        scenarios = collect_gtest(output / "gtest")
        report["gtest_scenarios"] = len(scenarios)
        tests += scenarios
        baseline.write(output / "tests.json", tests)
        report["scenarios"] = scenario_coverage(baseline.read(reference / "test-results.json"), tests,
                                                baseline.read(ROOT / "tests/baseline/scenario-migrations.json"))
        baseline.replay(SimpleNamespace(source=ROOT, build=build, baseline=reference,
                                        repository=args.repository, output=output / "replay"))
        report["replay"] = baseline.read(output / "replay/replay.json")
        report["status"] = "passed"
    except (ValueError, OSError) as error:
        report["error"] = str(error)
        raise
    finally:
        baseline.write(output / "report.json", report)
    print(f"Modular baseline gate passed: {output / 'report.json'}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--repository", type=Path, default=ROOT)
    parser.add_argument("--jobs", type=int, default=2)
    parser.add_argument("--cmake-arg", action="append", default=[])
    args = parser.parse_args()
    try:
        verify(args)
    except (ValueError, OSError) as error:
        print(error, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
