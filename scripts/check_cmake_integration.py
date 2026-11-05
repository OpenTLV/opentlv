#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Marek Cingel
"""Build external consumers through FetchContent, add_subdirectory() and a relocated
installation, and check their CMake package and target identities (#747)."""

import argparse
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

from check_cmake_discovery import check_discovery


# Current CMake identities from the naming contract inventory (#746, #747). A
# change to any of them is a naming change under docs/development/naming.md.
CACHE_OPTIONS = frozenset(f"OPENTLV_{name}" for name in (
    "BUILD_SHARED_LIBS", "BUILD_CXX", "QUERY_FRONTEND", "QUERY_SET_OPERATIONS",
    "BUILD_EXAMPLES", "BUILD_DOCS", "BUILD_TESTS", "BUILD_UNIT_TESTS",
    "BUILD_INTEGRATION_TESTS", "BUILD_QUERY_TESTS", "BUILD_PROPERTY_TESTS",
    "BUILD_BENCHMARKS", "BUILD_FUZZING", "BUILD_MEMCHECK", "WARNINGS_AS_ERRORS"))
REMOVED_OPTIONS = tuple(f"OPENTLV_{name}" for name in (
    "READER", "WRITER", "DOCUMENT", "QUERY", "CODEC", "SCHEMA", "FORMAT_ASN1", "FORMAT_BER",
    "FORMAT_DER", "FORMAT_CER", "EMV", "BLUETOOTH", "NFC", "DHCP", "LLDP"))
# Only project(OpenTLV); IS_TOP_LEVEL exists from CMake 3.21.
PROJECT_VARIABLES = frozenset(f"{project}_{name}" for project in ("OpenTLV",)
                              for name in ("SOURCE_DIR", "BINARY_DIR", "IS_TOP_LEVEL"))
# Entries owned by CMake: GNUInstallDirs, GenerateExportHeader and FetchContent.
CMAKE_OWNED = re.compile(r"^(?:CMAKE_|_GNUInstallDirs_|FETCHCONTENT_|COMPILER_HAS_)")
PACKAGE_FILES = frozenset(("OpenTLVConfig.cmake", "OpenTLVConfigVersion.cmake",
                           "OpenTLVModules.cmake", "OpenTLVTargets.cmake"))
# Consumer-side FetchContent names: the legacy spelling and the contract's canonical one.
DEPENDENCIES = {"fetch": "opentlv_core", "fetch-package": "opentlv_core", "fetch-canonical": "opentlv",
                "fetch-package-canonical": "opentlv", "fetch-collision": "opentlv_core",
                "fetch-collision-after": "opentlv_core"}


def run(*args, cwd=None):
    print("+ " + " ".join(map(str, args)), flush=True)
    subprocess.run(list(map(str, args)), check=True, cwd=cwd)


def contract_identities(contract):
    """Map each public target suffix to its contract key and unnamespaced build target."""
    identities = {}
    for key, entry in contract["targets"].items():
        public = entry.get("public_target") or contract["modules"][key]["public_target"]
        aggregate = entry["role"] == "aggregate"
        identities[public.split("::", 1)[1]] = {
            "key": key, "aggregate": aggregate,
            "target": "opentlv_tlv" if aggregate else "opentlv_" + key.replace("-", "_")}
    return identities


def check_package_identity(files, registry, identities):
    """Compare installed package files and the component registry with the contract."""
    errors = []
    components = {name: value["key"] for name, value in identities.items() if not value["aggregate"]}
    listed = re.findall(r'^list\(APPEND _OpenTLV_components "([^"]+)"\)$', registry, re.M)
    keys = dict(re.findall(r'^set\(_OpenTLV_key_(\S+) "([^"]+)"\)$', registry, re.M))
    if sorted(listed) != sorted(components) or len(listed) != len(set(listed)) or keys != components:
        errors.append(f"component registry differs from the contract: {sorted(listed)}, {keys}")
    exports = {"OpenTLVTargets"} | {f"OpenTLV-{key}Targets" for key in components.values()}
    required = PACKAGE_FILES | {f"{export}.cmake" for export in exports}
    configured = re.compile(r"^(" + "|".join(map(re.escape, sorted(exports))) + r")-[a-z]+\.cmake$")
    missing = sorted(required - set(files))
    unexpected = sorted(name for name in set(files) - required if not configured.match(name))
    if missing or unexpected:
        errors.append(f"package files differ: missing={missing}, unexpected={unexpected}")
    return errors


