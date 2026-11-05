#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Marek Cingel
"""Fail closed on behavioral divergence between like-for-like distributions."""

import argparse
from collections import Counter
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time
import xml.etree.ElementTree as ET

from verify_modular_baseline import collect_gtest

ROOT = Path(__file__).resolve().parents[1]
VARIANTS = ("conventional", "installed", "amalgamation")


def require(condition, message):
    if not condition:
        raise ValueError(message)


def write(path, value):
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def run(args, log, env=None):
    print(f"{log.parent.name}: {log.name}", flush=True)
    with log.open("wb") as stream:
        result = subprocess.run(list(map(str, args)), stdout=stream,
                                stderr=subprocess.STDOUT, env=env)
    output = log.read_bytes()
    require(result.returncode == 0, f"command failed ({result.returncode}): {log}\n"
            + output.decode(errors="replace")[-6000:])
    return output


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def compiler_flags(build, config):
    cache = dict(re.findall(r"^(CMAKE_C(?:XX)?_FLAGS(?:_\w+)?):\w+=(.*)$",
                            (build / "CMakeCache.txt").read_text(), re.MULTILINE))
    return {language: " ".join(cache.get(key, "") for key in
            (f"CMAKE_{language}_FLAGS", f"CMAKE_{language}_FLAGS_{config.upper()}"))
            for language in ("C", "CXX")}


def relocate(prefix, destination):
    # Windows scanners can briefly retain newly installed files. Retry the
    # rename; never silently fall back to testing the original installation.
    require(prefix.resolve().parent == destination.resolve().parent,
            "relocation must stay inside the evidence directory")
    for attempt in range(20):
        try:
            prefix.rename(destination)
            return
        except PermissionError:
            if os.name != "nt" or attempt == 19:
                raise
            time.sleep(0.5)


def check_metadata(metadata, config, modules, frontend, sets):
    require(metadata["schema"] == 1 and metadata["library"] == "OpenTLV", "unknown metadata contract")
    require(metadata["configuration"] == {"query_frontend": frontend,
            "query_set_operations": sets}, "Query configuration differs")
    for field in ("version", "revision", "commit", "branch", "tag"):
        require(str(metadata[field]) == config[field.upper()], f"metadata {field} differs")
    require([row["key"] for row in metadata["targets"]] ==
            [row["key"] for row in modules if row["type"] != "INTERFACE_LIBRARY"],
            "module composition differs")


def scenarios(path):
    # CTest excludes the binary-only ABI check, but no selected case may skip.
    rows = ET.parse(path).findall(".//testcase")
    require(rows, f"empty CTest evidence: {path}")
    for row in rows:
        require(row.get("status") == "run" and row.find("failure") is None
                and row.find("skipped") is None, f"test did not pass: {row.get('name')}")
    return Counter(row.attrib["name"] for row in rows)


def equal_scenarios(reference, current, variant):
    require(reference == current, f"{variant}: scenario inventory differs; "
            f"missing={dict(reference - current)}, extra={dict(current - reference)}")


def equal_observations(reference, current, variant):
    changed = sorted(key for key in reference.keys() | current.keys()
                     if key not in reference or key not in current or reference[key] != current[key])
    require(not changed, f"{variant}: observable results differ: {changed} (see observations.json)")


def gtest_scenarios(directory):
    return Counter(re.sub(r"\d+-byte object <[^>]+>", "<opaque-parameter>", row["name"])
                   for row in collect_gtest(directory))


def symbols(nm, libraries, log):
    output = run([nm, "--defined-only", "--extern-only", *libraries], log).decode(errors="replace")
    # Compiler support symbols are not part of the library's external contract.
    result = Counter()
    for line in output.splitlines():
        match = re.search(r"\b[TDBRCSG] (_?[A-Za-z]\w*)$", line)
        if match:
            result[match[1].removeprefix("_")] += 1
    require(result, f"no OpenTLV symbols found: {log}")
    require(all(count == 1 for count in result.values()), f"duplicate external symbols: {log}")
    return result


