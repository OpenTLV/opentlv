#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Marek Cingel
"""Check optional source installation and direct C99/C++17 compiler consumers.

The same samples also run with the documented optional dead-stripping flags and
against a relocated conventional static/shared install.
CMake prepares the distributions and conventional comparison only; standalone
consumers use compiler commands and require no OpenTLV libraries or CMake package.
"""

import argparse
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]


def run(*args, cwd=None, env=None, fails=False):
    print("+ " + " ".join(map(str, args)), flush=True)
    result = subprocess.run(list(map(str, args)), cwd=cwd, env=env,
                            capture_output=True, text=True)
    if (result.returncode == 0) == fails:
        raise RuntimeError(f"Unexpected exit {result.returncode}:\n{result.stdout}{result.stderr}")
    return result.stdout + result.stderr


def inventory(directory):
    return {path.relative_to(directory).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest()
            for path in directory.rglob("*") if path.is_file()}


def compile_object(compiler, source, output, includes=(), *, cxx=False, msvc=False, extra=()):
    if msvc:
        flags = ["/nologo", "/WX", "/MD", "/c", "/std:c++17" if cxx else "/std:c11"]
        flags += ["/EHsc", "/TP"] if cxx else ["/TC"]
        flags += [f"/I{path}" for path in includes]
        run(compiler, *flags, *extra, source, f"/Fo{output}", cwd=output.parent)
    else:
        flags = ["-std=c++17" if cxx else "-std=c99", "-pedantic-errors", "-Werror"]
        flags += [f"-I{path}" for path in includes]
        run(compiler, *flags, *extra, "-c", source, "-o", output, cwd=output.parent)