def read_embedded_report(path):
    """Parse the consumer's 'kind name [type]' lines into targets and cache snapshots."""
    report = {"target": {}, "before": set(), "after": set()}
    for line in path.read_text(encoding="utf-8").splitlines():
        kind, name, *rest = line.split()
        if kind == "target":
            report["target"][name] = rest[0]
        else:
            report[kind].add(name)
    return report


def check_embedded_identity(report, identities, dependency):
    """Reject unnamespaced targets and cache entries beyond the inventoried identities."""
    errors = []
    expected = {value["target"] for value in identities.values()}
    actual = set(report["target"])
    if actual != expected:
        errors.append(f"embedded build targets differ: missing={sorted(expected - actual)}, "
                      f"unexpected={sorted(actual - expected)}")
    options = {name for name in report["after"] if name.startswith("OPENTLV_")}
    if options != CACHE_OPTIONS:
        errors.append(f"embedded cache options differ: missing={sorted(CACHE_OPTIONS - options)}, "
                      f"unexpected={sorted(options - CACHE_OPTIONS)}")
    allowed = PROJECT_VARIABLES | CACHE_OPTIONS | {f"{dependency}_DIR"}
    unknown = sorted(name for name in report["after"] - report["before"]
                     if name not in allowed and not CMAKE_OWNED.match(name))
    if unknown:
        errors.append(f"embedded build added uninventoried cache entries: {unknown}")
    return errors


def require_no_errors(errors, context):
    """Fail with every error, naming the checked package or consumer mode."""
    if errors:
        raise RuntimeError(f"CMake identity check failed ({context}):\n  " + "\n  ".join(errors))


def expected_example_tests(entries, *, cxx, query_frontend, aggregate=True):
    """Select declared tests using the consumer language and installed features."""
    return {entry["test"] for entry in entries
            if (entry["language"] != "cpp" or cxx)
            and (not entry["query_frontend"] or query_frontend)
            and (not entry["umbrella"] or aggregate)}


def check_example_tests(listing, expected):
    """Reject missing, unexpected or duplicated CTest names, even at equal counts."""
    names = [test["name"] for test in listing["tests"]]
    actual = set(names)
    if actual != expected or len(names) != len(actual):
        duplicates = sorted(name for name in actual if names.count(name) > 1)
        raise RuntimeError(f"Installed example test mismatch: missing={sorted(expected - actual)}, "
                           f"unexpected={sorted(actual - expected)}, duplicates={duplicates}")


