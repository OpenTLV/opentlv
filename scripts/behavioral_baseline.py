#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Marek Cingel
"""Capture/replay the Linux x64 v0.8.0 behavioral reference without changing Core."""

import argparse
import hashlib
import io
import json
import os
import platform
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
TAG = "v0.8.0"
COMMIT = "345c77acb29266036df5f27d04ce315bb61853cc"
RUN = "40500582050"
DEFAULT = ROOT / "tests/baseline/v0.8.0"
OPTIONS = ["-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_C_STANDARD=99",
           "-DCMAKE_CXX_STANDARD=23", "-DOPENTLV_BUILD_SHARED_LIBS=OFF",
           "-DOPENTLV_BUILD_PROPERTY_TESTS=ON", "-DOPENTLV_PROPERTY_COUNT=32",
           f"-DOPENTLV_PROPERTY_COMMIT={COMMIT}", "-DOPENTLV_BUILD_BENCHMARKS=ON",
           "-DOPENTLV_BUILD_EXAMPLES=OFF"]


def digest(data):
    return hashlib.sha256(data).hexdigest()


def read(path):
    return json.loads(path.read_text(encoding="utf-8"))


def write(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def checked(condition, message):
    if not condition:
        raise ValueError(message)


def command(args, *, cwd=None, log=None):
    result = subprocess.run([str(arg) for arg in args], cwd=cwd, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, check=False,
                            env=dict(os.environ, PYTHONDONTWRITEBYTECODE="1"))
    if log:
        log.parent.mkdir(parents=True, exist_ok=True)
        log.write_bytes(result.stdout)
    if result.returncode:
        raise ValueError(f"command failed ({result.returncode}): {args}\n"
                         + result.stdout.decode(errors="replace")[-4000:])
    return result.stdout


def check_provenance(provenance):
    release = read(provenance / "release.json")
    run = read(provenance / "ci-run.json")
    manifest = read(provenance / "release-manifest.json")
    checked(release["tag_name"] == TAG and release["published_at"]
            and not release["draft"] and not release["prerelease"], "reference release is not published")
    checked(str(run["id"]) == RUN and run["head_sha"] == COMMIT
            and run["conclusion"] == "success" and run["status"] == "completed",
            "reference release CI did not succeed at the pinned commit")
    quality = manifest["quality"]
    checked(manifest["commit"] == COMMIT and manifest["candidate"]["tag"] == TAG
            and quality["commit"] == COMMIT and quality["tier"] == "full"
            and str(quality["run_id"]) == RUN, "Full CI provenance does not match reference")
    required = {"clang", "msvc", "static", "consumers", "codeql", "gcc", "properties",
                "query", "fuzz", "capabilities", "abi", "memcheck"}
    checked(set(quality["checks"]) == required
            and all(value == "success" for value in quality["checks"].values()),
            "incomplete Full CI quality gate")
    return {"tag": TAG, "commit": COMMIT, "release_url": release["html_url"],
            "published_at": release["published_at"], "full_ci_url": run["html_url"],
            "full_ci_note": "Core Release invokes Core Full CI; this is its enclosing run.",
            "approval": {"status": "pending", "required_before": "#646"}}


def archive(repository):
    actual = command(["git", "-C", repository, "rev-parse", f"{TAG}^{{commit}}"])
    checked(actual.decode().strip() == COMMIT, "local reference tag moved")
    data = command(["git", "-c", "core.autocrlf=false", "-c", "core.eol=lf",
                    "-C", repository, "archive", "--format=tar", COMMIT])
    with tarfile.open(fileobj=io.BytesIO(data)) as source:
        files = {}
        for member in source:
            if member.isdir():
                continue
            checked(member.isfile() and not Path(member.name).is_absolute()
                    and ".." not in Path(member.name).parts, "unsafe source archive member")
            files[member.name] = source.extractfile(member).read()
        return files


def inventory(files):
    return {name: {"sha256": digest(data), "size": len(data)} for name, data in sorted(files.items())}


def reference_files(repository):
    files = archive(repository)
    # The release packaging contract prevents Git discovery in an unrelated
    # parent checkout. A plain git archive does not contain this metadata.
    files["opentlv-source.txt"] = f"version=0.8.0\ncommit={COMMIT}\n".encode("ascii")
    return files


def prepare_source(repository, source):
    files = reference_files(repository)
    if source.exists():
        actual = {p.relative_to(source).as_posix(): p.read_bytes()
                  for p in source.rglob("*") if p.is_file()}
        checked(actual == files, "reference source directory differs from immutable Git archive")
    else:
        source.mkdir(parents=True)
        for name, data in files.items():
            path = source / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
    return files


def collect_ctest(xml):
    tree = ET.parse(xml)
    cases = tree.findall(".//testcase")
    checked(cases, "CTest returned no scenarios")
    results = []
    for case in cases:
        checked(case.find("failure") is None and case.find("error") is None
                and case.find("skipped") is None and case.get("status") == "run",
                f"CTest scenario did not pass: {case.get('name')}")
        results.append({"name": case.attrib["name"], "status": "passed",
                        "seconds": float(case.attrib["time"])})
    return results


def check_version(build):
    header = (build / "generated/include/tlv/version.h").read_text()
    for definition in ('OPENTLV_VERSION_STRING "0.8.0"', 'OPENTLV_VERSION_REVISION 0',
                       f'OPENTLV_GIT_COMMIT_HASH "{COMMIT}"', 'OPENTLV_GIT_BRANCH "archive"'):
        checked(f"#define {definition}\n" in header, f"incorrect generated reference version: {definition}")


def check_performance(data):
    workloads = {"ber_flat_small", "ber_flat_large", "ber_nested_definite", "ber_nested_indefinite",
                 "fixed_tlv", "fixed_ltv", "variable_tlv", "variable_sparse", "packed_lldp"}
    rows = data["benchmarks"]
    checked(rows and not any(row.get("error_occurred") for row in rows), "benchmark workload failed or absent")
    runs = [row for row in rows if row.get("run_type") == "iteration"]
    expected = {(f"reader_decode/{name}", index) for name in workloads for index in range(3)}
    checked(len(runs) == len(expected) and
            {(row["run_name"], row["repetition_index"]) for row in runs} == expected
            and all(row["iterations"] > 0 for row in runs), "incomplete Reader performance reference")


def candidate_configuration(metadata):
    fmt = metadata["format"]
    limit = metadata["options"]["max_value_size"]
    if fmt == "fixed":
        width = metadata["configuration"]["tag_size"]
        tags = ["01" * width]
        limit = min(limit, (1 << (8 * metadata["configuration"]["length_size"])) - 1)
    elif fmt in ("ber", "der", "emv"):
        tags = ["04", "80", "30", "a0"]
    elif fmt == "bluetooth-ltv":
        tags, limit = ["09", "ff"], min(limit, 254)
    elif fmt == "nfc-type2":
        tags, limit = ["03", "fd"], min(limit, 65534)
    else:
        raise ValueError(f"unknown candidate domain {fmt}")
    return [{"tag_hex": tag, "min_value_size": 0, "max_value_size": limit} for tag in tags]


def collect_generator(build, output):
    roots = sorted((build / "property-data/Release").glob("*/metadata.json"))
    checked(len(roots) == 10, "expected all ten property domains")
    for path in roots:
        metadata = read(path)
        metadata["generation_command"][0:3] = ["<build>/tests/property/property-generate",
                                                 metadata["format"], "<output>"]
        metadata["candidate_configuration"] = candidate_configuration(metadata)
        cases = [{"index": int(p.stem), "hex": p.read_bytes().hex(), "sha256": digest(p.read_bytes())}
                 for p in sorted(path.parent.glob("*.bin"))]
        checked(len(cases) == 32 and [c["index"] for c in cases] == list(range(32)),
                "incomplete generated corpus")
        write(output / "generator" / (path.parent.name + ".json"), {"metadata": metadata, "cases": cases})


def collect_diagnostics(source, output):
    # Use #644's family selection, but read values from the release's headers.
    import check_module_contracts as contracts
    mapping = read(ROOT / "docs/development/modules/diagnostic-mapping.json")
    families = []
    for family in mapping["families"]:
        # Newly split vocabularies are checked against the candidate headers,
        # but were not declarations in the immutable v0.8.0 reference.
        if family.get("baseline") is False:
            continue
        codes = (contracts.parse_enum(source, family["header"], family["typedef"])
                 if "typedef" in family else [(name, None) for name in sorted(contracts.limit_names(source))])
        checked(dict(codes) == {c["previous"]: c["value"] for c in family["codes"]},
                f"#644 mapping differs from release: {family['family']}")
        families.append({"family": family["family"], "header": family.get("header"),
                         "codes": dict(codes)})
    write(output / "diagnostic-codes.json", families)


def run_probe(source, build, work, output):
    executable = work / "baseline-probe"
    # The immutable reference has one archive; extracted candidates declare
    # their dependency closure in the generated module registry.
    libraries = [build / "tlv/libtlv.a"]
    registry = build / "modules-Release.json"
    if registry.is_file():
        modules = {entry["key"]: entry for entry in read(registry)}
        libraries += [Path(modules[key]["library"])
                      for key in ("emv", "asn1", "bluetooth", "nfc", "dhcp", "lldp",
                                  "schema-query-document", "schema-query", "schema-number",
                                  "codec-structure", "query-document", "query-codec", "query",
                                  "document", "generator", "codec", "schema", "io", "core")
                      if key in modules]
    command(["g++", "-std=c++23", "-DTLV_STATIC_DEFINE", "-I" + str(source / "tlv/include"),
             "-I" + str(build / "generated/include"), ROOT / "tests/baseline/probe.cpp",
             *libraries, "-o", executable], log=output / "logs/probe-build.txt")
    return json.loads(command([executable]))


def capture(args):
    checked(platform.system() == "Linux" and platform.machine() == "x86_64",
            "this capture configuration requires Linux x64 (WSL is supported)")
    identity = check_provenance(args.provenance)
    checked(not args.output.exists(), "capture requires a new output directory; never overwrite approved evidence")
    source, build = args.work / "source", args.work / "build"
    files = prepare_source(args.repository, source)
    args.output.mkdir(parents=True)
    for name in ("release.json", "ci-run.json", "release-manifest.json"):
        shutil.copyfile(args.provenance / name, args.output / name)
    write(args.output / "reference.json", identity)
    write(args.output / "source-inventory.json", inventory(files))
    seeds = {name: {"hex": data.hex(), "sha256": digest(data)} for name, data in files.items()
             if name.startswith("tests/fuzz/") and "/corpus/" in name}
    checked(seeds, "fuzz corpus missing")
    write(args.output / "fuzz-corpus.json", seeds)
    command(["cmake", "-S", source, "-B", build, *OPTIONS], log=args.output / "logs/configure.txt")
    check_version(build)
    command(["cmake", "--build", build, "--parallel", str(args.jobs)], log=args.output / "logs/build.txt")
    cache = (build / "CMakeCache.txt").read_text()
    settings = {line.split("=", 1)[0]: line.split("=", 1)[1] for line in cache.splitlines()
                if re.match(r"(?:OPENTLV_|CMAKE_(?:BUILD_TYPE|C_STANDARD|CXX_STANDARD|C_COMPILER:|CXX_COMPILER:))", line)
                and "=" in line}
    write(args.output / "environment.json", {
        "platform": platform.platform(), "machine": platform.machine(), "python": sys.version,
        "tools": {tool: command([tool, "--version"]).decode().splitlines()[0]
                  for tool in ("gcc", "g++", "cmake", "ninja", "git")},
        "dependencies": {name: command(["git", "-C", build / "_deps" / (name + "-src"),
                                        "rev-parse", "HEAD"]).decode().strip()
                         for name in ("googletest", "googlebenchmark")},
        "configure_options": OPTIONS, "cache": settings})
    xml = args.work / "ctest.xml"
    command(["ctest", "--test-dir", build, "--output-on-failure", "--no-tests=error",
             "--parallel", str(args.jobs), "--output-junit", xml], log=args.output / "logs/ctest.txt")
    write(args.output / "test-results.json", collect_ctest(xml))
    collect_generator(build, args.output)
    command([sys.executable, source / "tests/query/run.py", "--native", build / "tests/query/test-query-conformance",
             "--property-cases", "32", "--commit", COMMIT, "--evidence", args.output / "query"],
            log=args.output / "logs/query.txt")
    collect_diagnostics(source, args.output)
    write(args.output / "behavior.json", run_probe(source, build, args.work, args.output))
    benchmark = build / "benchmarks/benchmark-tlv"
    command([benchmark, "--benchmark_filter=^reader_decode/", "--benchmark_min_time=0.05s",
             "--benchmark_repetitions=3", "--benchmark_report_aggregates_only=false",
             "--benchmark_out_format=json", "--benchmark_out=" + str(args.output / "performance.json")],
            log=args.output / "logs/performance.txt")
    check_performance(read(args.output / "performance.json"))
    write(args.output / "capture-tools.json", inventory({name: (ROOT / name).read_bytes() for name in (
        "scripts/behavioral_baseline.py", "tests/baseline/probe.cpp",
        "scripts/check_module_contracts.py", "docs/development/modules/diagnostic-mapping.json")}))
    write(args.output / "checksums.json", inventory({p.relative_to(args.output).as_posix(): p.read_bytes()
                                                    for p in args.output.rglob("*") if p.is_file()}))
    print(f"Captured {len(read(args.output / 'test-results.json'))} passing tests at {COMMIT}")


def verify(baseline, repository):
    check_provenance(baseline)
    expected = read(baseline / "checksums.json")
    actual = inventory({p.relative_to(baseline).as_posix(): p.read_bytes()
                        for p in baseline.rglob("*") if p.is_file() and p != baseline / "checksums.json"})
    checked(actual == expected, "baseline file inventory or checksum mismatch")
    checked(read(baseline / "reference.json") == check_provenance(baseline), "reference identity mismatch")
    checked(inventory(reference_files(repository)) == read(baseline / "source-inventory.json"),
            "reference source inventory differs from Git")
    for path in (baseline / "generator").glob("*.json"):
        fixture = read(path)
        for case in fixture["cases"]:
            checked(digest(bytes.fromhex(case["hex"])) == case["sha256"], "generator fixture is corrupt")
    print("Reference provenance, source inventory and evidence checksums verified.")


def replay(args):
    verify(args.baseline, args.repository)
    with tempfile.TemporaryDirectory(prefix="opentlv-baseline-") as temporary:
        work = Path(temporary)
        for path in sorted((args.baseline / "generator").glob("*.json")):
            fixture = read(path)
            metadata = fixture["metadata"]
            target = work / path.stem
            target.mkdir()
            argv = metadata["generation_command"][:]
            argv[0] = str(args.build / "tests/property/property-generate")
            argv[2] = str(target)
            command(argv)
            for case in fixture["cases"]:
                wire = (target / f"{case['index']:06d}.bin").read_bytes()
                if metadata["transform"] == "nfc-null-prefix-terminator-suffix-v1":
                    wire = b"\0" + wire + b"\xfe"
                checked(wire.hex() == case["hex"], f"generator drift: {path.stem}/{case['index']}")
        observed = run_probe(args.source, args.build, work, work)
        checked(observed == read(args.baseline / "behavior.json"), "behavior or diagnostic drift")
        command([sys.executable, args.source / "tests/query/run.py", "--native",
                 args.build / "tests/query/test-query-conformance", "--property-cases", "32",
                 "--commit", COMMIT, "--evidence", work / "query"])
        for name in ("manifest.json", "wire.json", "expected.json"):
            checked(read(work / "query" / name) == read(args.baseline / "query" / name), "Query fixture drift")
    print("All 320 Generator cases, Query fixtures and behavioral observations match.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    capture_parser = commands.add_parser("capture")
    capture_parser.add_argument("--work", type=Path, required=True)
    capture_parser.add_argument("--output", type=Path, required=True)
    capture_parser.add_argument("--provenance", type=Path, required=True)
    capture_parser.add_argument("--jobs", type=int, default=8)
    verify_parser = commands.add_parser("verify")
    replay_parser = commands.add_parser("replay")
    replay_parser.add_argument("--source", type=Path, required=True)
    replay_parser.add_argument("--build", type=Path, required=True)
    for sub in (capture_parser, verify_parser, replay_parser):
        sub.add_argument("--repository", type=Path, default=ROOT)
    for sub in (verify_parser, replay_parser):
        sub.add_argument("--baseline", type=Path, default=DEFAULT)
    args = parser.parse_args()
    for key, value in vars(args).items():
        if isinstance(value, Path):
            setattr(args, key, value.resolve())
    try:
        if args.command == "capture":
            capture(args)
        elif args.command == "verify":
            verify(args.baseline, args.repository)
        else:
            replay(args)
    except (OSError, ValueError, KeyError, ET.ParseError) as error:
        print(error, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
