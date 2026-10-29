#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Marek Cingel
"""Build external consumers through FetchContent and a relocated Core installation."""

import argparse
from pathlib import Path
import re
import shutil
import subprocess
import tempfile


def run(*args):
    print("+ " + " ".join(map(str, args)), flush=True)
    subprocess.run(list(map(str, args)), check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-root", type=Path, default=Path("build/cmake-integration"))
    parser.add_argument("--generator")
    parser.add_argument("--shared", choices=("ON", "OFF"), default="ON")
    parser.add_argument("--cxx", choices=("ON", "OFF"), default="ON")
    parser.add_argument("--cmake-arg", action="append", default=[])
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    args.build_root.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix="consumer-", dir=args.build_root.resolve()))
    print(f"CMake consumer evidence: {work}", flush=True)
    common = ["-DCMAKE_BUILD_TYPE=Release"]
    if args.generator:
        common += ["-G", args.generator]
    common += args.cmake_arg
    core_options = [f"-DOPENTLV_BUILD_SHARED_LIBS={args.shared}",
                    f"-DOPENTLV_BUILD_CXX={args.cxx}"]

    def configure(source, build, *options):
        run("cmake", "-S", source, "-B", build, *common, *options)

    def build_and_test(build):
        run("cmake", "--build", build, "--config", "Release", "--parallel", "1")
        run("ctest", "--test-dir", build, "-C", "Release",
            "--output-on-failure", "--no-tests=error")

    # A standalone build supplies the reference metadata and installed package.
    core_build = work / "core"
    configure(root, core_build, *core_options, "-DOPENTLV_BUILD_TESTS=OFF",
              "-DOPENTLV_BUILD_EXAMPLES=OFF",
              "-DCMAKE_CXX_COMPILER=/opentlv-no-cxx-compiler",
              "-DCMAKE_INSTALL_LIBDIR=lib", "-DCMAKE_INSTALL_INCLUDEDIR=include")
    if list((core_build / "CMakeFiles").glob("*/CMakeCXXCompiler.cmake")):
        raise RuntimeError("Library/facade-only configuration enabled a C++ compiler")
    cache = (core_build / "CMakeCache.txt").read_text(encoding="utf-8")
    if re.search(r"^(OPENTLV_BUILD_(?:CLI(?:_TESTS)?|PYTHON)|nlohmann_json_[^:]*):", cache, re.M):
        raise RuntimeError("Core still configures migrated consumer options or dependencies")
    run("cmake", "--build", core_build, "--config", "Release", "--parallel", "1")
    reference = (core_build / "generated/include/tlv/version.h").read_bytes()
    version = re.search(rb'#define OPENTLV_VERSION_STRING "([^"]+)"', reference)[1].decode()
    numeric_version = re.search(rb'#define OPENTLV_VERSION_MAJOR (\d+)', reference)[1].decode()
    for component in ("MINOR", "PATCH"):
        numeric_version += "." + re.search(
            rb'#define OPENTLV_VERSION_' + component.encode() + rb' (\d+)', reference)[1].decode()
    prefix = work / "installed"
    run("cmake", "--install", core_build, "--config", "Release", "--prefix", prefix)
    relocated = work / "relocated installation"
    shutil.copytree(prefix, relocated)
    missing = work / "missing-component"
    missing.mkdir()
    (missing / "CMakeLists.txt").write_text(
        "cmake_minimum_required(VERSION 3.16)\nproject(missing LANGUAGES NONE)\n"
        "find_package(OpenTLV REQUIRED CONFIG COMPONENTS unavailable)\n", encoding="utf-8")
    result = subprocess.run(["cmake", "-S", str(missing), "-B", str(missing / "build"),
                             f"-DCMAKE_PREFIX_PATH={relocated.as_posix()}"],
                            capture_output=True, text=True)
    if result.returncode == 0 or "OpenTLV_FOUND" not in result.stdout + result.stderr:
        raise RuntimeError("Missing required component did not reject the installed package")
    for path in relocated.rglob("*.cmake"):
        content = path.read_text(encoding="utf-8").replace("\\", "/")
        for private_path in (root, core_build, prefix):
            if private_path.as_posix() in content:
                raise RuntimeError(f"Non-relocatable package: {path}")

    consumer = work / "application"
    shutil.copytree(root / "tests/cmake-consumer", consumer)
    # An empty application repository prevents accidental use of Core's Git
    # history via a parent directory. No commits or tags are created.
    run("git", "init", "--quiet", consumer)
    consumer_options = [f"-DCONSUMER_CXX={args.cxx}",
                        f"-DEXPECTED_VERSION={numeric_version}",
                        f"-DEXPECTED_PREFIX={relocated.as_posix()}"]
    # Runtime version can include a prerelease suffix; package matching uses the
    # numeric CMake project version, consistent with the existing package policy.
    consumer_options.append(f"-DEXPECTED_RUNTIME_VERSION={version}")
    modes = ["package", "fetch"]
    cmake_version = subprocess.check_output(["cmake", "--version"], text=True)
    if tuple(map(int, re.search(r"version (\d+)\.(\d+)", cmake_version).groups())) >= (3, 24):
        modes.append("fetch-package")
    for mode in modes:
        build = work / mode
        options = [*consumer_options, f"-DCONSUMER_MODE={mode}",
                   f"-DCMAKE_PREFIX_PATH={relocated.as_posix()}",
                   "-DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF",
                   "-DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=OFF"]
        if mode == "fetch":
            options += core_options + [f"-DFETCHCONTENT_SOURCE_DIR_OPENTLV_CORE={root.as_posix()}"]
        configure(consumer, build, *options)
        if mode == "fetch":
            generated = build / "_deps/opentlv_core-build/generated/include/tlv/version.h"
            if generated.read_bytes() != reference:
                raise RuntimeError("Embedded Core version differs from standalone metadata")
        build_and_test(build)
        print(f"PASS {mode}: shared={args.shared}, cxx={args.cxx}", flush=True)


if __name__ == "__main__":
    main()
