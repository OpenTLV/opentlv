"""Build and run source and package consumers of the OpenTLV release candidate."""

import argparse
import os
import re
import shutil
import subprocess
import tempfile
from pathlib import Path

from check_doc_examples import code_block, readme_outputs, write_cpp_examples
from check_symbols import check_symbols
from release_baseline import public_headers


ROOT = Path(__file__).resolve().parents[1]
TEST_COMPONENT = re.compile(
    r"(?<![a-z0-9])(?:lib|-l)?(?:googletest|googlemock|gtest|gmock)"
    r"|\bopentlv(?:_|::)(?:core|io|integration)_tests\b",
    re.IGNORECASE,
)


def build_project(source, build, *options, c_only=False):
    env = os.environ.copy()
    if c_only:
        env["CXX"] = "opentlv-unavailable-cxx"
    subprocess.run(["cmake", "-S", source, "-B", build, "-DCMAKE_BUILD_TYPE=Release",
                    *options], env=env, check=True)
    # IDE generators can select their own C++ compiler independently of CXX.
    if c_only and any((build / "CMakeFiles").glob("*/CMakeCXXCompiler.cmake")):
        raise SystemExit(f"C-only configuration enabled C++: {build}")
    subprocess.run(["cmake", "--build", build, "--config", "Release"], env=env, check=True)


def check_install_tree(relocated, private_paths):
    expected = {"include/" + header for header in public_headers(ROOT / "include")}
    for path in relocated.rglob("*"):
        relative = path.relative_to(relocated)
        if path.is_file() and path.suffix in (".h", ".hpp"):
            if relative.as_posix() not in expected:
                raise SystemExit(f"Unexpected header in production install: {relative}")
            expected.remove(relative.as_posix())
        if TEST_COMPONENT.search(relative.as_posix()):
            raise SystemExit(f"Test artifact in production install: {relative}")
        if path.suffix.lower() != ".cmake" or not path.is_file():
            continue
        contents = path.read_text(encoding="utf-8")
        reference = TEST_COMPONENT.search(contents)
        if reference:
            raise SystemExit(f"{relative} references test dependency {reference.group()}")
        normalized = os.path.normcase(contents.replace("\\", "/"))
        for private_path in private_paths:
            if os.path.normcase(private_path.as_posix()) in normalized:
                raise SystemExit(f"{relative} contains private path {private_path}")
    if expected:
        raise SystemExit(f"Missing installed public headers: {sorted(expected)}")


def runtime_environment(prefix):
    env = os.environ.copy()
    if os.name == "nt":
        env["PATH"] = str(prefix / "bin") + os.pathsep + env["PATH"]
    return env


def check_consumers(work, location, include_root, shared, mode="find_package", doc_examples=None):
    library_type = "SHARED_LIBRARY" if shared else "STATIC_LIBRARY"
    options = [f"-DCONSUMPTION_MODE={mode}", f"-DEXPECTED_LIBRARY_TYPE={library_type}",
               f"-DHEADER_INVENTORY_ROOT={include_root}"]
    if mode == "FetchContent":
        # Exercise normal population from the local checkout. CMake 3.15 ignores
        # a declaration's SOURCE_DIR in fully disconnected mode.
        options.append(f"-DFETCHCONTENT_SOURCE_DIR_GOOGLETEST:PATH={work / 'unavailable-googletest'}")
    else:
        options.append("-DFETCHCONTENT_FULLY_DISCONNECTED:BOOL=ON")
    if mode == "find_package":
        options.append(f"-DPACKAGE_PREFIX={location}")
        env = runtime_environment(location)
    else:
        options.extend((f"-DOPENTLV_SOURCE_DIR:PATH={location}", "-DBUILD_TESTING=ON",
                        "-DBUILD_SHARED_LIBS=" + ("ON" if shared else "OFF")))
        env = None
    for language in ("C", "CXX"):
        consumer = work / language
        if language == "CXX" and doc_examples:
            options.append(f"-DDOC_EXAMPLES_DIR:PATH={doc_examples}")
        # Keep embedded DLLs beside the consumer executables on Windows.
        build_project(ROOT / "tests" / "package", consumer, f"-DCONSUMER_LANGUAGE={language}",
                      f"-DCMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE={consumer / 'bin'}",
                      *options, c_only=(language == "C"))
        subprocess.run(["ctest", "-C", "Release", "--output-on-failure"],
                       cwd=consumer, env=env, check=True)


