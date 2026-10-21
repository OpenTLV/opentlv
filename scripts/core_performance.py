#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Marek Cingel

"""Measure immutable Core revisions together and validate release evidence.

Performance changes are advisory. Missing or incomparable measurements fail.
The stable release supplies one workload source for both libraries. Seven
independent, alternating benchmark runs expose variation without a percentage
budget: disjoint observed ranges indicate a change, overlapping ranges noise.
These ranges are descriptive, not statistical confidence intervals.
"""

import argparse
from datetime import datetime, timezone
import gzip
import hashlib
import json
import math
import os
from pathlib import Path, PurePosixPath
import platform
import re
import shutil
import statistics
import subprocess
import sys
import tarfile
import tempfile

from callgrind_compare import WORKLOADS, require_equivalent, run, validate_result, write_json
from callgrind_report import parse_profile


ROOT = Path(__file__).resolve().parents[1]
ASSETS = ("performance.json", "performance.md", "performance-data.tar.gz")
SIDES = ("baseline", "candidate")
HARNESS_FILES = (
    "src/tlv_benchmarks.cpp", "src/reader_decode_benchmarks.cpp",
    "src/schema_benchmarks.cpp", "src/cxx_facade_benchmarks.cpp",
    "callgrind/workloads.c", "callgrind/schema_workloads.h",
)
# Resolved v1.9.4; never fetch a moving branch or version tag for a release.
BENCHMARK_COMMIT = "eddb0241389718a23a42db6af5f0164b6e0139af"
CONFIGURATION = {
    "build_type": "Release", "c_flags": "-O3 -g -DNDEBUG",
    "cxx_flags": "-O3 -g -DNDEBUG", "c_standard": 99, "cxx_standard": 11,
    "linkage": "static", "lto": False, "capabilities": "all",
    "benchmark_repetitions": 7, "benchmark_min_time": "0.05s",
    "benchmark_filter": ".", "callgrind_iterations": 2000,
    "time_metric": "cpu_time_ns", "time_classification": "disjoint-observed-ranges",
    "callgrind_workloads": list(WORKLOADS),
}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def digest(path):
    with Path(path).open("rb") as stream:
        value = hashlib.sha256()
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            value.update(block)
    return value.hexdigest()


def object_digest(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, allow_nan=False).encode()).hexdigest()


def load_json(path):
    def invalid(value):
        raise ValueError(f"Nonfinite JSON value: {value}")

    def unique(pairs):
        result = {}
        for key, value in pairs:
            require(key not in result, f"Duplicate JSON key: {key}")
            result[key] = value
        return result

    def finite_float(value):
        number = float(value)
        require(math.isfinite(number), "Nonfinite JSON number")
        return number

    return json.loads(Path(path).read_text(encoding="utf-8"),
                      parse_constant=invalid, parse_float=finite_float, object_pairs_hook=unique)


