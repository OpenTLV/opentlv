"""Run consumers against build-tree and relocated OpenTLV packages and check the examples."""

import argparse
import os
import re
import subprocess
import tempfile
from pathlib import Path

from check_symbols import check_symbols


ROOT = Path(__file__).resolve().parents[1]
TEST_COMPONENT = re.compile(
    r"(?<![a-z0-9])(?:lib|-l)?(?:googletest|googlemock|gtest|gmock)"
    r"|\bopentlv(?:_|::)(?:core|io|integration)_tests\b",
    re.IGNORECASE,
)


def build_project(source, build, *options):
    subprocess.run(["cmake", "-S", source, "-B", build, "-DCMAKE_BUILD_TYPE=Release",
                    *options], check=True)
    subprocess.run(["cmake", "--build", build, "--config", "Release"], check=True)


def check_install_tree(relocated, private_paths):
    public_headers = {path.relative_to(ROOT) for path in (ROOT / "include").rglob("*")
                      if path.suffix in (".h", ".hpp")}
    for path in relocated.rglob("*"):
        relative = path.relative_to(relocated)
        if path.suffix in (".h", ".hpp") and relative not in public_headers:
            raise SystemExit(f"Unexpected header in production install: {relative}")
        public_headers.discard(relative)
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
    if public_headers:
        raise SystemExit(f"Missing installed public headers: {sorted(public_headers)}")


def runtime_environment(prefix):
    env = os.environ.copy()
    if os.name == "nt":
        env["PATH"] = str(prefix / "bin") + os.pathsep + env["PATH"]
    return env


def check_consumers(work, package_prefix):
    for language in ("C", "CXX"):
        consumer = work / language
        build_project(ROOT / "tests" / "package", consumer, f"-DCONSUMER_LANGUAGE={language}",
                      f"-DPACKAGE_PREFIX={package_prefix}")
        subprocess.run(["ctest", "-C", "Release", "--output-on-failure"],
                       cwd=consumer, env=runtime_environment(package_prefix), check=True)


def check_package(work, shared, audit_symbols=False):
    build = work / "library"
    prefix = work / "original install"
    relocated = work / "relocated install"
    build_project(ROOT, build, "-DBUILD_TESTING=OFF", f"-DCMAKE_INSTALL_PREFIX={prefix}",
                  f"-DCMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE={build / 'bin'}",
                  "-DBUILD_SHARED_LIBS=" + ("ON" if shared else "OFF"))
    check_consumers(work / "build-tree consumers", build)

    subprocess.run(["cmake", "--install", build, "--config", "Release"], check=True)
    prefix.rename(relocated)

    check_install_tree(relocated, (ROOT, build, prefix))
    if audit_symbols:
        check_symbols(relocated, shared, build)
    check_consumers(work / "installed consumers", relocated)

    package_config = next(relocated.rglob("OpenTLVConfig.cmake"))
    binaries = work / "example binaries"
    build_project(ROOT / "examples", work / "examples", f"-DOpenTLV_DIR={package_config.parent}",
                  f"-DCMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE={binaries}")
    env = runtime_environment(relocated)
    for name, expected in (("opentlv-write", "12 34 00 03 AA BB CC"),
                           ("opentlv-write-cpp", "12 34 00 03 AA BB CC"),
                           ("opentlv-read", "tag 12 34, value AA BB CC")):
        executable = binaries / (name + ".exe" if os.name == "nt" else name)
        output = subprocess.run([executable], env=env, stdout=subprocess.PIPE,
                                text=True, check=True).stdout
        if output != expected + "\n":
            raise SystemExit(f"{name}: expected {expected!r}, got {output!r}")
        print(f"{name}: {expected}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--shared", action="store_true", help="build shared libraries")
    parser.add_argument("--check-symbols", action="store_true",
                        help="compare installed library symbols with the public C ABI baseline")
    args = parser.parse_args()
    with tempfile.TemporaryDirectory() as directory:
        check_package(Path(directory).resolve(), args.shared, args.check_symbols)
