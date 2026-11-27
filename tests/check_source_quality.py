"""Check formatting and static analysis of first-party C99 and C++17 files."""

import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[1]
SOURCE_DIRS = ("include", "src", "examples", "tests", "benchmarks")
EXCLUDED_DIRS = {"CMakeFiles", "_deps", "build", "generated", "third_party", "vendor"}
SUFFIXES = {".c", ".h", ".cc", ".cpp", ".cxx", ".hh", ".hpp", ".hxx"}
TOOLS = {
    "format": ("clang-format", "CLANG_FORMAT", "18.1.8"),
    "tidy": ("clang-tidy", "CLANG_TIDY", "18.1.8"),
    "cppcheck": ("cppcheck", "CPPCHECK", "2.13.0"),
}


def source_files():
    files = set()
    for source_dir in SOURCE_DIRS:
        for directory, children, names in os.walk(ROOT / source_dir):
            if "CMakeCache.txt" in names:
                children.clear()
                continue
            children[:] = [name for name in children if name not in EXCLUDED_DIRS]
            for name in names:
                path = Path(directory, name)
                if path.suffix in SUFFIXES and not path.is_symlink():
                    files.add(path.relative_to(ROOT))
    return files


def tool_path(check):
    name, variable, version = TOOLS[check]
    program = os.environ.get(variable, name)
    executable = shutil.which(program)
    if not executable:
        raise ValueError(f"Cannot find {program}. Install {name} {version} or set {variable}.")
    output = subprocess.check_output([executable, "--version"], text=True).strip()
    if not re.search(rf"\b{re.escape(version)}(?![\d.])", output):
        raise ValueError(f"Expected {name} {version}, found {output}")
    print(f"Checking with {name} {version}", flush=True)
    return executable


def analysis_inputs(files):
    groups = {"c": set(), "c++": set()}
    for path in files:
        if path.parent.parent == Path("tests/baselines") and path.name in ("core.h", "io.h"):
            # These X-macro lists require the consumer's types and macro definition.
            groups["c++"].add(path.with_name("abi_layout.cpp"))
        elif path.suffix == ".c":
            groups["c"].add(path)
        elif path.suffix == ".h" and path != Path("tests/io/expect_view.h"):
            groups["c"].add(path)
            if path.parts[0] == "include" or path == Path("benchmarks/adapter.h"):
                groups["c++"].add(path)
        else:
            groups["c++"].add(path)
    return groups


def run_check(check, files, clang_args):
    executable = tool_path(check)
    if check == "format":
        subprocess.run([executable, "--dry-run", "--Werror", *sorted(files)], cwd=ROOT, check=True)
        return

    for language, paths in analysis_inputs(files).items():
        if not paths:
            continue
        standard = "c99" if language == "c" else "c++17"
        if check == "tidy":
            flags = ["-x", language, f"-std={standard}", "-Iinclude", *clang_args]
            command = [executable, "--quiet", f"--config-file={ROOT / '.clang-tidy'}",
                       *sorted(paths), "--", *flags]
        else:
            # Cppcheck normally infers __cplusplus from the file extension. Select the
            # same C++17 branches when a dual-language .h file is the primary input.
            model = ["-D__cplusplus=201703L", "--library=googletest"] if language == "c++" else []
            # The fixture takes an owned snapshot outside the measured benchmark interval.
            suppression = "--suppress=passedByValue:benchmarks/fixtures.cpp:129"
            command = [executable, "--quiet", "--error-exitcode=1", "--enable=warning,performance,portability",
                       "--platform=unix64", f"--language={language}", f"--std={standard}", "-Iinclude",
                       *model, suppression, *sorted(paths)]
        subprocess.run(command, cwd=ROOT, check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", choices=TOOLS, help="run just this check (default: all)")
    parser.add_argument("--system-include", action="append", type=Path, default=[], metavar="DIR",
                        help="additional system header directory for clang-tidy (repeatable)")
    parser.add_argument("--clang-arg", action="append", default=[], metavar="ARG",
                        help="additional compiler flag for clang-tidy, e.g. --clang-arg=--target=...")
    parser.add_argument("files", type=Path, nargs="*", help="restrict checks to these first-party files")
    args = parser.parse_args()
    files = source_files()
    if args.files:
        requested = {path.resolve().relative_to(ROOT) for path in args.files}
        unknown = requested - files
        if unknown:
            parser.error(f"file outside first-party scope: {min(unknown)}")
        files = requested
    if not files:
        parser.error("no first-party C or C++ files found")
    clang_args = [f"-isystem{path.resolve()}" for path in args.system_include] + args.clang_arg
    for check in ([args.check] if args.check else TOOLS):
        run_check(check, files, clang_args)


if __name__ == "__main__":
    try:
        main()
    except (ValueError, OSError) as error:
        sys.exit(str(error))
    except subprocess.CalledProcessError as error:
        sys.exit(error.returncode)