def identity(commit, tag, repository, run_id, attempt, baseline):
    require(isinstance(commit, str) and re.fullmatch(r"[0-9a-f]{40}", commit),
            "Candidate must be an exact commit SHA")
    require(isinstance(tag, str) and re.fullmatch(
        r"v(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)(?:-[0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*)?", tag),
        "Expected a v-prefixed release tag")
    for component in tag.split("-", 1)[1].split(".") if "-" in tag else []:
        require(not (component.isdigit() and len(component) > 1 and component[0] == "0"),
                "Prerelease numbers cannot have leading zeroes")
    require(isinstance(repository, str) and re.fullmatch(r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+", repository),
            "Expected repository owner/name")
    require(re.fullmatch(r"[1-9][0-9]*", str(run_id)) and
            re.fullmatch(r"[1-9][0-9]*", str(attempt)), "Expected run ID and attempt")
    require(isinstance(baseline, dict) and set(baseline) == {
        "release_id", "tag", "version", "commit", "url", "published_at"}, "Invalid baseline release lock")
    require(type(baseline["release_id"]) is int and baseline["release_id"] > 0,
            "Invalid baseline release ID")
    require(isinstance(baseline["commit"], str) and re.fullmatch(r"[0-9a-f]{40}", baseline["commit"]),
            "Baseline must be an exact commit SHA")
    require(isinstance(baseline["version"], str) and re.fullmatch(
        r"(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)", baseline["version"]),
        "Baseline must be a stable version")
    require(baseline["tag"] in (baseline["version"], "v" + baseline["version"]),
            "Baseline tag/version mismatch")
    require(baseline["url"] == f"https://github.com/{repository}/releases/tag/{baseline['tag']}",
            "Baseline release URL mismatch")
    require(isinstance(baseline["published_at"], str) and
            datetime.fromisoformat(baseline["published_at"].replace("Z", "+00:00")).tzinfo is not None,
            "Baseline needs a publication timestamp")
    return {"commit": commit, "tag": tag, "repository": repository, "run_id": str(run_id),
            "run_attempt": str(attempt), "baseline": baseline}


def timing_comparison(baseline, candidate):
    for samples in (baseline, candidate):
        require(len(samples) == CONFIGURATION["benchmark_repetitions"], "Incomplete timing repetitions")
        require(all(type(value) in (float, int) and math.isfinite(value) and value > 0
                    for value in samples), "Invalid timing measurement")
    classification = ("improvement" if max(candidate) < min(baseline) else
                      "regression" if min(candidate) > max(baseline) else "noise")
    before, after = statistics.median(baseline), statistics.median(candidate)
    return {"baseline_median_ns": before, "candidate_median_ns": after,
            "baseline_range_ns": [min(baseline), max(baseline)],
            "candidate_range_ns": [min(candidate), max(candidate)],
            "change_percent": (after / before - 1) * 100, "classification": classification}


def inventory(path):
    names = Path(path).read_text(encoding="utf-8").splitlines()
    require(names and all(name and name == name.strip() for name in names) and
            len(names) == len(set(names)), "Empty or duplicate benchmark inventory")
    return sorted(names)


def benchmark_samples(path, expected):
    value = load_json(path)
    require(isinstance(value, dict) and isinstance(value.get("context"), dict) and
            isinstance(value.get("benchmarks"), list), "Malformed benchmark JSON")
    require(value["context"].get("library_build_type") == "release", "Benchmark library is not optimized")
    samples = {}
    for entry in value["benchmarks"]:
        require(isinstance(entry, dict) and entry.get("run_type") == "iteration" and
                not entry.get("error_occurred") and not entry.get("error_message"),
                "Failed, skipped or aggregate benchmark measurement")
        name = entry.get("name")
        require(isinstance(name, str) and name not in samples, "Duplicate or unnamed benchmark")
        require(type(entry.get("iterations")) is int and entry["iterations"] > 0,
                f"Empty benchmark: {name}")
        unit = entry.get("time_unit")
        require(unit in ("ns", "us", "ms", "s"), "Unknown benchmark time unit")
        for field in ("real_time", "cpu_time"):
            number = entry.get(field)
            require(type(number) in (float, int) and math.isfinite(number) and number > 0,
                    f"Invalid {field}: {name}")
        samples[name] = entry["cpu_time"] * {"ns": 1, "us": 1000, "ms": 1000000, "s": 1000000000}[unit]
        require(math.isfinite(samples[name]), f"Nonfinite normalized timing: {name}")
    require(sorted(samples) == expected, "Benchmark inventory differs from complete suite")
    return samples


def compare_raw(raw):
    names = inventory(raw / "benchmark-inventory-baseline.txt")
    require(names == inventory(raw / "benchmark-inventory-candidate.txt"), "Library benchmark inventories differ")
    require(set(file_manifest(raw / "benchmarks")) == {
        f"{side}-{number}.json" for side in SIDES for number in range(CONFIGURATION["benchmark_repetitions"])},
        "Missing or unexpected benchmark sample files")
    allowed_profiles = {f"{side}-{workload}{suffix}" for side in SIDES for workload in WORKLOADS
                        for suffix in (".json", ".out.1", ".out", ".log")}
    require(set(file_manifest(raw / "callgrind")).issubset(allowed_profiles), "Unexpected Callgrind workload files")
    samples = {side: [benchmark_samples(raw / "benchmarks" / f"{side}-{number}.json", names)
                      for number in range(CONFIGURATION["benchmark_repetitions"])] for side in SIDES}
    timings = [{"name": name, **timing_comparison(
        [sample[name] for sample in samples["baseline"]],
        [sample[name] for sample in samples["candidate"]])} for name in names]
    instructions = []
    for workload in WORKLOADS:
        results, profiles = {}, {}
        for side in SIDES:
            stem = raw / "callgrind" / f"{side}-{workload}"
            result = load_json(stem.with_suffix(".json"))
            results[side] = validate_result(json.dumps(result, allow_nan=False), workload,
                                            CONFIGURATION["callgrind_iterations"])
            profiles[side] = parse_profile(stem.with_suffix(".out.1"))
            require(profiles[side]["instructions"] > 0, "Empty Callgrind measured region")
        require_equivalent(results["baseline"], results["candidate"])
        before, after = (profiles[side]["instructions"] for side in SIDES)
        symbols = set(profiles["baseline"]["functions"]) | set(profiles["candidate"]["functions"])
        changes = [{"name": symbol,
                    "baseline_ir": profiles["baseline"]["functions"].get(symbol, 0),
                    "candidate_ir": profiles["candidate"]["functions"].get(symbol, 0)} for symbol in symbols]
        changes.sort(key=lambda item: (-abs(item["candidate_ir"] - item["baseline_ir"]), item["name"]))
        instructions.append({"name": workload, "input_id": results["baseline"]["input_id"],
                             "input_bytes": results["baseline"]["input_bytes"],
                             "checksum": results["baseline"]["checksum"],
                             "baseline_ir": before, "candidate_ir": after,
                             "change_percent": (after / before - 1) * 100,
                             "classification": "improvement" if after < before else
                             "regression" if after > before else "unchanged",
                             "largest_function_changes": changes[:10]})
    return {"benchmark_inventory": names, "timings": timings, "instructions": instructions}


def render_markdown(report):
    item = report["identity"]
    baseline = item["baseline"]
    lines = ["# Core release performance", "",
             f"Candidate: `{item['tag']}` / `{item['commit']}`.", "",
             f"Stable baseline: [{baseline['tag']}]({baseline['url']}) / `{baseline['commit']}`.", "",
             f"Run: `{item['repository']}` / `{item['run_id']}` / attempt `{item['run_attempt']}`.", "",
             "Both revisions were rebuilt and measured on this runner with the stable release's workloads.",
             "This is a remeasurement of the stable source, not a claim about historical release artifacts.", "",
             "Changes are advisory. Time changes require disjoint observed ranges across seven alternating runs;",
             "overlap is classified as noise. These ranges are not confidence intervals. Ir counts are deterministic",
             "for the recorded toolchain, configuration and workload. Missing evidence blocks publication.", "",
             "## Google Benchmark", "",
             "| Workload | Baseline median ns | Candidate median ns | Change | Classification |",
             "| --- | ---: | ---: | ---: | --- |"]
    for row in report["comparison"]["timings"]:
        lines.append(f"| `{row['name'].replace('|', '&#124;')}` | {row['baseline_median_ns']:.3f} | "
                     f"{row['candidate_median_ns']:.3f} | {row['change_percent']:+.2f}% | {row['classification']} |")
    lines += ["", "## Callgrind", "", "| Workload | Baseline Ir | Candidate Ir | Change | Classification |",
              "| --- | ---: | ---: | ---: | --- |"]
    for row in report["comparison"]["instructions"]:
        lines.append(f"| `{row['name']}` | {row['baseline_ir']} | {row['candidate_ir']} | "
                     f"{row['change_percent']:+.2f}% | {row['classification']} |")
    lines += ["", "Full samples, profiles, checksums, build commands and workload sources are in `performance-data.tar.gz`.",
              f"Harness SHA-256: `{report['metadata']['harness_sha256']}`.", ""]
    return "\n".join(lines)


def safe_extract(archive, destination, expected=None):
    """Extract regular files only; reject traversal, duplicate names and links."""
    seen = set()
    with tarfile.open(archive, "r:*") as source:
        for member in source:
            name = PurePosixPath(member.name)
            require(not name.is_absolute() and ".." not in name.parts and "\\" not in member.name and
                    ":" not in member.name and str(name) == member.name.rstrip("/") and name.parts,
                    "Unsafe archive path")
            require(member.name not in seen, "Duplicate archive member")
            seen.add(member.name)
            require(member.isfile() or member.isdir(), "Archive links and special files are forbidden")
            if member.isdir():
                require(expected is None, "Unexpected directory in evidence archive")
                continue
            if expected is not None:
                require(member.name in expected, "Unexpected evidence archive member")
            target = destination.joinpath(*name.parts)
            target.parent.mkdir(parents=True, exist_ok=True)
            with source.extractfile(member) as src, target.open("wb") as dst:
                shutil.copyfileobj(src, dst)
    if expected is not None:
        require(seen == set(expected), "Incomplete evidence archive")


def file_manifest(directory):
    return {path.relative_to(directory).as_posix(): digest(path)
            for path in sorted(directory.rglob("*")) if path.is_file()}


def validate_metadata(raw, metadata, expected):
    require(isinstance(metadata, dict) and metadata.get("configuration") == CONFIGURATION,
            "Unexpected performance configuration")
    require(metadata.get("harness_commit") == expected["baseline"]["commit"] and
            metadata.get("baseline_mode") == "remeasured-stable-release", "Wrong workload revision or baseline mode")
    require(metadata.get("benchmark_dependency_commit") == BENCHMARK_COMMIT, "Unpinned benchmark dependency")
    harness = file_manifest(raw / "harness")
    require(set(harness) == set(HARNESS_FILES) | {"CMakeLists.txt"} and
            metadata.get("harness_files") == harness and metadata.get("harness_sha256") == object_digest(harness),
            "Workload source hashes differ")
    require((raw / "harness" / "CMakeLists.txt").read_text(encoding="utf-8") == harness_cmake(),
            "Changed benchmark build harness")
    toolchain = metadata.get("toolchain")
    require(isinstance(toolchain, dict) and all(isinstance(toolchain.get(key), str) and toolchain[key]
            for key in ("cc", "cxx", "cc_target", "cxx_target", "valgrind", "cmake", "ninja", "libc", "python")),
            "Incomplete toolchain provenance")
    runner = metadata.get("runner")
    require(isinstance(runner, dict) and all(isinstance(runner.get(key), str) and runner[key]
            for key in ("platform", "machine", "cpu_model")) and
            isinstance(runner.get("cpu_affinity"), list) and runner["cpu_affinity"], "Incomplete runner provenance")
    for side in SIDES:
        source = load_json(raw / "sources" / f"{side}.json")
        commit = expected["commit"] if side == "candidate" else expected["baseline"]["commit"]
        require(source.get("commit") == commit and isinstance(source.get("files"), dict) and source["files"] and
                source.get("files_sha256") == object_digest(source["files"]), "Source revision/manifest mismatch")
        require(isinstance(source.get("git_tree"), str) and re.fullmatch(r"[0-9a-f]{40}", source["git_tree"]) and
                isinstance(source.get("archive_sha256"), str) and
                re.fullmatch(r"[0-9a-f]{64}", source["archive_sha256"]) and
                all(isinstance(value, str) and re.fullmatch(r"[0-9a-f]{64}", value)
                    for value in source["files"].values()), "Invalid immutable source hashes")
        if side == "baseline":
            require(all(source["files"].get("benchmarks/" + name) == harness[name] for name in HARNESS_FILES),
                    "Harness differs from the stable release source manifest")
        build = raw / "build-metadata" / side
        for name in ("CMakeCache.txt", "compile_commands.json", "config.h", "binaries.json"):
            require((build / name).is_file(), f"Missing {side} build provenance: {name}")
        cache = dict(re.findall(r"^([^/#\n][^:\n]*):[^=\n]+=(.*)$", (build / "CMakeCache.txt").read_text(encoding="utf-8"), re.M))
        required = {"CMAKE_BUILD_TYPE": "Release", "CMAKE_C_FLAGS": "", "CMAKE_CXX_FLAGS": "",
                    "CMAKE_C_FLAGS_RELEASE": CONFIGURATION["c_flags"],
                    "CMAKE_CXX_FLAGS_RELEASE": CONFIGURATION["cxx_flags"],
                    "CMAKE_EXE_LINKER_FLAGS": "", "OPENTLV_BUILD_SHARED_LIBS": "OFF", "OPENTLV_BUILD_CXX": "ON"}
        require(all(cache.get(key) == value for key, value in required.items()), "Different or unoptimized build flags")
        commands = load_json(build / "compile_commands.json")
        require(isinstance(commands, list) and commands and all(isinstance(entry, dict) and
                isinstance(entry.get("file"), str) and (entry.get("command") or entry.get("arguments"))
                for entry in commands), "Missing compilation commands")
        binaries = load_json(build / "binaries.json")
        require(isinstance(binaries, dict) and set(binaries) == {"benchmark-tlv", "opentlv-perf-workload"} and
                all(isinstance(value, str) and re.fullmatch(r"[0-9a-f]{64}", value) for value in binaries.values()),
                "Missing measured binary hashes")
    require((raw / "build-metadata" / "baseline" / "config.h").read_bytes() ==
            (raw / "build-metadata" / "candidate" / "config.h").read_bytes(), "Library capabilities differ")


def validate_report(directory, commit, tag, repository, run_id, attempt, baseline):
    """Verify the three distributable assets and recompute every comparison."""
    directory = Path(directory)
    expected = identity(commit, tag, repository, run_id, attempt, baseline)
    report = load_json(directory / ASSETS[0])
    require(isinstance(report, dict) and type(report.get("schema")) is int and
            report["schema"] == 1 and report.get("advisory") is True,
            "Invalid performance report schema")
    require(report.get("identity") == expected, "Performance report identity differs from release lock")
    require(report.get("archive") == {"name": ASSETS[2], "sha256": digest(directory / ASSETS[2])},
            "Performance archive hash mismatch")
    files = report.get("files")
    require(isinstance(files, dict) and files and all(isinstance(key, str) and key.startswith("raw/") and
            isinstance(value, str) and re.fullmatch(r"[0-9a-f]{64}", value) for key, value in files.items()),
            "Invalid raw evidence manifest")
    with tempfile.TemporaryDirectory(prefix="opentlv-performance-validate-") as temporary:
        extracted = Path(temporary)
        safe_extract(directory / ASSETS[2], extracted, files)
        require(file_manifest(extracted) == files, "Raw evidence hash mismatch")
        raw = extracted / "raw"
        require(load_json(raw / "identity.json") == expected, "Raw evidence identity mismatch")
        metadata = load_json(raw / "metadata.json")
        require(metadata == report.get("metadata"), "Raw metadata differs from report")
        validate_metadata(raw, metadata, expected)
        require(compare_raw(raw) == report.get("comparison"), "Performance comparisons are missing or altered")
    require((directory / ASSETS[1]).read_text(encoding="utf-8") == render_markdown(report),
            "Markdown report differs from measured evidence")
    return report


def harness_cmake():
    return '''cmake_minimum_required(VERSION 3.16)
project(opentlv-release-performance LANGUAGES C CXX)
include(FetchContent)
foreach(feature IN ITEMS TESTS UNIT_TESTS INTEGRATION_TESTS QUERY_TESTS PROPERTY_TESTS
        CLI_TESTS EXAMPLES CLI BENCHMARKS FUZZING MEMCHECK DOCS WASM PYTHON LUA SHARED_LIBS)
    # Migrated consumer switches also disable bindings in historical baselines.
    set(OPENTLV_BUILD_${feature} OFF CACHE BOOL "" FORCE)
endforeach()
set(OPENTLV_BUILD_CXX ON CACHE BOOL "" FORCE)
foreach(feature IN ITEMS READER WRITER DOCUMENT QUERY QUERY_FRONTEND QUERY_SET_OPERATIONS
        SCHEMA CODEC FORMAT_ASN1 FORMAT_BER FORMAT_DER FORMAT_CER EMV BLUETOOTH NFC DHCP LLDP)
    set(OPENTLV_${feature} ON CACHE BOOL "" FORCE)
endforeach()
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION OFF)
add_subdirectory("${OPENTLV_SOURCE_DIR}" opentlv EXCLUDE_FROM_ALL)
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_WERROR OFF CACHE BOOL "" FORCE)
FetchContent_Declare(googlebenchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG ''' + BENCHMARK_COMMIT + ''')
FetchContent_MakeAvailable(googlebenchmark)
add_executable(benchmark-tlv src/tlv_benchmarks.cpp src/reader_decode_benchmarks.cpp
    src/schema_benchmarks.cpp src/cxx_facade_benchmarks.cpp)
target_link_libraries(benchmark-tlv PRIVATE tlv tlv++ benchmark::benchmark_main)
add_executable(opentlv-perf-workload callgrind/workloads.c)
target_compile_definitions(opentlv-perf-workload PRIVATE
    _POSIX_C_SOURCE=200809L OPENTLV_CALLGRIND_INSTRUMENTATION=1)
find_path(CALLGRIND_INCLUDE_DIR valgrind/callgrind.h REQUIRED)
target_include_directories(opentlv-perf-workload PRIVATE "${CALLGRIND_INCLUDE_DIR}")
target_link_libraries(opentlv-perf-workload PRIVATE tlv)
'''


def archive_revision(repository, commit, destination, archive):
    require(run(["git", "-C", repository, "rev-parse", commit + "^{commit}"]).strip() == commit,
            "Commit cannot be resolved exactly")
    run(["git", "-C", repository, "archive", "--format=tar", "--output", archive, commit])
    destination.mkdir()
    safe_extract(archive, destination)
    files = file_manifest(destination)
    return {"commit": commit, "git_tree": run(["git", "-C", repository, "rev-parse", commit + "^{tree}"]).strip(),
            "archive_sha256": digest(archive), "files": files, "files_sha256": object_digest(files)}


def collect_metadata(harness, expected, cc, cxx, env):
    cpu_info = Path("/proc/cpuinfo").read_text(encoding="utf-8")
    cpu_model = next((line.split(":", 1)[1].strip() for line in cpu_info.splitlines()
                      if line.startswith(("model name", "Hardware", "Processor")) and ":" in line), platform.machine())
    versions = {"cc": [cc, "--version"], "cxx": [cxx, "--version"],
                "cc_target": [cc, "-dumpmachine"], "cxx_target": [cxx, "-dumpmachine"],
                "valgrind": ["valgrind", "--version"], "cmake": ["cmake", "--version"],
                "ninja": ["ninja", "--version"], "libc": ["ldd", "--version"]}
    files = file_manifest(harness)
    return {"created_at": datetime.now(timezone.utc).isoformat(),
            "baseline_mode": "remeasured-stable-release", "harness_commit": expected["baseline"]["commit"],
            "harness_files": files, "harness_sha256": object_digest(files),
            "benchmark_dependency_commit": BENCHMARK_COMMIT, "configuration": CONFIGURATION,
            "toolchain": {**{key: run(command, env=env).strip() for key, command in versions.items()},
                          "python": platform.python_version()},
            "runner": {"platform": platform.platform(), "machine": platform.machine(), "cpu_model": cpu_model,
                       "cpu_affinity": sorted(os.sched_getaffinity(0)),
                       "image_os": env.get("ImageOS", "unknown"), "image_version": env.get("ImageVersion", "unknown")}}


def build_command(harness, source, build, cc, cxx):
    return ["cmake", "-S", harness, "-B", build, "-G", "Ninja", f"-DOPENTLV_SOURCE_DIR={source}",
            f"-DCMAKE_C_COMPILER={cc}", f"-DCMAKE_CXX_COMPILER={cxx}", "-DCMAKE_BUILD_TYPE=Release",
            "-DCMAKE_C_STANDARD=99", "-DCMAKE_C_STANDARD_REQUIRED=ON", "-DCMAKE_C_EXTENSIONS=OFF",
            "-DCMAKE_CXX_STANDARD=11", "-DCMAKE_CXX_STANDARD_REQUIRED=ON", "-DCMAKE_CXX_EXTENSIONS=OFF",
            "-DCMAKE_C_FLAGS=", "-DCMAKE_CXX_FLAGS=", "-DCMAKE_EXE_LINKER_FLAGS=",
            "-DCMAKE_SHARED_LINKER_FLAGS=", f"-DCMAKE_C_FLAGS_RELEASE={CONFIGURATION['c_flags']}",
            f"-DCMAKE_CXX_FLAGS_RELEASE={CONFIGURATION['cxx_flags']}", "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"]


def package_report(output, expected, metadata):
    raw = output / "raw"
    comparison = compare_raw(raw)
    validate_metadata(raw, metadata, expected)
    files = {"raw/" + name: value for name, value in file_manifest(raw).items()}
    with (output / ASSETS[2]).open("wb") as stream, gzip.GzipFile(filename="", mode="wb", fileobj=stream, mtime=0) as zipped:
        with tarfile.open(fileobj=zipped, mode="w") as archive:
            for name in sorted(files):
                info = archive.gettarinfo(str(output / name), arcname=name)
                info.uid = info.gid = 0
                info.uname = info.gname = ""
                info.mtime = 0
                with (output / name).open("rb") as content:
                    archive.addfile(info, content)
    report = {"schema": 1, "advisory": True, "identity": expected, "metadata": metadata,
              "comparison": comparison, "files": files,
              "archive": {"name": ASSETS[2], "sha256": digest(output / ASSETS[2])}}
    write_json(output / ASSETS[0], report)
    (output / ASSETS[1]).write_text(render_markdown(report), encoding="utf-8")
    validate_report(output, expected["commit"], expected["tag"], expected["repository"],
                    expected["run_id"], expected["run_attempt"], expected["baseline"])
    return report


def measure(args):
    require(sys.platform == "linux", "Release performance requires native Linux")
    record = load_json(args.candidate_record)
    expected = identity(record["commit"], record["tag"], record.get("repository", os.getenv("GITHUB_REPOSITORY")),
                        record.get("run_id", os.getenv("GITHUB_RUN_ID")),
                        record.get("run_attempt", os.getenv("GITHUB_RUN_ATTEMPT")), record["baseline"])
    output = args.output.resolve()
    require(not output.exists() or not any(output.iterdir()), "Performance output must be empty")
    output.mkdir(parents=True, exist_ok=True)
    raw = output / "raw"
    raw.mkdir()
    write_json(raw / "identity.json", expected)
    env = os.environ.copy()
    for key in ("CFLAGS", "CXXFLAGS", "CPPFLAGS", "LDFLAGS", "CMAKE_TOOLCHAIN_FILE",
                "CMAKE_PROJECT_TOP_LEVEL_INCLUDES", "CMAKE_PREFIX_PATH"):
        env.pop(key, None)
    cc, cxx = shutil.which(args.cc), shutil.which(args.cxx)
    require(cc and cxx, "C and C++ compilers are required")
    with tempfile.TemporaryDirectory(prefix="opentlv-release-performance-") as temporary:
        work = Path(temporary)
        sources = {}
        for side in SIDES:
            sources[side] = work / side
            commit = expected["commit"] if side == "candidate" else expected["baseline"]["commit"]
            source = archive_revision(args.source_root.resolve(), commit, sources[side], work / f"{side}.tar")
            write_json(raw / "sources" / f"{side}.json", source)
        harness = work / "harness"
        for name in HARNESS_FILES:
            destination = harness / name
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(sources["baseline"] / "benchmarks" / name, destination)
        (harness / "CMakeLists.txt").write_text(harness_cmake(), encoding="utf-8")
        shutil.copytree(harness, raw / "harness")
        metadata = collect_metadata(harness, expected, cc, cxx, env)
        write_json(raw / "metadata.json", metadata)
        builds = {}
        for side in SIDES:
            print(f"Building {side} with the pinned stable workload harness", flush=True)
            build = work / f"build-{side}"
            builds[side] = build
            run(build_command(harness, sources[side], build, cc, cxx), raw / "logs" / f"{side}-configure.log", env)
            run(["cmake", "--build", build, "--target", "benchmark-tlv", "opentlv-perf-workload",
                 "--parallel", args.jobs], raw / "logs" / f"{side}-build.log", env, timeout=1800)
            destination = raw / "build-metadata" / side
            destination.mkdir(parents=True)
            for name in ("CMakeCache.txt", "compile_commands.json"):
                shutil.copyfile(build / name, destination / name)
            shutil.copyfile(build / "opentlv" / "generated" / "include" / "tlv" / "config.h", destination / "config.h")
            write_json(destination / "binaries.json", {name: digest(build / name)
                       for name in ("benchmark-tlv", "opentlv-perf-workload")})
            listing = run([build / "benchmark-tlv", "--benchmark_list_tests=true"],
                          raw / "logs" / f"{side}-inventory.log", env)
            (raw / f"benchmark-inventory-{side}.txt").write_text(listing, encoding="utf-8")
        require(inventory(raw / "benchmark-inventory-baseline.txt") == inventory(raw / "benchmark-inventory-candidate.txt"),
                "Workload inventory differs across library builds")
        (raw / "benchmarks").mkdir()
        for repetition in range(CONFIGURATION["benchmark_repetitions"]):
            order = SIDES if repetition % 2 == 0 else tuple(reversed(SIDES))
            for side in order:
                print(f"Benchmark repetition {repetition + 1}/7: {side}", flush=True)
                result = raw / "benchmarks" / f"{side}-{repetition}.json"
                run([builds[side] / "benchmark-tlv", "--benchmark_filter=.", "--benchmark_min_time=0.05s",
                     "--benchmark_repetitions=1", "--benchmark_out_format=json", f"--benchmark_out={result}"],
                    raw / "logs" / f"{side}-benchmark-{repetition}.log", env, timeout=1800)
                benchmark_samples(result, inventory(raw / f"benchmark-inventory-{side}.txt"))
        (raw / "callgrind").mkdir()
        for workload in WORKLOADS:
            for side in SIDES:
                print(f"Callgrind: {side} {workload}", flush=True)
                stem = raw / "callgrind" / f"{side}-{workload}"
                result = run(["valgrind", "--tool=callgrind", "--instr-atstart=no", "--cache-sim=no",
                              "--branch-sim=no", "--collect-atstart=yes", "--error-exitcode=99",
                              f"--callgrind-out-file={stem}.out", f"--log-file={stem}.log",
                              builds[side] / "opentlv-perf-workload", workload, CONFIGURATION["callgrind_iterations"]],
                             raw / "logs" / f"{side}-{workload}.log", env)
                write_json(stem.with_suffix(".json"), validate_result(result, workload, CONFIGURATION["callgrind_iterations"]))
        for side in SIDES:
            require(file_manifest(sources[side]) == load_json(raw / "sources" / f"{side}.json")["files"],
                    f"{side} source changed during measurement")
        require(file_manifest(harness) == metadata["harness_files"], "Workload sources changed during measurement")
        package_report(output, expected, metadata)
    print(f"Validated performance evidence: {output}", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    execute = commands.add_parser("run")
    execute.add_argument("--candidate-record", type=Path, required=True)
    execute.add_argument("--source-root", type=Path, default=ROOT)
    execute.add_argument("--output", type=Path, required=True)
    execute.add_argument("--cc", default="gcc")
    execute.add_argument("--cxx", default="g++")
    execute.add_argument("--jobs", type=int, default=2)
    check = commands.add_parser("validate")
    check.add_argument("--candidate-record", type=Path, required=True)
    check.add_argument("--directory", type=Path, required=True)
    args = parser.parse_args()
    try:
        if args.command == "run":
            require(args.jobs > 0, "Build parallelism must be positive")
            measure(args)
        else:
            record = load_json(args.candidate_record)
            validate_report(args.directory, record["commit"], record["tag"], record["repository"],
                            record["run_id"], record["run_attempt"], record["baseline"])
    except (ValueError, RuntimeError, OSError, KeyError, TypeError, tarfile.TarError, subprocess.SubprocessError) as error:
        print(f"Performance evidence rejected: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