def check_documented_output(executable, expected, env):
    if os.name == "nt":
        executable = executable.with_suffix(".exe")
    output = subprocess.check_output([executable], env=env, text=True)
    if output != expected + "\n":
        raise SystemExit(f"{executable.name}: expected {expected!r}, got {output!r}")
    print(f"{executable.name}: {expected}")


def check_readme_examples(work, prefix):
    readme = ROOT / "README.md"
    programs = {"example": "examples/write.c", "opentlv-write": "examples/write.c",
                "opentlv-write-cpp": "examples/write.cpp", "opentlv-read": "examples/read.cpp"}
    outputs = readme_outputs(readme, set(programs.values()))
    env = runtime_environment(prefix)
    binaries = work / "example binaries"
    options = [f"-DCMAKE_PREFIX_PATH={prefix}", f"-DCMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE={binaries}"]

    source = work / "README consumer"
    source.mkdir()
    (source / "CMakeLists.txt").write_text(code_block(readme, "Use the installed package", "cmake"),
                                          encoding="utf-8")
    shutil.copy2(ROOT / "examples" / "write.c", source)
    package_dir = next(prefix.rglob("OpenTLVConfig.cmake")).parent
    for project, build in ((source, work / "README build"), (ROOT / "examples", work / "examples")):
        build_project(project, build, *options, c_only=(project == source))
        cache = (build / "CMakeCache.txt").read_text(encoding="utf-8")
        found = re.search(r"^OpenTLV_DIR:PATH=(.+)$", cache, re.MULTILINE)
        if not found or Path(found[1]).resolve() != package_dir.resolve():
            raise SystemExit(f"{build}: expected find_package to select {package_dir}")
    for name, path in programs.items():
        check_documented_output(binaries / name, outputs[path], env)


def check_package(work, shared, audit_symbols=False):
    prefix = work / "original install"
    relocated = work / "relocated install"
    # Only the installation survives this scope. Both installed consumer runs
    # must resolve their headers, libraries and runtime dependencies from it.
    with tempfile.TemporaryDirectory(dir=work) as directory:
        source = Path(directory) / "src"
        build = Path(directory) / "build"
        shutil.copytree(ROOT, source, symlinks=True,
                        ignore=shutil.ignore_patterns(".git", "build", "build-*", "__pycache__", work.name))
        for mode in ("add_subdirectory", "FetchContent"):
            check_consumers(Path(directory) / mode, source, source / "include", shared, mode)
        build_project(source, build, f"-DCMAKE_INSTALL_PREFIX={prefix}",
                      f"-DCMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE={build / 'bin'}",
                      "-DFETCHCONTENT_FULLY_DISCONNECTED:BOOL=ON",
                      "-DBUILD_SHARED_LIBS=" + ("ON" if shared else "OFF"), c_only=True)
        check_consumers(work / "build-tree consumers", build, source / "include", shared)
        subprocess.run(["cmake", "--install", build, "--config", "Release"], check=True)
        if audit_symbols:
            check_symbols(prefix, shared, build)

    check_consumers(work / "installed consumers", prefix, prefix / "include", shared)
    prefix.rename(relocated)

    check_install_tree(relocated, (ROOT, source, build, prefix))
    doc_examples = work / "guide examples"
    write_cpp_examples(doc_examples)
    check_consumers(work / "relocated consumers", relocated, relocated / "include", shared,
                    doc_examples=doc_examples)
    check_readme_examples(work, relocated)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--shared", action="store_true", help="build shared libraries")
    parser.add_argument("--check-symbols", action="store_true",
                        help="compare installed library symbols with the public C ABI baseline")
    args = parser.parse_args()
    with tempfile.TemporaryDirectory() as directory:
        check_package(Path(directory).resolve(), args.shared, args.check_symbols)
