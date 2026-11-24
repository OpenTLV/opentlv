"""Build and run the fixed TLV and diagnostics tests with C99 and C++11 compilers."""

import argparse
import os
import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def write_coverage(build, objects, destination):
    profiles = [obj.with_suffix(".gcda") for obj in objects]
    for profile in profiles:
        if not profile.is_file():
            raise SystemExit(f"Missing execution counts for {profile.name}")

    gcov = os.environ.get("GCOV", "gcov")
    summary = subprocess.run(
        [gcov, "--no-output", "--branch-probabilities", "--relative-only",
         "--source-prefix", ROOT.as_posix(), *profiles],
        cwd=build, env={**os.environ, "LC_ALL": "C"}, check=True,
        stdout=subprocess.PIPE, text=True,
    ).stdout
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(summary.encode("utf-8"))
    print(summary, end="")


def check(build, release=False, sanitize=False, coverage=None):
    cc = os.environ.get("CC", "cc")
    cxx = os.environ.get("CXX", "c++")
    flags = ["-pedantic-errors", "-Wall", "-Wextra", "-Werror", "-I", ROOT / "include"]
    if release:
        flags.extend(["-O2", "-DNDEBUG"])
    if sanitize:
        flags.extend(["-O1", "-g", "-fno-omit-frame-pointer",
                      "-fsanitize=address,undefined", "-fno-sanitize-recover=all"])
    # Only library objects contribute coverage. Test executables link the runtime.
    library_flags = ["--coverage", "-O0"] if coverage else []
    link_flags = ["-lgcov"] if coverage else []

    def compile_c(relative):
        source = ROOT / "src" / relative
        obj = (build / "objects" / relative).with_suffix(".o")
        obj.parent.mkdir(parents=True, exist_ok=True)
        subprocess.run([cc, "-std=c99", *flags, *library_flags, "-c", source, "-o", obj],
                       cwd=build, check=True)
        return obj

    core = [compile_c(name) for name in ("core/size.c", "core/value.c", "core/fixed.c", "core/result.c")]
    reader = [*core, compile_c("io/reader/reader.c")]
    reader_and_writer = [*reader, compile_c("io/writer/writer.c")]
    consumers = [
        (cc, "c99", "fixed_model.c", core),
        (cc, "c99", "fixed_reader.c", reader),
        (cc, "c99", "fixed_writer.c", reader_and_writer),
        (cc, "c99", "fixed_diagnostics.c", reader_and_writer),
        (cxx, "c++11", "fixed_model.cpp", reader_and_writer),
    ]
    for compiler, standard, source, objects in consumers:
        executable = build / ("test.exe" if os.name == "nt" else "test")
        subprocess.run(
            [compiler, "-std=" + standard, *flags, ROOT / "tests" / source,
             *objects, *link_flags, "-o", executable],
            cwd=build, check=True,
        )
        subprocess.run([executable], cwd=build, check=True)
        print(f"PASS {standard} {source}")
    if coverage:
        write_coverage(build, reader_and_writer, coverage)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    modes = parser.add_mutually_exclusive_group()
    modes.add_argument("--release", action="store_true", help="compile with -O2 and -DNDEBUG")
    modes.add_argument("--sanitize", action="store_true",
                       help="run with AddressSanitizer and UndefinedBehaviorSanitizer")
    modes.add_argument("--coverage", type=Path, metavar="FILE",
                       help="write a GCC/gcov coverage summary to FILE")
    args = parser.parse_args()
    with tempfile.TemporaryDirectory() as directory:
        check(Path(directory), release=args.release, sanitize=args.sanitize, coverage=args.coverage)
