"""Build and relocate an OpenTLV package, then run installed C and C++ consumers."""

import argparse
import os
import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def check_package(work, shared):
    build = work / "library"
    prefix = work / "original install"
    relocated = work / "relocated install"
    subprocess.run([
        "cmake", "-S", ROOT, "-B", build, "-DCMAKE_BUILD_TYPE=Release",
        f"-DCMAKE_INSTALL_PREFIX={prefix}",
        "-DBUILD_SHARED_LIBS=" + ("ON" if shared else "OFF"),
    ], check=True)
    subprocess.run(["cmake", "--build", build, "--config", "Release"], check=True)
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
        subprocess.run([
            "cmake", "-S", ROOT / "tests" / "package", "-B", consumer,
            "-DCMAKE_BUILD_TYPE=Release", f"-DCONSUMER_LANGUAGE={language}",
            f"-DPACKAGE_PREFIX={relocated}",
        ], check=True)
        subprocess.run(["cmake", "--build", consumer, "--config", "Release"], check=True)
        subprocess.run(["ctest", "-C", "Release", "--output-on-failure"],
                       cwd=consumer, env=env, check=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--shared", action="store_true", help="build shared libraries")
    args = parser.parse_args()
    with tempfile.TemporaryDirectory() as directory:
        check_package(Path(directory).resolve(), args.shared)