def api_source(modules, flat):
    headers = sorted({p.split("/include/")[-1] for row in modules
                      for p in row["public_headers"] + row["generated_headers"]})
    names = sorted({name for row in modules for name in row["public_symbols"]})
    includes = '#include "opentlv.h"\n' if flat else "".join(f'#include "{p}"\n' for p in headers)
    return includes + "int main() {\n" + "".join(
        f"  auto volatile p{i} = &{name}; (void)p{i};\n" for i, name in enumerate(names)) + "}\n"


def verify(args):
    work = args.output.resolve()
    require(not work.exists(), "use a new --output directory to avoid stale evidence")
    work.mkdir(parents=True)
    report = {"status": "failed", "variants": {}, "binary_abi": "separate compatible-binary gate"}
    try:
        cc, cxx, nm = (shutil.which(tool) for tool in (args.cc, args.cxx, args.nm))
        require(cc and cxx and nm, "C/C++ compilers and nm must be on PATH")
        require(not args.sanitize or sys.platform.startswith("linux"), "sanitizers require Linux")
        common = ["-G", "Ninja", f"-DCMAKE_C_COMPILER={cc}", f"-DCMAKE_CXX_COMPILER={cxx}",
                  f"-DCMAKE_BUILD_TYPE={args.config}", f"-DOPENTLV_QUERY_FRONTEND={args.frontend}",
                  f"-DOPENTLV_QUERY_SET_OPERATIONS={args.set_operations}"]
        if args.sanitize:
            flags = "-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer"
            common += [f"-DCMAKE_C_FLAGS={flags}", f"-DCMAKE_CXX_FLAGS={flags}"]
        report["configuration"] = {"cmake": common, "shared": args.shared,
                                   "sanitize": args.sanitize}
        report["commit"] = run(["git", "-C", ROOT, "rev-parse", "HEAD"], work / "commit.log").decode().strip()
        report["working_tree"] = run(["git", "-C", ROOT, "status", "--porcelain"], work / "status.log").decode()
        library = work / "library"
        run(["cmake", "-S", ROOT, "-B", library, *common, "-DOPENTLV_BUILD_TESTS=OFF",
             "-DOPENTLV_BUILD_EXAMPLES=OFF", f"-DOPENTLV_BUILD_SHARED_LIBS={args.shared}"], work / "configure.log")
        run(["cmake", "--build", library, "--parallel", args.jobs], work / "build.log")
        library_flags = compiler_flags(library, args.config)["C"]
        report["configuration"]["library_c_flags"] = library_flags
        report["configuration"]["ndebug"] = bool(re.search(r"[-/]D\s*NDEBUG\b", library_flags))
        run(["cmake", "--build", library, "--target", "opentlv_amalgamation"], work / "generate.log")
        artifact = library / "amalgamation"
        metadata = json.loads((artifact / "opentlv-amalgamation.json").read_text())
        modules = json.loads((library / f"modules-{args.config}.json").read_text())
        config = dict(re.findall(r"set\(OPENTLV_AMALGAMATION_(\w+) \[==\[(.*?)\]==\]\)",
                                (library / "opentlv-amalgamation.cmake").read_text()))
        check_metadata(metadata, config, modules, args.frontend == "ON", args.set_operations == "ON")
        for header in ("opentlv/core/version.h", "opentlv/query/features.h"):
            require((artifact / "include" / header).read_text() ==
                    (library / "generated/include" / header).read_text(), f"generated header differs: {header}")
        for header in (ROOT / "include").rglob("*"):
            if header.is_file():
                shipped = artifact / "include" / header.relative_to(ROOT / "include")
                require(shipped.read_text(encoding="utf-8") == header.read_text(encoding="utf-8"),
                        f"amalgamation public header differs: {header}")
        write(work / "amalgamation-metadata.json", metadata)
        report["artifact_hashes"] = {p.relative_to(artifact).as_posix(): digest(p)
                                     for p in artifact.rglob("*") if p.is_file()}
        prefix = work / "install"
        run(["cmake", "--install", library, "--prefix", prefix], work / "install.log")
        installed_hashes = {p.relative_to(prefix).as_posix(): digest(p)
                            for p in prefix.rglob("*") if p.is_file()}
        relocate(prefix, work / "relocated")
        prefix = work / "relocated"
        require(installed_hashes == {p.relative_to(prefix).as_posix(): digest(p)
                                     for p in prefix.rglob("*") if p.is_file()},
                "relocated installation differs from verified install manifest")
        write(work / "installed-files.json", installed_hashes)
        # This gate qualifies binaries built from this exact source/configuration.
        # A published binary from an older version is not a comparable input.
        binaries = {}
        for row in modules:
            if "library" not in row:
                continue
            source = Path(row["library"])
            matches = list(prefix.rglob(source.name))
            require(len(matches) == 1,
                    f"installed binary identity differs: {row['key']}")
            # ELF installation may rewrite RPATH. Record both identities; the
            # relocated file must match the installed manifest, not build bytes.
            binaries[row["key"]] = {"build_sha256": digest(source), "installed_sha256": digest(matches[0])}
        report["verified_installed_binaries"] = binaries
        reference = None
        for variant in VARIANTS:
            env = dict(os.environ)
            if os.name == "nt":
                runtime_root = prefix if variant == "installed" else library
                dll_dirs = sorted({str(p.parent) for p in runtime_root.rglob("*.dll")})
                env["PATH"] = os.pathsep.join([*dll_dirs, env.get("PATH", "")])
            evidence = work / variant
            evidence.mkdir()
            report["active_variant"] = variant
            build = evidence / "build"
            build.mkdir()
            (build / "api.cpp").write_text(api_source(modules, variant == "amalgamation"))
            package = library if variant == "conventional" else next(prefix.rglob("OpenTLVConfig.cmake")).parent
            headers = artifact / "include" if variant == "amalgamation" else prefix / "include"
            options = [f"-DVARIANT={variant}", f"-DARTIFACT={artifact.as_posix()}",
                       f"-DPACKAGE={package.as_posix()}", f"-DHEADERS={headers.as_posix()}",
                       f"-DREGISTRY={(library / 'opentlv-registry.cmake').as_posix()}",
                       "-DOPENTLV_PROPERTY_SEED=743", "-DOPENTLV_PROPERTY_COUNT=32",
                       f"-DOPENTLV_PROPERTY_COMMIT={report['commit']}"]
            if args.gtest_source:
                options += [f"-DFETCHCONTENT_SOURCE_DIR_GOOGLETEST={args.gtest_source.resolve().as_posix()}"]
            elif reference:
                options += [f"-DFETCHCONTENT_SOURCE_DIR_GOOGLETEST={(work / 'conventional/build/_deps/googletest-src').as_posix()}"]
            run(["cmake", "-S", ROOT / "tests/amalgamation/equivalence", "-B", build,
                 *common, *options], evidence / "configure.log", env)
            run(["cmake", "--build", build, "--parallel", args.jobs], evidence / "build.log", env)
            toolchain = json.loads((build / "toolchain.json").read_text())
            toolchain["flags"] = compiler_flags(build, args.config)
            require(toolchain["flags"]["C"] == library_flags,
                    f"{variant}: library and consumer C flags differ")
            if "toolchain" in report:
                require(report["toolchain"] == toolchain, f"{variant}: toolchain configuration differs")
            else:
                report["toolchain"] = toolchain
            test_env = dict(env, GTEST_OUTPUT="xml:" + (evidence / "gtest").as_posix() + "/")
            run(["ctest", "--test-dir", build, "--output-on-failure", "--no-tests=error",
                 "--parallel", args.jobs, "-E", "^(query-abi-snapshot|include-order)$",
                 "--output-junit", evidence / "ctest.xml"], evidence / "ctest.log", test_env)
            if variant == "amalgamation":
                run(["ctest", "--test-dir", build, "-R", "^include-order$", "--no-tests=error",
                     "--output-on-failure"], evidence / "include-order.log", env)
            suffix = ".exe" if os.name == "nt" else ""
            observations = {}
            for name in ("behavior", "diagnostic_strings", "capabilities"):
                observations[name] = run([build / "bin" / (name + suffix)], evidence / f"{name}.log", env).decode()
            observations["generator_bytes"] = {
                p.relative_to(build / "property-data").as_posix(): digest(p)
                for p in (build / "property-data").rglob("*.bin")}
            require(len(observations["generator_bytes"]) == 320, "expected 320 shared Generator cases")
            fuzz = {}
            for executable in sorted((build / "bin").glob("replay_*" + suffix)):
                if executable.suffix not in ("", ".exe"):
                    continue
                name = executable.stem.removeprefix("replay_")
                corpus_name = "query_match" if name == "query_pairs" else name
                seeds = [p for p in (ROOT / "tests/fuzz").rglob("*") if p.is_file() and "corpus" in p.parts
                         and (("query_" if "query" in p.parts else "") + p.parent.name) == corpus_name]
                require(seeds, f"no corpus for {name}")
                run([executable, *sorted(seeds)], evidence / f"fuzz-{name}.log", env)
                fuzz[name] = {p.relative_to(ROOT).as_posix(): digest(p) for p in sorted(seeds)}
            require(fuzz, "no fuzz replay targets")
            observations["fuzz_corpus"] = fuzz
            tests = scenarios(evidence / "ctest.xml")
            gtests = gtest_scenarios(evidence / "gtest")
            write(evidence / "observations.json", observations)
            write(evidence / "scenarios.json", {"ctest": tests, "gtest": gtests})
            if reference is None:
                reference = (tests, gtests, observations)
            else:
                equal_scenarios(reference[0], tests, variant)
                equal_scenarios(reference[1], gtests, variant)
                equal_observations(reference[2], observations, variant)
            report["variants"][variant] = {"status": "passed", "ctest": sum(tests.values()),
                                            "gtest": sum(gtests.values()), "fuzz_targets": len(fuzz)}
        if args.shared == "OFF":
            conventional = symbols(nm, [row["library"] for row in modules if "library" in row], work / "symbols-conventional.log")
            amalgamated = symbols(nm, [(work / "amalgamation/build" / f"library-{args.config}.txt").read_text()],
                                  work / "symbols-amalgamation.log")
            equal_scenarios(conventional, amalgamated, "external symbols")
            report["external_symbols"] = len(conventional)
        report["status"] = "passed"
        report.pop("active_variant", None)
    except (ValueError, OSError) as error:
        report["error"] = str(error)
        raise
    finally:
        write(work / "report.json", report)
    print(f"Distribution equivalence passed: {work / 'report.json'}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--cc", default="cl" if os.name == "nt" else "cc")
    parser.add_argument("--cxx", default="cl" if os.name == "nt" else "c++")
    parser.add_argument("--nm", default="llvm-nm" if os.name == "nt" else "nm")
    parser.add_argument("--shared", choices=("ON", "OFF"), default="OFF")
    parser.add_argument("--frontend", choices=("ON", "OFF"), default="ON")
    parser.add_argument("--set-operations", choices=("ON", "OFF"), default="ON")
    parser.add_argument("--config", choices=("Release", "Debug"), default="Release")
    parser.add_argument("--sanitize", action="store_true")
    parser.add_argument("--jobs", default="2")
    parser.add_argument("--gtest-source", type=Path)
    verify(parser.parse_args())


if __name__ == "__main__":
    main()
