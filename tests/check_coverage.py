"""Measure production code exercised by the Core, IO and integration CTest suites."""

import argparse
import json
import os
import re
import subprocess
import tempfile
from collections import Counter, defaultdict
from pathlib import Path

from run_ctest import run as run_ctest


ROOT = Path(__file__).resolve().parents[1]
READER = "src/io/reader/reader.c"
WRITER = "src/io/writer/writer.c"
# Reviewed in docs/coverage.md. Branch indices are zero-based within a line.
EXCLUDED_LINES = set()
EXCLUDED_BRANCHES = {(READER, 90, 6), (WRITER, 78, 4)}


def read_profiles(build, gcov, version):
    by_source = defaultdict(list)
    for notes in sorted(build.rglob("*.gcno")):
        if not notes.with_suffix(".gcda").is_file():
            raise SystemExit(f"Missing execution counts for {notes.name}")
        output = subprocess.check_output([gcov, "--json-format", "--branch-probabilities",
                                          "--stdout", notes], text=True)
        profile = json.loads(output)
        if profile["gcc_version"] != version:
            raise SystemExit(f"{notes.name} was compiled with GCC {profile['gcc_version']}, expected {version}")
        for entry in profile["files"]:
            path = (Path(profile["current_working_directory"]) / entry["file"]).resolve()
            if ROOT / "src" not in path.parents:
                continue
            name = path.relative_to(ROOT).as_posix()
            by_source[name].append((notes.relative_to(build).as_posix(), entry))
    expected = {path.relative_to(ROOT).as_posix() for path in (ROOT / "src").rglob("*.c")}
    expected.update({"src/core/internal/fixed.h", "src/core/internal/validation.h",
                     "src/io/internal/diagnostic.h"})
    missing = sorted(expected - by_source.keys())
    unexpected = sorted(by_source.keys() - expected)
    if missing or unexpected:
        raise SystemExit(f"Production coverage scope differs: missing={missing}, unexpected={unexpected}")
    return aggregate_counts(by_source)


def aggregate_counts(by_source):
    lines, branches, calls = Counter(), Counter(), Counter()
    for name, profiles in by_source.items():
        for unit, entry in profiles:
            # Branch indices belong to one object. Keep repeated sources' counters separate.
            suffix = (unit,) if len(profiles) > 1 else ()
            for function in entry["functions"]:
                calls[function["name"]] += function["execution_count"]
            for line in entry["lines"]:
                key = (name, line["line_number"])
                lines[(*key, *suffix)] += line["count"]
                for index, branch in enumerate(line["branches"]):
                    branches[(*key, index, *suffix)] += branch["count"]
    return lines, branches, calls


def public_api():
    declared = set()
    for header in (ROOT / "include").rglob("*.h"):
        code = re.sub(r"/\*.*?\*/|//[^\n]*", "", header.read_text(encoding="utf-8"), flags=re.S)
        declared.update(re.findall(r"\b(tlv_\w+)\s*\(", code))
    mapping = dict(re.findall(r"^\| `(tlv_\w+)` \| `([^`]+)` \|$",
                              (ROOT / "docs/coverage.md").read_text(encoding="utf-8"), re.M))
    if not declared:
        raise SystemExit("No public function declarations found")
    return declared, mapping


def fraction(hit, total):
    return f"{hit}/{total} ({100 * hit / total:.2f}%)" if total else "0/0 (n/a)"


def report(destination, version, lines, branches, calls, passed):
    declared, mapping = public_api()
    output = [f"GCC/gcov {version}", "File | Lines hit | Branch outcomes taken"]
    for name in sorted({key[0] for key in lines}):
        file_lines = [v for k, v in lines.items() if k[0] == name]
        file_branches = [v for k, v in branches.items() if k[0] == name]
        output.append(f"{name} | {fraction(sum(v > 0 for v in file_lines), len(file_lines))} | "
                      f"{fraction(sum(v > 0 for v in file_branches), len(file_branches))}")
    errors = []
    for title, counts, excluded in (("Lines", lines, EXCLUDED_LINES),
                                    ("Branches", branches, EXCLUDED_BRANCHES)):
        missed = {key for key, count in counts.items() if not count}
        output.append(f"{title} total: {fraction(len(counts) - len(missed), len(counts))}")
        reachable = counts.keys() - excluded
        output.append(f"{title} after reviewed exclusions: "
                      f"{fraction(len(reachable - missed), len(reachable))}")
        for key in sorted(missed):
            status = "reviewed exclusion" if key in excluded else "UNCOVERED"
            output.append(f"  {':'.join(map(str, key))} {status}")
        if missed - excluded:
            errors.append(f"{title}: coverage below 100% after reviewed exclusions")
        if excluded - missed:
            errors.append(f"{title}: review stale exclusions {sorted(excluded - missed)}")

    covered = 0
    for name in sorted(declared):
        case = mapping.get(name)
        if calls[name] and case in passed:
            covered += 1
        else:
            errors.append(f"Missing API execution or passing behavioral test: {name} -> {case or '(unmapped)'}")
    if mapping.keys() - declared:
        errors.append(f"Stale public API mappings: {sorted(mapping.keys() - declared)}")
    output.append(f"Public API with passing behavioral tests: {fraction(covered, len(declared))}")
    text = "\n".join(output + errors) + "\n"
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(text.encode("utf-8"))
    print(text, end="")
    if errors:
        raise SystemExit("Coverage gate failed")


def check_coverage(build, destination):
    cc = os.environ.get("CC", "gcc")
    cxx = os.environ.get("CXX", "g++")
    gcov = os.environ.get("GCOV", "gcov")
    version = subprocess.check_output([cc, "-dumpfullversion"], text=True).strip()
    banner = subprocess.check_output([gcov, "--version"], text=True).splitlines()[0]
    if (subprocess.check_output([cxx, "-dumpfullversion"], text=True).strip() != version
            or banner.split()[-1] != version):
        raise SystemExit(f"Use matching GCC, G++ and gcov versions (C compiler {version}, {banner})")
    configure = ["cmake", "-S", ROOT, "-B", build, "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Debug",
                 "-DBUILD_TESTING=ON", "-DOPENTLV_TEST_COVERAGE=ON",
                 f"-DCMAKE_C_COMPILER={cc}", f"-DCMAKE_CXX_COMPILER={cxx}"]
    if archive := os.environ.get("OPENTLV_GOOGLETEST_URL"):
        configure.append(f"-DOPENTLV_GOOGLETEST_URL={archive}")
    subprocess.run(configure, check=True)
    subprocess.run(["cmake", "--build", build], check=True)
    passed = run_ctest(build, "Debug", build / "reports")
    report(destination, version, *read_profiles(build, gcov, version), passed)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("report", type=Path, help="write the coverage report to this file")
    args = parser.parse_args()
    with tempfile.TemporaryDirectory() as directory:
        check_coverage(Path(directory), args.report)
