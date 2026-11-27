"""Run the same Fixed TLV contributor checks used by GitHub Actions."""

import argparse
import os
from pathlib import Path
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_WORK = ROOT / "build" / "quality"


def run(*command):
    subprocess.run(command, cwd=ROOT, check=True)


def configure(build, generator, benchmarks=False):
    options = ["-DBUILD_TESTING=ON", "-DCMAKE_C_COMPILER=gcc-13", "-DCMAKE_CXX_COMPILER=g++-13"]
    archives = ["OPENTLV_GOOGLETEST_URL"]
    if benchmarks:
        options.append("-DOPENTLV_BUILD_BENCHMARKS=ON")
        archives.append("OPENTLV_GOOGLE_BENCHMARK_URL")
    for name in archives:
        if os.environ.get(name):
            options.append(f"-D{name}={os.environ[name]}")
    run("cmake", "-S", ROOT, "-B", build, "-G", generator, *options)


def contracts(work):
    build = work / "opentlv-tests"
    configure(build, "Ninja Multi-Config")
    for config in ("Debug", "Release"):
        run("cmake", "--build", build, "--config", config)
        run(sys.executable, "-B", "tests/run_ctest.py", build, config,
            work / "ctest-reports" / config, "--require-x64")


def analysis(work):
    build = work / "quality-headers"
    configure(build, "Ninja", benchmarks=True)
    run(sys.executable, "-B", "tests/check_source_quality.py",
        "--clang-arg=--gcc-install-dir=/usr/lib/gcc/x86_64-linux-gnu/13",
        "--system-include", build / "_deps/googletest-src/googletest/include",
        "--system-include", build / "_deps/googlebenchmark-src/include")
    run(sys.executable, "-B", "tests/test_source_quality.py")


def compare_performance(reports, output, candidate):
    pin = ROOT / "benchmarks/performance-baseline.txt"
    try:
        baseline = pin.read_text(encoding="ascii").strip()
    except (OSError, UnicodeDecodeError) as error:
        # An empty revision lets the comparator publish an invalid report for CI.
        print(error, file=sys.stderr)
        baseline = ""
    if candidate is None:
        candidate = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip()
    run(sys.executable, "-B", "benchmarks/compare_performance.py",
        "--baseline", reports / "baseline", f"--baseline-revision={baseline}",
        "--candidate", reports / "candidate", f"--candidate-revision={candidate}",
        "--output", output)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("format", help="check first-party C and C++ formatting")
    for name, description in (("contracts", "build and test GCC 13 x64 Debug and Release"),
                              ("analysis", "check source quality and run its negative controls")):
        subparser = commands.add_parser(name, help=description)
        subparser.add_argument("--work-dir", type=Path, default=DEFAULT_WORK,
                               help="directory for builds and reports (default: build/quality)")
    compare = commands.add_parser("compare-performance", help="compare collected baseline and candidate reports")
    compare.add_argument("--reports", type=Path, required=True, help="directory containing baseline/ and candidate/")
    compare.add_argument("--output", type=Path, required=True, help="destination for comparison.json and summary.md")
    compare.add_argument("--candidate-revision", metavar="SHA", help="expected candidate commit (default: Git HEAD)")
    args = parser.parse_args()
    if args.command == "format":
        run(sys.executable, "-B", "tests/check_source_quality.py", "--check", "format")
    elif args.command == "contracts":
        contracts(args.work_dir.resolve())
    elif args.command == "analysis":
        analysis(args.work_dir.resolve())
    else:
        compare_performance(args.reports.resolve(), args.output.resolve(), args.candidate_revision)


if __name__ == "__main__":
    try:
        main()
    except OSError as error:
        sys.exit(str(error))
    except subprocess.CalledProcessError as error:
        sys.exit(error.returncode)
