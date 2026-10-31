#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Marek Cingel
"""Configure, link and execute representative capability profiles."""
import argparse
from pathlib import Path
import subprocess

# Module selection belongs to target links. Only Query implementation features
# still have configuration profiles; each profile runs all native module probes.
PROFILES = {
    "full": ("ON", "ON"),
    "static-query": ("OFF", "OFF"),
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-root", default="build/capabilities")
    parser.add_argument("--profile", choices=PROFILES, action="append")
    parser.add_argument("--generator")
    parser.add_argument("--static", action="store_true")
    parser.add_argument("--cmake-arg", action="append", default=[])
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    for name in args.profile or PROFILES:
        build = Path(args.build_root).resolve() / name
        command = ["cmake", "-S", str(root), "-B", str(build),
                   "-DOPENTLV_BUILD_CXX=OFF", "-DOPENTLV_BUILD_TESTS=ON",
                   "-DOPENTLV_BUILD_UNIT_TESTS=OFF", "-DOPENTLV_BUILD_INTEGRATION_TESTS=OFF",
                   "-DOPENTLV_BUILD_QUERY_TESTS=OFF", "-DOPENTLV_BUILD_EXAMPLES=OFF",
                   "-DOPENTLV_BUILD_SHARED_LIBS=" + ("OFF" if args.static else "ON")]
        if args.generator:
            command += ["-G", args.generator]
        frontend, sets = PROFILES[name]
        command += [f"-DOPENTLV_QUERY_FRONTEND={frontend}",
                    f"-DOPENTLV_QUERY_SET_OPERATIONS={sets}"]
        command += args.cmake_arg
        print(f"Capability profile: {name}", flush=True)
        subprocess.run(command, check=True)
        subprocess.run(["cmake", "--build", str(build), "--config", "Debug",
                        "--parallel", "1"], check=True)
        subprocess.run(["ctest", "--test-dir", str(build), "-C", "Debug",
                        "-L", "capabilities|module:", "--output-on-failure",
                        "--no-tests=error"], check=True)


if __name__ == "__main__":
    main()
