#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Marek Cingel
"""Build each registered module alone, inspect symbols, and consume its installation.

Run on ELF platforms with GNU/LLVM nm. Windows package/runtime consumers remain
covered by check_cmake_integration.py. No functional module is fabricated for
the transitional monolith: only opentlv_add_module registrations are checked.
"""

import argparse
import json
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

from check_module_contracts import COMMENT, cycles

ROOT = Path(__file__).resolve().parents[1]
INCLUDE = re.compile(r'^\s*#\s*include\s*[<"]([^>"\n]+)[>"]', re.M)


def run(*args):
    print("+ " + " ".join(map(str, args)), flush=True)
    subprocess.run(list(map(str, args)), check=True)


def dependencies(module):
    return module["public_deps"] + module["private_deps"] + module.get("facade_deps", [])


def closure(modules, key):
    found, pending = set(), [key]
    while pending:
        current = pending.pop()
        if current not in found:
            found.add(current)
            pending.extend(dependencies(modules[current]))
    return found


def check_metadata(entries, contract=None, source_root=None):
    modules = {entry["key"]: entry for entry in entries}
    if len(modules) != len(entries):
        raise ValueError("Duplicate module keys")
    graph = {key: dependencies(entry) for key, entry in modules.items()}
    for key, deps in graph.items():
        if len(deps) != len(set(deps)) or set(deps) - modules.keys():
            raise ValueError(f"{key}: duplicate or undeclared dependency")
    if cycles(graph):
        raise ValueError("Module dependency cycle")
    owned, private = {}, set()
    for key, entry in modules.items():
        if entry["owner"][0] not in closure(modules, key):
            raise ValueError(f"{key}: integration does not depend on its owner")
        for kind in ("sources", "public_headers", "private_headers", "cxx_headers"):
            for name in entry.get(kind, []):
                path = Path(name).resolve()
                if path in owned:
                    raise ValueError(f"Duplicate file ownership: {path}")
                if not path.is_file():
                    raise ValueError(f"Missing owned file: {path}")
                owned[path] = key
                if kind == "private_headers":
                    private.add(path)
        if contract is not None:
            expected = contract["targets"].get(key)
            if expected is None:
                raise ValueError(f"{key}: missing architecture contract")
            for scope in ("public", "private"):
                if sorted(entry[scope + "_deps"]) != sorted(expected.get(scope, [])):
                    raise ValueError(f"{key}: {scope} dependencies differ from contract")
            if sorted(entry.get("facade_deps", [])) != sorted(expected.get("facade", [])):
                raise ValueError(f"{key}: facade dependencies differ from contract")
            if entry["owner"] != [expected["module"]]:
                raise ValueError(f"{key}: owner differs from contract")
    roots = [Path(entry["include_dir"][0]) for entry in entries]
    if source_root is not None:
        roots.append(source_root / "tlv++/include")
    if contract is not None and source_root is not None:
        for name, key in contract["files"].items():
            # Feature-disabled C sources remain owned by the contract but are
            # absent from this configuration's build. Installed/private headers
            # must still have a registered boundary in every configuration.
            if key in modules and name.endswith((".h", ".hpp")):
                if owned.get((source_root / name).resolve()) != key:
                    raise ValueError(f"{key}: contract file missing from registration: {name}")
        for path, key in owned.items():
            if path.is_relative_to(source_root):
                name = path.relative_to(source_root).as_posix()
                generated = name.rsplit("/generated/", 1)
                generated_owner = (contract.get("generated", {}).get(generated[1])
                                   if len(generated) == 2 else None)
                if contract["files"].get(name) != key and generated_owner != key:
                    raise ValueError(f"{key}: file ownership differs from contract: {name}")
    for path, key in owned.items():
        for include in INCLUDE.findall(COMMENT.sub("", path.read_text(encoding="utf-8"))):
            candidates = [path.parent / include] + [root / include for root in roots]
            resolved = next((p.resolve() for p in candidates if p.is_file()), None)
            if resolved is None:
                # A private -I directory must not hide a cross-module include
                # from the ownership check. Ambiguous basenames need an explicit
                # relative include so ownership can be determined reliably.
                matches = {p for p in private if p.as_posix().endswith("/" + include)}
                if len(matches) > 1:
                    raise ValueError(f"{path}: ambiguous private header {include}")
                if matches:
                    resolved = matches.pop()
            if resolved is None:
                # Compiler verifies system/generated includes during isolated builds.
                continue
            owner = owned.get(resolved)
            if owner is None:
                raise ValueError(f"{path}: unregistered project header {include}")
            if resolved in private:
                if path in {Path(p).resolve() for p in modules[key]["public_headers"] + modules[key].get("cxx_headers", [])}:
                    raise ValueError(f"{path}: public header includes private header {include}")
                if modules[key]["owner"] != modules[owner]["owner"]:
                    raise ValueError(f"{path}: cross-module private header {include}")
            if owner != key:
                allowed = dependencies(modules[key])
                if path in {Path(p).resolve() for p in modules[key]["public_headers"] + modules[key].get("cxx_headers", [])}:
                    allowed = modules[key]["public_deps"]
                    if path.suffix == ".hpp":
                        allowed = list(closure(modules, key))
                if owner not in allowed:
                    raise ValueError(f"{path}: undeclared direct dependency {owner}")
    return modules


