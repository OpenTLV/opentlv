# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Marek Cingel
"""Collect the reference Linux Fixed TLV measurements for one Git checkout."""

import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

# Keep this checkout clean when importing the CMake File API reader.
sys.dont_write_bytecode = True
from performance_build import TARGETS, build_signature, request_codemodel


RELEASE_FLAGS = "-O2 -DNDEBUG -march=x86-64 -mtune=generic"
PROFILE_FLAGS = "-O2 -g -DNDEBUG -march=x86-64 -mtune=generic"
PROTOCOL_FILES = (
    "benchmarks/adapter.c", "benchmarks/adapter.cpp", "benchmarks/adapter.h",
    "benchmarks/callgrind.cpp", "benchmarks/fixtures.cpp", "benchmarks/fixtures.hpp",
    "benchmarks/timing.hpp", "benchmarks/check_fixtures.cpp", "benchmarks/google_benchmark.cpp",
    "benchmarks/run_callgrind.py", "benchmarks/footprint/report.py",
    "examples/write.c", "examples/write.cpp",
)


def build_targets(source, tree, config, flags, options, targets, profile):
    request_codemodel(tree)
    # A relative build RPATH keeps absolute build path lengths out of shared ELF sizes.
    subprocess.run(["cmake", "-S", source, "-B", tree, "-G", "Ninja",
                    "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON", "-DCMAKE_INTERPROCEDURAL_OPTIMIZATION=OFF",
                    "-DCMAKE_BUILD_RPATH_USE_ORIGIN=ON", f"-DCMAKE_BUILD_TYPE={config}",
                    f"-DCMAKE_C_FLAGS_{config.upper()}={flags}",
                    f"-DCMAKE_CXX_FLAGS_{config.upper()}={flags}", *options], check=True)
    subprocess.run(["cmake", "--build", tree, "--target", *targets], check=True)
    return build_signature(tree, source, TARGETS[profile])


def record_build(tree, config, directory, metadata_subdir=""):
    destination = directory / metadata_subdir
    destination.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(tree / "benchmarks" / config / "metadata.json", destination / "metadata.json")
    shutil.copyfile(tree / "compile_commands.json", directory / "compile_commands.json")


def record_catalog(probe, reports):
    with (reports / "workloads.txt").open("wb") as catalog:
        subprocess.run([probe, "--list"], stdout=catalog, check=True)


def collect(source, build, reports):
    for directory in (build, reports):
        if directory == source or source in directory.parents or directory.exists():
            raise ValueError(f"{directory}: expected a new directory outside the source checkout")
    subprocess.run(["git", "-C", source, "rev-parse", "--verify", "HEAD"], check=True,
                   stdout=subprocess.DEVNULL)
    changes = subprocess.check_output(["git", "--no-optional-locks", "-C", source, "status",
                                       "--porcelain=v1", "--untracked-files=normal"])
    if changes:
        raise ValueError(f"{source}: performance evidence requires a clean Git checkout")
    build.mkdir(parents=True)
    reports.mkdir(parents=True)
    os.environ.update(LC_ALL="C", PYTHONDONTWRITEBYTECODE="1", CC="gcc-13", CXX="g++-13")
    for compiler in (os.environ["CC"], os.environ["CXX"]):
        actual = subprocess.check_output([compiler, "-dumpfullversion"], text=True).strip()
        if actual != "13.3.0":
            raise ValueError(f"expected {compiler} 13.3.0, found {actual}")

    tree = build / "callgrind"
    probe = tree / "opentlv_callgrind"
    settings = {}
    settings["callgrind"] = build_targets(
        source, tree, "RelWithDebInfo", PROFILE_FLAGS,
        ["-DBUILD_SHARED_LIBS=OFF", "-DOPENTLV_BUILD_CALLGRIND=ON"], ["opentlv_callgrind"], "callgrind")
    callgrind = reports / "callgrind"
    record_build(tree, "RelWithDebInfo", callgrind / "callgrind-build", "benchmarks/RelWithDebInfo")
    profiles = callgrind / "callgrind-reports"
    subprocess.run([sys.executable, "-B", source / "benchmarks/run_callgrind.py", "--probe", probe,
                    "--output", profiles], check=True)
    record_catalog(probe, profiles)

    footprint = reports / "footprint"
    footprint.mkdir()
    for linkage, shared in (("static", "OFF"), ("shared", "ON")):
        tree = build / f"footprint-{linkage}"
        options = [f"-DBUILD_SHARED_LIBS={shared}", "-DOPENTLV_BUILD_FOOTPRINT=ON",
                   "-DOPENTLV_SIZE_TOOL=/usr/bin/size"]
        settings[linkage] = build_targets(source, tree, "Release", RELEASE_FLAGS, options,
                                          ["opentlv_footprint"], linkage)
        shutil.copyfile(tree / "benchmarks/footprint/Release/footprint.json", footprint / f"{linkage}.json")
        shutil.copyfile(tree / "compile_commands.json", footprint / f"{linkage}-compile_commands.json")

    tree = build / "timing"
    timing = reports / "timing"
    options = ["-DBUILD_SHARED_LIBS=OFF", "-DOPENTLV_BUILD_BENCHMARKS=ON"]
    archive = os.environ.get("OPENTLV_GOOGLE_BENCHMARK_URL")
    if archive:
        options.append(f"-DOPENTLV_GOOGLE_BENCHMARK_URL={archive}")
    settings["timing"] = build_targets(source, tree, "Release", RELEASE_FLAGS, options,
                                       ["opentlv_check_fixtures", "opentlv_benchmark"], "timing")
    record_build(tree, "Release", timing)
    record_catalog(tree / "opentlv_check_fixtures", timing)
    with (tree / "timing.log").open("wb") as log:
        subprocess.run([tree / "opentlv_benchmark", "--benchmark_min_time=8192x",
                        "--benchmark_repetitions=3", "--benchmark_out_format=json",
                        f"--benchmark_out={timing / 'results.json'}"], stdout=log, check=True)

    (reports / "build-settings.json").write_text(json.dumps(settings, indent=2) + "\n", encoding="utf-8")
    # Hash the measurement boundaries and workloads separately from the production code.
    protocol = hashlib.sha256()
    for name in PROTOCOL_FILES:
        protocol.update(name.encode("ascii") + b"\0" + hashlib.sha256((source / name).read_bytes()).digest())
    (reports / "protocol.sha256").write_text(protocol.hexdigest() + "\n", encoding="ascii")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path, help="Git checkout to measure")
    parser.add_argument("build", type=Path, help="new build directory outside the checkout")
    parser.add_argument("reports", type=Path, help="new evidence directory outside the checkout")
    args = parser.parse_args()
    try:
        collect(args.source.resolve(strict=True), args.build.resolve(), args.reports.resolve())
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"{error}\n")
