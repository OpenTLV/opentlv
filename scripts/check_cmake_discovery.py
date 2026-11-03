#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Marek Cingel
"""Exercise real package/component/version resolution, including rejected probes."""

import shutil
import subprocess


def check_discovery(root, work, core_build, prefix, version, configure_options, core_options):
    """Use the built Library and relocated installation from the consumer gate."""
    package_dir = prefix / "lib/cmake/OpenTLV"
    conflict = work / "conflicting installation"
    conflict_config = conflict / "lib/cmake/OpenTLV"
    shutil.copytree(package_dir, conflict_config)
    major, minor, patch = version.split(".")
    other = f"{major}.{minor}.{int(patch) + 1}"
    (conflict_config / "OpenTLVConfigVersion.cmake").write_text(
        f'set(PACKAGE_VERSION "{other}")\n'
        f'if(PACKAGE_FIND_VERSION VERSION_EQUAL "{other}")\n'
        '  set(PACKAGE_VERSION_COMPATIBLE TRUE)\n'
        '  set(PACKAGE_VERSION_EXACT TRUE)\nendif()\n', encoding="utf-8")
    config = conflict_config / "OpenTLVConfig.cmake"
    config.write_text(config.read_text(encoding="utf-8").replace(
        f'set(OpenTLV_VERSION "{version}")', f'set(OpenTLV_VERSION "{other}")'), encoding="utf-8")

    cmake_version = subprocess.check_output(["cmake", "--version"], text=True).split()[2]
    redirects = tuple(map(int, cmake_version.split(".")[:2])) >= (3, 24)
    source = root / "tests/cmake-discovery"
    cases = []
    for package in ("OpenTLV", "opentlv_core", "opentlv"):
        for mode in ("installed", "build-tree"):
            for case in ("positive", "required-component", "required-version"):
                cases.append((mode, package, case, ""))
    if redirects:
        for package in ("opentlv_core", "opentlv"):
            for case in ("positive", "required-component"):
                cases.append(("fetch-package", package, case, ""))
    for dependency in ("opentlv_core", "opentlv", "subdirectory"):
        cases.append(("embedded", dependency, "positive", ""))
        if redirects and dependency != "subdirectory":
            cases.append(("embedded", dependency, "positive", "plain"))
            cases.append(("embedded", dependency, "positive", "override"))
        cases.append(("embedded", dependency, "required-component", ""))
        cases.append(("embedded", dependency, "required-version", ""))

    for mode, name, case, variant in cases:
        label = "-".join(filter(None, (mode, name, case, variant)))
        build = work / "discovery" / label
        package = "OpenTLV" if name == "subdirectory" else name
        search = core_build if mode == "build-tree" else prefix
        other_config = package_dir if mode == "build-tree" else core_build
        command = ["cmake", "-S", str(source), "-B", str(build), *configure_options,
                   *core_options, f"-DMODE={mode}", f"-DPACKAGE={package}", f"-DCASE={case}",
                   f"-DVERSION={version}", f"-DOTHER_VERSION={other}",
                   f"-DDEPENDENCY={name}", f"-DDEPENDENCY_UPPER={name.upper()}",
                   f"-DLIBRARY_SOURCE={root.as_posix()}", f"-DCMAKE_PREFIX_PATH={search.as_posix()}",
                   f"-DCONFLICT_PREFIX={conflict.as_posix()}",
                   f"-DOTHER_CONFIG={(other_config / 'OpenTLVConfig.cmake').as_posix()}",
                   f"-DPLAIN_FETCH={'ON' if variant == 'plain' else 'OFF'}",
                   f"-DOVERRIDE_PACKAGE={'ON' if variant == 'override' else 'OFF'}",
                   "-DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF",
                   "-DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=OFF"]
        result = subprocess.run(command, text=True, capture_output=True, check=False)
        build.mkdir(parents=True, exist_ok=True)
        output = result.stdout + result.stderr
        (build / "discovery.log").write_text(output, encoding="utf-8")
        if case == "required-component":
            passed = result.returncode != 0 and "Unknown or unavailable OpenTLV component: unavailable" in output
        elif case == "required-version":
            passed = result.returncode != 0 and "requested version" in output
        else:
            passed = result.returncode == 0
        if not passed:
            raise RuntimeError(f"CMake discovery failed ({label}):\n{output}")
        print(f"PASS discovery: {label}", flush=True)