def nm_symbols(nm, library, *options):
    output = subprocess.check_output(
        [nm, "-P", "-g", *options, str(library)], text=True, stderr=subprocess.PIPE)
    result = set()
    for line in output.splitlines():
        fields = line.split()
        if len(fields) >= 2 and len(fields[1]) == 1 and fields[1].isalpha():
            result.add(fields[0].split("@")[0])
    return result


def check_symbols(entries, shared, nm):
    modules = {entry["key"]: entry for entry in entries}
    defined, undefined, owners = {}, {}, {}
    for key, entry in modules.items():
        if "library" not in entry:
            continue
        defined[key] = nm_symbols(nm, entry["library"], "--defined-only")
        undefined[key] = nm_symbols(nm, entry["library"], "--undefined-only")
        expected = set(entry["public_symbols"] + entry["owner_symbols"])
        if not expected <= defined[key]:
            raise ValueError(f"{key}: declared symbols missing: {sorted(expected - defined[key])}")
        if shared:
            exported = nm_symbols(nm, entry["library"], "-D", "--defined-only")
            if exported != expected:
                raise ValueError(f"{key}: exported symbols differ: {sorted(exported ^ expected)}")
        for symbol in defined[key]:
            if symbol in owners:
                raise ValueError(f"Duplicate symbol ownership: {symbol}")
            owners[symbol] = key
    for key, symbols in undefined.items():
        for symbol in symbols - defined[key]:
            owner = owners.get(symbol)
            if owner is None:
                if symbol.startswith(("tlv_", "opentlv_")):
                    raise ValueError(f"{key}: unresolved library symbol {symbol}")
                continue  # External C runtime/toolchain symbols.
            if owner not in dependencies(modules[key]):
                raise ValueError(f"{key}: undeclared symbol dependency {owner}: {symbol}")
            if symbol not in modules[owner]["public_symbols"]:
                if (symbol not in modules[owner]["owner_symbols"] or
                        modules[key]["owner"] != modules[owner]["owner"]):
                    raise ValueError(f"{key}: private symbol from {owner}: {symbol}")