def installed_examples(root, work, prefix, args):
    """Build the copied examples without access to repository CMake helpers."""
    source = work / "copied examples"
    shutil.copytree(root / "examples", source)
    cmake = Path(shutil.which(args.examples_cmake) or args.examples_cmake).resolve()
    ctest = cmake.with_name("ctest.exe" if cmake.suffix == ".exe" else "ctest")
    build = work / "examples"
    options = ["-DCMAKE_BUILD_TYPE=Release", f"-DOPENTLV_BUILD_CXX={args.cxx}",
               "-DCMAKE_C_STANDARD=99", "-DCMAKE_C_STANDARD_REQUIRED=ON", "-DCMAKE_C_EXTENSIONS=OFF",
               "-DCMAKE_CXX_STANDARD=11", "-DCMAKE_CXX_STANDARD_REQUIRED=ON", "-DCMAKE_CXX_EXTENSIONS=OFF",
               "-DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF", "-DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=OFF"]
    if args.generator:
        options += ["-G", args.generator]
    options += args.cmake_arg
    if args.cxx == "OFF":
        options += ["-DCMAKE_CXX_COMPILER=/opentlv-no-cxx-compiler"]
    run(cmake, "-S", source, "-B", build, *options, f"-DCMAKE_PREFIX_PATH={prefix.as_posix()}")

    def build_and_test(build, *, aggregate=True):
        if args.cxx == "OFF" and list((build / "CMakeFiles").glob("*/CMakeCXXCompiler.cmake")):
            raise RuntimeError("C-only examples enabled a C++ compiler")
        registry = build / "examples.jsonl"
        run(sys.executable, "-B", root / "scripts/check_module_contracts.py", "--examples", registry)
        entries = [json.loads(line) for line in registry.read_text(encoding="utf-8").splitlines() if line.strip()]
        expected = expected_example_tests(entries, cxx=args.cxx == "ON",
                                          query_frontend=args.query_frontend == "ON", aggregate=aggregate)
        run(cmake, "--build", build, "--config", "Release", "--parallel", "1")
        # CTest 3.16 has no --test-dir. Audit and execute the same complete inventory.
        listing = json.loads(subprocess.check_output(
            [str(ctest), "-C", "Release", "--show-only=json-v1"], cwd=build, text=True))
        check_example_tests(listing, expected)
        run(ctest, "-C", "Release", "--output-on-failure", cwd=build)
        return entries

    entries = build_and_test(build)
    # Build and run every available example without the optional aggregate,
    # using a separate installation so earlier consumers remain unaffected.
    component_prefix = work / "component-only installation"
    shutil.copytree(prefix, component_prefix)
    (component_prefix / "lib/cmake/OpenTLV/OpenTLVTargets.cmake").unlink()
    component_build = work / "examples-components"
    result = subprocess.run([str(cmake), "-S", str(source), "-B", str(component_build),
                             *options, f"-DCMAKE_PREFIX_PATH={component_prefix.as_posix()}"],
                            text=True, capture_output=True, check=True)
    print(result.stdout, end="", flush=True)
    for entry in entries:
        if not entry["umbrella"] or (entry["language"] == "cpp" and args.cxx == "OFF"):
            continue
        skip = f"Skipping {entry['target']}: optional OpenTLV::tlv component is unavailable"
        if skip not in result.stdout:
            raise RuntimeError("Missing optional aggregate did not produce an explicit skip")
    build_and_test(component_build, aggregate=False)
    print(f"PASS installed examples: shared={args.shared}, cxx={args.cxx}, frontend={args.query_frontend}", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-root", type=Path, default=Path("build/cmake-integration"))
    parser.add_argument("--generator")
    parser.add_argument("--shared", choices=("ON", "OFF"), default="ON")
    parser.add_argument("--cxx", choices=("ON", "OFF"), default="ON")
    parser.add_argument("--query-frontend", choices=("ON", "OFF"), default="ON")
    parser.add_argument("--examples-cmake", default="cmake",
                        help="CMake executable for standalone examples (CI also exercises 3.16)")
    parser.add_argument("--library-cxx", choices=("ON", "OFF"),
                        help="Library development-language switch, independent of consumer language")
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
                    f"-DOPENTLV_BUILD_CXX={args.library_cxx or args.cxx}",
                    f"-DOPENTLV_QUERY_FRONTEND={args.query_frontend}"]

    def configure(source, build, *options):
        run("cmake", "-S", source, "-B", build, *common, *options)

    def build_and_test(build, targets=()):
        selected = [argument for target in targets for argument in ("--target", target)]
        run("cmake", "--build", build, "--config", "Release", "--parallel", "1", *selected)
        # CTest 3.16 has no --test-dir. Reject empty suites explicitly.
        listing = json.loads(subprocess.check_output(
            ["ctest", "-C", "Release", "--show-only=json-v1"], cwd=build, text=True))
        if not listing["tests"]:
            raise RuntimeError(f"No consumer tests registered in {build}")
        run("ctest", "-C", "Release", "--output-on-failure", cwd=build)

    # A standalone build supplies the reference metadata and installed package.
    core_build = work / "core"
    # CMake 3.16 GraphViz omits aliases. Trace executed declarations instead of
    # inspecting source text, so both supported CMake paths check the real branch.
    result = subprocess.run(["cmake", "-S", str(root), "-B", str(core_build), *common, *core_options,
                             "-DOPENTLV_BUILD_TESTS=OFF", "-DOPENTLV_BUILD_EXAMPLES=OFF",
                             "-DCMAKE_CXX_COMPILER=/opentlv-no-cxx-compiler",
                             "-DCMAKE_INSTALL_LIBDIR=lib", "-DCMAKE_INSTALL_INCLUDEDIR=include",
                             "--trace-expand", f"--trace-source={root / 'CMakeLists.txt'}"],
                            capture_output=True, text=True)
    (work / "standalone-configure.log").write_text(result.stdout + result.stderr, encoding="utf-8")
    if result.returncode:
        raise RuntimeError(result.stdout + result.stderr)
    for alias in ("OpenTLV::tlv", "tlv"):
        if not re.search(r'add_library\(\s*' + re.escape(alias) + r'\s+ALIAS\s+opentlv_tlv\s*\)', result.stderr):
            raise RuntimeError(f"Standalone aggregate lost its {alias} alias")
    if list((core_build / "CMakeFiles").glob("*/CMakeCXXCompiler.cmake")):
        raise RuntimeError("Library/facade-only configuration enabled a C++ compiler")
    cache = (core_build / "CMakeCache.txt").read_text(encoding="utf-8")
    if re.search(r"^(OPENTLV_BUILD_(?:CLI(?:_TESTS)?|PYTHON)|nlohmann_json_[^:]*):", cache, re.M):
        raise RuntimeError("Core still configures migrated consumer options or dependencies")
    run("cmake", "--build", core_build, "--config", "Release", "--parallel", "1")
    reference = (core_build / "generated/include/opentlv/core/version.h").read_bytes()
    version = re.search(rb'#define OPENTLV_VERSION_STRING "([^"]+)"', reference)[1].decode()
    numeric_version = re.search(rb'#define OPENTLV_VERSION_MAJOR (\d+)', reference)[1].decode()
    for component in ("MINOR", "PATCH"):
        numeric_version += "." + re.search(
            rb'#define OPENTLV_VERSION_' + component.encode() + rb' (\d+)', reference)[1].decode()
    prefix = work / "installed"
    run("cmake", "--install", core_build, "--config", "Release", "--prefix", prefix)
    # The amalgamation is the explicit standalone target opentlv_amalgamation
    # (#740): neither ALL nor install() generates it. Embedding builds do not
    # receive the target; the embedded target inventory above rejects it.
    amalgamation = core_build / "amalgamation"
    if amalgamation.exists() or any(prefix.rglob("opentlv-amalgamation.json")):
        raise RuntimeError("The default build or installation generated the amalgamation")
    run("cmake", "--build", core_build, "--config", "Release", "--target", "opentlv_amalgamation")
    for name in ("opentlv.h", "opentlv.c", "opentlv-amalgamation.json", "include/opentlv/tlv.h"):
        if not (amalgamation / name).is_file():
            raise RuntimeError(f"opentlv_amalgamation did not write {name}")
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

    # Installed package identity: file names, components and exact versions.
    identities = contract_identities(json.loads(
        (root / "docs/development/modules/ownership.json").read_text(encoding="utf-8")))
    package_dir = relocated / "lib/cmake/OpenTLV"
    require_no_errors(check_package_identity(
        [path.name for path in package_dir.iterdir()],
        (package_dir / "OpenTLVModules.cmake").read_text(encoding="utf-8"), identities),
        "installed package")
    if not (relocated / "share/doc/OpenTLV/LICENSE").is_file():
        raise RuntimeError("Documentation is not installed under share/doc/OpenTLV")
    major, minor, patch = numeric_version.split(".")
    other = work / "other-version"
    other.mkdir()
    (other / "CMakeLists.txt").write_text(
        "cmake_minimum_required(VERSION 3.16)\nproject(other LANGUAGES NONE)\n"
        f"find_package(OpenTLV {major}.{minor}.{int(patch) + 1} EXACT CONFIG REQUIRED)\n",
        encoding="utf-8")
    result = subprocess.run(["cmake", "-S", str(other), "-B", str(other / "build"),
                             f"-DCMAKE_PREFIX_PATH={relocated.as_posix()}"],
                            capture_output=True, text=True)
    if result.returncode == 0 or "requested version" not in result.stdout + result.stderr:
        raise RuntimeError("ExactVersion package accepted a different patch version")
    for option in REMOVED_OPTIONS:
        result = subprocess.run(["cmake", "-S", str(root), "-B", str(work / "removed" / option),
                                 f"-D{option}=ON"], capture_output=True, text=True)
        if result.returncode == 0 or f"{option} was removed" not in result.stdout + result.stderr:
            raise RuntimeError(f"Removed option {option} is no longer rejected")
    print(f"PASS package identity: {len(identities)} targets, {len(REMOVED_OPTIONS)} removed options",
          flush=True)

    for path in relocated.rglob("*.cmake"):
        content = path.read_text(encoding="utf-8").replace("\\", "/")
        for private_path in (root, core_build, prefix):
            if private_path.as_posix() in content:
                raise RuntimeError(f"Non-relocatable package: {path}")

    # Generator's header-only facade must work without the aggregate library.
    # Compile copied sources against the relocated installation only.
    for language in (["C", "CXX"] if args.cxx == "ON" else ["C"]):
        generator_consumer = work / ("generator-" + language.lower())
        generator_consumer.mkdir()
        extension = "cpp" if language == "CXX" else "c"
        shutil.copyfile(root / f"tests/modules/generator.{extension}",
                        generator_consumer / f"main.{extension}")
        (generator_consumer / "CMakeLists.txt").write_text(
            f"cmake_minimum_required(VERSION 3.16)\nproject(generator LANGUAGES {language})\n"
            "find_package(OpenTLV REQUIRED CONFIG COMPONENTS Generator)\n"
            "foreach(unrelated IN ITEMS tlv tlv++ Document Query Schema Codec)\n"
            "  if(TARGET OpenTLV::${unrelated})\n"
            "    message(FATAL_ERROR \"Generator loaded unrelated target ${unrelated}\")\n"
            "  endif()\nendforeach()\n"
            f"add_executable(consumer main.{extension})\n"
            "target_link_libraries(consumer PRIVATE OpenTLV::Generator)\n"
            "get_target_property(kind OpenTLV::Generator TYPE)\n"
            "if(WIN32 AND kind STREQUAL \"SHARED_LIBRARY\")\n"
            "  add_custom_command(TARGET consumer POST_BUILD\n"
            "    COMMAND ${CMAKE_COMMAND} -E copy_if_different\n"
            "      $<TARGET_FILE:OpenTLV::Generator> $<TARGET_FILE:OpenTLV::IO>\n"
            "      $<TARGET_FILE:OpenTLV::Core> $<TARGET_FILE_DIR:consumer> VERBATIM)\n"
            "endif()\n"
            # The public target must raise an older requested standard to C++11.
            "set_target_properties(consumer PROPERTIES CXX_STANDARD 98 CXX_STANDARD_REQUIRED ON)\n"
            "enable_testing()\nadd_test(NAME generator COMMAND consumer)\n",
            encoding="utf-8")
        configure(generator_consumer, generator_consumer / "build",
                  f"-DCMAKE_PREFIX_PATH={relocated.as_posix()}",
                  "-DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF",
                  "-DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=OFF")
        build_and_test(generator_consumer / "build")

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
    consumer_options.append("-DEXPECTED_TARGETS=" + ";".join(
        f"{name}:{value['target']}" for name, value in sorted(identities.items())))
    # The full fetch build compiles every embedded module; the other source modes
    # check identity and build only the application against Core.
    modes = ["package", "package-all", "build-tree", "fetch", "fetch-canonical",
             "fetch-collision", "fetch-collision-after", "subdirectory", "subdirectory-after"]
    cmake_version = subprocess.check_output(["cmake", "--version"], text=True)
    if tuple(map(int, re.search(r"version (\d+)\.(\d+)", cmake_version).groups())) >= (3, 24):
        modes.extend(("fetch-package", "fetch-package-canonical"))
    applications = ["consumer_c", "consumer_cpp"] if args.cxx == "ON" else ["consumer_c"]
    for mode in modes:
        build = work / mode
        dependency = DEPENDENCIES.get(mode, "")
        options = [*consumer_options, f"-DCONSUMER_MODE={mode}",
                   f"-DCMAKE_PREFIX_PATH={relocated.as_posix()}",
                   "-DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF",
                   "-DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=OFF"]
        if dependency:
            options.append(f"-DCONSUMER_DEPENDENCY={dependency}")
        if mode == "build-tree":
            options += [f"-DOpenTLV_DIR={core_build.as_posix()}",
                        f"-DEXPECTED_PREFIX={core_build.as_posix()}"]
        if mode in ("fetch", "fetch-canonical", "fetch-collision", "fetch-collision-after"):
            options += core_options + [
                f"-DFETCHCONTENT_SOURCE_DIR_{dependency.upper()}={root.as_posix()}"]
        elif mode.startswith("subdirectory"):
            options += core_options + [f"-DCONSUMER_SOURCE_DIR={root.as_posix()}"]
        configure(consumer, build, *options)
        if mode in ("fetch", "fetch-canonical", "fetch-collision", "fetch-collision-after",
                    "subdirectory", "subdirectory-after"):
            embedded = build / ("opentlv" if mode.startswith("subdirectory") else f"_deps/{dependency}-build")
            generated = embedded / "generated/include/opentlv/core/version.h"
            if generated.read_bytes() != reference:
                raise RuntimeError("Embedded Core version differs from standalone metadata")
            require_no_errors(check_embedded_identity(
                read_embedded_report(build / "opentlv-embedded.txt"), identities, dependency),
                f"{mode} consumer")
        build_and_test(build, () if mode == "fetch" else applications)
        print(f"PASS {mode}: shared={args.shared}, cxx={args.cxx}", flush=True)

    check_discovery(root, work, core_build, relocated, numeric_version,
                    common, core_options)
    installed_examples(root, work, relocated, args)


if __name__ == "__main__":
    main()
