"""Build and run the fixed TLV and diagnostics tests with C99 and C++11 compilers."""

import argparse
import os
import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def check(build, release=False):
    cc = os.environ.get("CC", "cc")
    cxx = os.environ.get("CXX", "c++")
    flags = ["-pedantic-errors", "-Wall", "-Wextra", "-Werror", "-I", ROOT / "include"]
    if release:
        flags.extend(["-O2", "-DNDEBUG"])

    def compile_c(relative):
        source = ROOT / "src" / relative
        obj = (build / "objects" / relative).with_suffix(".o")
        obj.parent.mkdir(parents=True, exist_ok=True)
        subprocess.run([cc, "-std=c99", *flags, "-c", source, "-o", obj], cwd=build, check=True)
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
            [compiler, "-std=" + standard, *flags, ROOT / "tests" / source, *objects, "-o", executable],
            cwd=build, check=True,
        )
        subprocess.run([executable], cwd=build, check=True)
        print(f"PASS {standard} {source}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--release", action="store_true", help="compile with -O2 and -DNDEBUG")
    args = parser.parse_args()
    with tempfile.TemporaryDirectory() as directory:
        check(Path(directory), release=args.release)
