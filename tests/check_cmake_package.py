"""Build and relocate an OpenTLV package, then run its consumers and examples."""

import argparse
import os
import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def build_project(source, build, *options):
    subprocess.run(["cmake", "-S", source, "-B", build, "-DCMAKE_BUILD_TYPE=Release",
                    *options], check=True)
    subprocess.run(["cmake", "--build", build, "--config", "Release"], check=True)


def check_package(work, shared):
    build = work / "library"
    prefix = work / "original install"
    relocated = work / "relocated install"
    build_project(ROOT, build, f"-DCMAKE_INSTALL_PREFIX={prefix}",
                  "-DBUILD_SHARED_LIBS=" + ("ON" if shared else "OFF"))
    subprocess.run(["cmake", "--install", build, "--config", "Release"], check=True)
    prefix.rename(relocated)

    for config in relocated.rglob("*.cmake"):
        contents = os.path.normcase(config.read_text(encoding="utf-8").replace("\\", "/"))
        for private_path in (ROOT, build, prefix):
            if os.path.normcase(private_path.as_posix()) in contents:
                raise SystemExit(f"{config.name} contains private path {private_path}")

    env = os.environ.copy()
    if os.name == "nt":
        env["PATH"] = str(relocated / "bin") + os.pathsep + env["PATH"]

    for language in ("C", "CXX"):
        consumer = work / f"consumer-{language}"
        build_project(ROOT / "tests" / "package", consumer, f"-DCONSUMER_LANGUAGE={language}",
                      f"-DPACKAGE_PREFIX={relocated}")
        subprocess.run(["ctest", "-C", "Release", "--output-on-failure"],
                       cwd=consumer, env=env, check=True)

    package_config = next(relocated.rglob("OpenTLVConfig.cmake"))
    binaries = work / "example binaries"
    build_project(ROOT / "examples", work / "examples", f"-DOpenTLV_DIR={package_config.parent}",
                  f"-DCMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE={binaries}")
    for name, expected in (("opentlv-write", "12 34 00 03 AA BB CC"),
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
    args = parser.parse_args()
    with tempfile.TemporaryDirectory() as directory:
        check_package(Path(directory).resolve(), args.shared)