def verify(source, build_root, shared, generator, nm, extra=()):
    build_root.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix="modules-", dir=build_root.resolve()))
    print(f"Module isolation evidence: {work}", flush=True)
    common = ["-G", generator, "-DCMAKE_BUILD_TYPE=Release",
              f"-DOPENTLV_BUILD_SHARED_LIBS={shared}", "-DOPENTLV_BUILD_TESTS=OFF",
              "-DOPENTLV_BUILD_EXAMPLES=OFF", "-DOPENTLV_BUILD_CXX=OFF",
              "-DCMAKE_CXX_COMPILER=/opentlv-no-cxx-compiler",
              "-DCMAKE_INSTALL_LIBDIR=lib", *extra]

    def configure(build):
        run("cmake", "-S", source, "-B", build, *common)
        if list((build / "CMakeFiles").glob("*/CMakeCXXCompiler.cmake")):
            raise ValueError("C-only module build enabled a C++ compiler")
        return json.loads((build / "modules-Release.json").read_text(encoding="utf-8"))

    full = work / "complete"
    entries = configure(full)
    contract_path = source / "docs/development/modules/ownership.json"
    contract = json.loads(contract_path.read_text(encoding="utf-8")) if contract_path.exists() else None
    modules = check_metadata(entries, contract, source)
    if not modules:
        print("No extracted modules registered; legacy consumers are checked separately.")
        return work
    # Fresh target-only builds cannot reuse artifacts from a complete build.
    for key, entry in modules.items():
        if "library" not in entry:
            continue
        build = work / ("isolated-" + key)
        isolated = configure(build)
        run("cmake", "--build", build, "--config", "Release",
            "--target", entry["target"], "--parallel", "2")
        for candidate in isolated:
            if ("library" in candidate and Path(candidate["library"]).exists()
                    and candidate["key"] not in closure(modules, key)):
                raise ValueError(f"{key}: built unrelated module {candidate['key']}")
    run("cmake", "--build", full, "--config", "Release", "--parallel", "2")
    check_symbols(entries, shared == "ON", nm)
    prefix = work / "installed"
    run("cmake", "--install", full, "--config", "Release", "--prefix", prefix)
    relocated = work / "relocated package"
    shutil.copytree(prefix, relocated)
    for path in relocated.rglob("*.cmake"):
        content = path.read_text(encoding="utf-8").replace("\\", "/")
        if any(p.as_posix() in content for p in (source, full, prefix)):
            raise ValueError(f"Non-relocatable module package: {path}")
    cases = [(key, entry, False) for key, entry in modules.items()]
    cases += [(key, entry, True) for key, entry in modules.items() if entry.get("cxx_consumer")]
    for key, entry, cxx in cases:
        consumer = work / ("consumer-" + key + ("-cxx" if cxx else ""))
        consumer.mkdir()
        facade = cxx or entry["facade"] == ["TRUE"]
        language = "CXX" if facade else "C"
        extension = "cpp" if facade else "c"
        shutil.copyfile(entry["cxx_consumer" if cxx else "consumer"][0], consumer / ("main." + extension))
        name = entry["export_name"][0]
        expected = closure(modules, key)
        component_checks = ""
        if facade:
            # Compile every installed header on its own; one umbrella TU can
            # accidentally hide missing includes in a module-specific header.
            headers = entry.get("cxx_headers", []) if cxx else entry["public_headers"]
            for index, header in enumerate(headers):
                include = Path(header).as_posix().split("/include/")[-1]
                (consumer / f"header{index}.cpp").write_text(
                    f'#include <{include}>\n', encoding="utf-8")
                component_checks += (f"add_library(header{index} OBJECT header{index}.cpp)\n"
                                     f"target_link_libraries(header{index} PRIVATE OpenTLV::{name})\n")
        component_checks += ('if(TARGET OpenTLV::tlv++)\n'
                             'message(FATAL_ERROR "Obsolete C++ target")\nendif()\n')
        for other, candidate in modules.items():
            condition = "NOT TARGET" if other in expected else "TARGET"
            target = "OpenTLV::" + candidate["export_name"][0]
            component_checks += (f"if({condition} {target})\n"
                                 f'  message(FATAL_ERROR "Unexpected component closure: {target}")\n'
                                 "endif()\n")
        (consumer / "CMakeLists.txt").write_text(
            f"cmake_minimum_required(VERSION 3.16)\nproject(consumer LANGUAGES {language})\n"
            f"find_package(OpenTLV REQUIRED CONFIG COMPONENTS {name})\n"
            f"{component_checks}"
            f"add_executable(consumer main.{extension})\n"
            f"target_link_libraries(consumer PRIVATE OpenTLV::{name})\n"
            "enable_testing()\nadd_test(NAME consumer COMMAND consumer)\n", encoding="utf-8")
        build = consumer / "build"
        run("cmake", "-S", consumer, "-B", build, "-G", generator,
            "-DCMAKE_BUILD_TYPE=Release",
            *(["-DCMAKE_CXX_STANDARD=98"] if facade else []),
            f"-DCMAKE_PREFIX_PATH={relocated}", "-DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF",
            "-DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=OFF")
        run("cmake", "--build", build, "--config", "Release", "--parallel", "2")
        run("ctest", "--test-dir", build, "-C", "Release",
            "--output-on-failure", "--no-tests=error")
    print(f"PASS {len(cases)} C/C++ module/component consumers; shared={shared}")
    return work


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=ROOT)
    parser.add_argument("--build-root", type=Path, default=ROOT / "build/module-isolation")
    parser.add_argument("--shared", choices=("ON", "OFF"), default="ON")
    parser.add_argument("--generator", default="Ninja")
    parser.add_argument("--nm", default="nm")
    parser.add_argument("--cmake-arg", action="append", default=[])
    args = parser.parse_args()
    verify(args.source.resolve(), args.build_root, args.shared, args.generator, args.nm,
           args.cmake_arg)


if __name__ == "__main__":
    main()