# The optional dead-stripping flags documented in the sample README:
# (compile flags, link flags) for MSVC and for GCC/Clang with GNU-compatible linkers.
DEAD_STRIPPING = {True: (["/Gy", "/Gw"], ["/link", "/OPT:REF", "/OPT:ICF"]),
                  False: (["-ffunction-sections", "-fdata-sections"], ["-Wl,--gc-sections"])}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--shared", choices=("ON", "OFF"), default="OFF")
    parser.add_argument("--cc", default=os.environ.get("CC", "cl" if os.name == "nt" else "cc"))
    parser.add_argument("--cxx", default=os.environ.get("CXX", "cl" if os.name == "nt" else "c++"))
    parser.add_argument("--generator", default="Ninja")
    parser.add_argument("--query-frontend", choices=("ON", "OFF"), default="ON")
    parser.add_argument("--query-set-operations", choices=("ON", "OFF"), default="ON")
    parser.add_argument("--data-dir", default="share")
    parser.add_argument("--build-root", type=Path, default=ROOT / "build/amalgamation-consumers")
    args = parser.parse_args()
    cc = shutil.which(args.cc)
    cxx = shutil.which(args.cxx)
    if not cc or not cxx:
        parser.error("Both compiler drivers must be on PATH (use a Developer shell for MSVC)")
    msvc = Path(cc).stem.lower() == "cl"
    args.build_root.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix=f"{args.shared.lower()}-", dir=args.build_root.resolve()))
    print(f"Evidence directory: {work}", flush=True)
    build = work / "library"
    prefix = work / "installed sources"
    common = ["-G", args.generator, f"-DCMAKE_C_COMPILER={cc}", "-DCMAKE_BUILD_TYPE=Release"]
    run("cmake", "-S", ROOT, "-B", build, *common,
        "-DOPENTLV_BUILD_TESTS=OFF", "-DOPENTLV_BUILD_EXAMPLES=OFF", "-DOPENTLV_BUILD_CXX=OFF",
        "-DCMAKE_CXX_COMPILER=/opentlv-no-cxx-compiler",
        f"-DOPENTLV_BUILD_SHARED_LIBS={args.shared}", f"-DCMAKE_INSTALL_DATADIR={args.data_dir}",
        f"-DOPENTLV_QUERY_FRONTEND={args.query_frontend}",
        f"-DOPENTLV_QUERY_SET_OPERATIONS={args.query_set_operations}")
    if list((build / "CMakeFiles").glob("*/CMakeCXXCompiler.cmake")):
        raise RuntimeError("C-only distribution preparation enabled C++")
    install = ["cmake", "--install", build, "--config", "Release", "--component", "Amalgamation",
               "--prefix", prefix]
    failure = run(*install, fails=True)
    if "--target opentlv_amalgamation" not in failure or inventory(prefix):
        raise RuntimeError("Missing outputs must fail before installing and explain how to generate")
    generated = build / "amalgamation"
    if generated.exists():
        raise RuntimeError("Configuration or failed install generated the amalgamation")
    run("cmake", "--build", build, "--config", "Release", "--target", "opentlv_amalgamation")
    before = inventory(generated)
    # An incomplete output directory must also fail, never trigger regeneration.
    metadata = generated / "opentlv-amalgamation.json"
    saved = metadata.read_bytes()
    metadata.unlink()
    try:
        run(*install, fails=True)
        if inventory(prefix) or metadata.exists():
            raise RuntimeError("Incomplete artifacts were installed or regenerated")
    finally:
        metadata.write_bytes(saved)
    run(*install)
    if before != inventory(generated):
        raise RuntimeError("Installation changed generated outputs")
    # Source installation needs only the generator target, no library build.
    # Temporarily remove the C output to detect implicit generation by ALL or
    # ordinary installation without relying on directory renames on Windows.
    generated_source = generated / "opentlv.c"
    saved_source = generated_source.read_bytes()
    generated_source.unlink()
    conventional = work / "conventional"
    try:
        run("cmake", "--build", build, "--config", "Release", "--parallel", "2")
        run("cmake", "--install", build, "--config", "Release", "--prefix", conventional)
        if generated_source.exists() or list(conventional.rglob("opentlv-amalgamation.json")):
            raise RuntimeError("Default build/install generated or installed the amalgamation")
    finally:
        generated_source.write_bytes(saved_source)
    # Even after generation a full installation must exclude this component.
    run("cmake", "--install", build, "--config", "Release", "--prefix", conventional)
    if list(conventional.rglob("opentlv-amalgamation.json")):
        raise RuntimeError("Full installation included the optional component")

    artifact = prefix / args.data_dir / "OpenTLV/amalgamation"
    expected = dict(before)
    for name in ("LICENSE", "LICENSES/MIT.txt", "THIRD_PARTY.md"):
        expected[name] = hashlib.sha256((ROOT / name).read_bytes()).hexdigest()
    for name in ("consumer.c", "consumer.cpp", "README.md"):
        expected[f"examples/{name}"] = hashlib.sha256(
            (ROOT / "tests/amalgamation/consumer" / name).read_bytes()).hexdigest()
    if inventory(artifact) != expected or len(inventory(prefix)) != len(expected):
        raise RuntimeError("Component installed missing, changed or extra files")
    if os.name != "nt":
        staged = work / "destdir"
        env = dict(os.environ, DESTDIR=str(staged))
        run(*install[:-1], "/opentlv-source-component", env=env)
        if inventory(staged / "opentlv-source-component" / args.data_dir /
                     "OpenTLV/amalgamation") != expected:
            raise RuntimeError("DESTDIR installation differs from the prefix installation")
    for path in artifact.rglob("*"):
        if path.is_file():
            data = path.read_bytes()
            for private in (ROOT, work):
                if str(private).encode() in data or private.as_posix().encode() in data:
                    raise RuntimeError(f"Private path in installed artifact: {path}")
    # All checked-in headers, including every C++ facade, remain unmodified.
    for header in (ROOT / "include").rglob("*"):
        if header.is_file():
            installed = artifact / "include" / header.relative_to(ROOT / "include")
            if installed.read_text(encoding="utf-8") != header.read_text(encoding="utf-8"):
                raise RuntimeError(f"Public header changed: {header}")

    relocated = work / "relocated sources"
    shutil.move(str(prefix), relocated)
    artifact = relocated / args.data_dir / "OpenTLV/amalgamation"
    # The C-only directory has exactly the pair and sample, no module headers.
    c_only = work / "C only"
    c_only.mkdir()
    for name in ("opentlv.c", "opentlv.h"):
        shutil.copyfile(artifact / name, c_only / name)
    shutil.copyfile(artifact / "examples/consumer.c", c_only / "consumer.c")
    outputs = {}
    sizes = {}
    # Build each sample as documented, then again with optional dead stripping. The
    # stripped programs must behave identically and be smaller, without fixed sizes.
    for strip in (False, True):
        compile_flags, link_flags = DEAD_STRIPPING[msvc] if strip else ([], [])
        suffix = "-stripped" if strip else ""
        implementation = c_only / f"opentlv{suffix}.obj"
        compile_object(cc, c_only / "opentlv.c", implementation, msvc=msvc, extra=compile_flags)
        for language, compiler in (("c", cc), ("cpp", cxx)):
            consumer = work / f"standalone-{language}{suffix}"
            consumer.mkdir()
            source = c_only / "consumer.c" if language == "c" else artifact / "examples/consumer.cpp"
            includes = [] if language == "c" else [artifact / "include"]
            obj = consumer / "consumer.obj"
            compile_object(compiler, source, obj, includes, cxx=language == "cpp", msvc=msvc,
                           extra=compile_flags)
            binary = consumer / ("consumer.exe" if os.name == "nt" else "consumer")
            flags = ["/nologo", f"/Fe{binary}"] if msvc else ["-o", binary]
            run(compiler, obj, implementation, *flags, *link_flags, cwd=consumer)
            output = run(binary)
            if strip:
                if output != outputs[language]:
                    raise RuntimeError(f"Dead-stripped {language} consumer output differs")
                if binary.stat().st_size >= sizes[language]:
                    raise RuntimeError(f"Dead stripping did not shrink the {language} consumer: "
                                       f"{binary.stat().st_size} >= {sizes[language]} bytes")
                print(f"{language} consumer: {sizes[language]} -> {binary.stat().st_size} bytes "
                      "with dead stripping", flush=True)
            else:
                outputs[language] = output
                sizes[language] = binary.stat().st_size

    relocated_libs = work / "relocated conventional"
    shutil.move(str(conventional), relocated_libs)
    comparison = work / "comparison"
    run("cmake", "-S", ROOT / "tests/amalgamation/conventional", "-B", comparison, *common,
        f"-DCMAKE_CXX_COMPILER={cxx}", f"-DCMAKE_PREFIX_PATH={relocated_libs.as_posix()}",
        f"-DSAMPLES={(artifact / 'examples').as_posix()}")
    run("cmake", "--build", comparison, "--config", "Release", "--parallel", "2")
    env = dict(os.environ)
    if os.name == "nt":
        dll_dirs = sorted({str(path.parent) for path in relocated_libs.rglob("*.dll")})
        env["PATH"] = os.pathsep.join([*dll_dirs, env.get("PATH", "")])
    for language in outputs:
        binary = comparison / "bin" / f"consumer-{language}{'.exe' if os.name == 'nt' else ''}"
        if run(binary, env=env) != outputs[language]:
            raise RuntimeError(f"Conventional and amalgamated {language} output differs")
    print(f"Amalgamation install, C99/C++17, dead stripping and shared={args.shared} "
          "comparisons passed")


if __name__ == "__main__":
    main()
