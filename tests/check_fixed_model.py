"""Build and run the fixed TLV model tests with C99 and C++11 compilers."""

import os
import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def check(build):
    cc = os.environ.get("CC", "cc")
    cxx = os.environ.get("CXX", "c++")
    flags = ["-pedantic-errors", "-Wall", "-Wextra", "-Werror", "-I", ROOT / "include"]
    objects = []
    for name in ("size", "value", "fixed"):
        source = ROOT / "src" / "core" / (name + ".c")
        obj = build / (name + ".o")
        subprocess.run([cc, "-std=c99", *flags, "-c", source, "-o", obj], cwd=build, check=True)
        objects.append(obj)

    consumers = [(cc, "c99", "fixed_model.c"), (cxx, "c++11", "fixed_model.cpp")]
    for compiler, standard, source in consumers:
        executable = build / ("test.exe" if os.name == "nt" else "test")
        subprocess.run(
            [compiler, "-std=" + standard, *flags, ROOT / "tests" / source, *objects, "-o", executable],
            cwd=build, check=True,
        )
        subprocess.run([executable], cwd=build, check=True)
        print(f"PASS {standard} {source}")


if __name__ == "__main__":
    with tempfile.TemporaryDirectory() as directory:
        check(Path(directory))
