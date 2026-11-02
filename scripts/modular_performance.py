#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Marek Cingel
"""Informational native comparison using identical release workloads on both sides."""

import argparse
from datetime import datetime, timezone
import json
from pathlib import Path
import platform
import re
import sys

import behavioral_baseline as baseline
from callgrind_compare import require_equivalent, validate_result
from core_performance import timing_comparison
from library_layout import historical_includes, public_roots

ROOT = Path(__file__).resolve().parents[1]
WORKLOADS = ("reader", "writer", "query", "der_schema_read_flat", "codec")


def forward_includes(text):
    mapping = baseline.read(ROOT / "docs/development/modules/header-paths.json")
    return re.sub(r'(#[ \t]*include[ \t]*[<"])([^>"\n]+)([>"])',
                  lambda match: match[1] + mapping.get(match[2], match[2]) + match[3], text)


def measure(args):
    output = args.output.resolve()
    baseline.checked(not output.exists(), "use a new performance evidence directory")
    output.mkdir(parents=True)
    source = output / "reference"
    files = baseline.prepare_source(args.repository, source)
    baseline.verify(ROOT / "tests/baseline/v0.8.0", args.repository)
    options = ["-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_C_COMPILER=gcc",
               "-DCMAKE_C_FLAGS=", "-DCMAKE_C_FLAGS_RELEASE=-O3 -DNDEBUG",
               "-DCMAKE_INTERPROCEDURAL_OPTIMIZATION=OFF", "-DOPENTLV_BUILD_SHARED_LIBS=OFF",
               "-DOPENTLV_BUILD_TESTS=OFF", "-DOPENTLV_BUILD_EXAMPLES=OFF", "-DOPENTLV_BUILD_CXX=OFF"]
    report = {"status": "failed", "informational": True,
              "created_at": datetime.now(timezone.utc).isoformat(), "platform": platform.platform(),
              "reference_commit": baseline.COMMIT, "configuration": options,
              "candidate_commit": baseline.command(["git", "-C", args.repository, "rev-parse", "HEAD"]).decode().strip(),
              "candidate_status": baseline.command(["git", "-C", args.repository, "status", "--porcelain"]).decode(),
              "compiler": baseline.command(["gcc", "--version"]).decode().splitlines()[0],
              "cpu": Path("/proc/cpuinfo").read_text().split("model name", 1)[-1].splitlines()[0],
              "iterations": args.iterations, "repetitions": 7, "results": {}}
    executables = {}
    try:
        inputs = baseline.candidate_inventory(ROOT)
        baseline.write(output / "candidate-files.json", inputs)
        report["candidate_files_sha256"] = baseline.digest(json.dumps(inputs, sort_keys=True).encode())
        report["harness_files"] = baseline.inventory({
            name: files["benchmarks/callgrind/" + name] for name in ("workloads.c", "schema_workloads.h")})
        report["harness_files"]["performance.c"] = baseline.inventory({
            "performance.c": (ROOT / "tests/baseline/performance.c").read_bytes()})["performance.c"]
        for side, tree in (("baseline", source), ("candidate", ROOT)):
            work, build = output / side, output / (side + "-build")
            work.mkdir()
            baseline.command(["cmake", "-S", tree, "-B", build, *options], log=work / "configure.log")
            baseline.command(["cmake", "--build", build, "--parallel", str(args.jobs)], log=work / "build.log")
            for name in ("workloads.c", "schema_workloads.h"):
                text = files["benchmarks/callgrind/" + name].decode()
                (work / name).write_text(forward_includes(text) if side == "candidate" else text)
            if side == "candidate":
                # The frozen harness uses removed selection macros. Both builds
                # include all functionality; this shim belongs only to the harness.
                (work / "tlv").mkdir()
                (work / "tlv/config.h").write_text("".join(
                    f"#define OPENTLV_{name} 1\n" for name in
                    ("SCHEMA", "READER", "FORMAT_BER", "FORMAT_DER", "CODEC", "WRITER")))
            (work / "performance.c").write_text(historical_includes(tree,
                (ROOT / "tests/baseline/performance.c").read_text()))
            executable = work / "performance"
            baseline.command(["gcc", "-std=c99", "-O3", "-DNDEBUG", "-D_POSIX_C_SOURCE=200809L",
                              "-DTLV_STATIC_DEFINE", *("-I" + str(p) for p in public_roots(tree)),
                              "-I" + str(build / "generated/include"), "-I" + str(work), work / "performance.c",
                              "-Wl,--start-group", *baseline.static_libraries(build), "-Wl,--end-group",
                              "-o", executable], log=work / "harness.log")
            executables[side] = executable
        rows = {name: {side: [] for side in executables} for name in WORKLOADS}
        for repetition in range(7):
            for name in WORKLOADS:
                for side in (("baseline", "candidate") if repetition % 2 == 0 else ("candidate", "baseline")):
                    observed = validate_result(baseline.command([executables[side], name, str(args.iterations)]),
                                               name, args.iterations)
                    rows[name][side].append(observed)
        for name, pair in rows.items():
            for row in pair["baseline"] + pair["candidate"]:
                require_equivalent(pair["baseline"][0], row)
            report["results"][name] = {"samples": pair, **timing_comparison(
                [r["elapsed_ns"] / args.iterations for r in pair["baseline"]],
                [r["elapsed_ns"] / args.iterations for r in pair["candidate"]])}
        report["status"] = "passed"
    except (ValueError, OSError) as error:
        report["error"] = str(error)
        raise
    finally:
        baseline.write(output / "performance.json", report)
    print(json.dumps({name: row["change_percent"] for name, row in report["results"].items()}, indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repository", type=Path, default=ROOT)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--iterations", type=int, default=5000)
    parser.add_argument("--jobs", type=int, default=2)
    arguments = parser.parse_args()
    try:
        measure(arguments)
    except (ValueError, OSError) as error:
        print(error, file=sys.stderr)
        sys.exit(1)
