"""Build the Fixed TLV release source archive from a candidate Git commit."""

import argparse
import gzip
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]
GIT = ["git", "--no-replace-objects", "-C", str(ROOT)]
PROJECT_VERSION = re.compile(
    r"^\s*project\s*\(\s*OpenTLV\s+VERSION\s+(\d+\.\d+\.\d+)(?=[\s)])",
    re.MULTILINE | re.IGNORECASE | re.ASCII,
)


def release_version(commit, requested):
    cmake = subprocess.check_output([*GIT, "cat-file", "blob", f"{commit}:CMakeLists.txt"],
                                    encoding="utf-8")
    project = PROJECT_VERSION.search(cmake)
    if not project:
        raise ValueError(f"{commit}: missing OpenTLV project version in CMakeLists.txt")
    base = project[1]
    version = base if requested is None else requested
    # A release may append SemVer prerelease and build identifiers to CMake's version.
    identifier = r"[0-9A-Za-z-]+"
    identifiers = rf"{identifier}(?:\.{identifier})*"
    match = re.fullmatch(rf"{re.escape(base)}(?:-({identifiers}))?(?:\+{identifiers})?", version)
    if not match:
        raise ValueError(f"expected a release version with CMake version {base}, got {version!r}")
    for part in (match[1] or "").split("."):
        if part.isdigit() and len(part) > 1 and part.startswith("0"):
            raise ValueError(f"invalid leading zero in prerelease version {version!r}")
    return version


def package(ref, requested, output_dir):
    if not (ROOT / ".git").exists():
        raise ValueError(f"{ROOT}: building a release archive requires an OpenTLV Git checkout")
    commit = subprocess.check_output([*GIT, "rev-parse", "--verify", "--end-of-options",
                                      f"{ref}^{{commit}}"], text=True).strip()
    version = release_version(commit, requested)
    name = f"opentlv-{version}"
    output_dir.mkdir(parents=True, exist_ok=True)
    archive = output_dir / f"{name}-source.tar.gz"
    # Git supplies the candidate's tree order, modes, commit time and PAX commit ID.
    command = [*GIT, "-c", "core.autocrlf=false", "-c", "core.eol=lf",
               "-c", f"core.attributesFile={os.devnull}", "-c", "tar.umask=0022",
               "archive", "--format=tar", f"--prefix={name}/", commit]
    with tempfile.TemporaryDirectory(dir=output_dir) as scratch:
        temporary = Path(scratch) / archive.name
        with temporary.open("wb") as raw, gzip.GzipFile(
            filename="", fileobj=raw, mode="wb", compresslevel=9, mtime=0,
        ) as compressed:
            with subprocess.Popen(command, stdout=subprocess.PIPE,
                                  env={**os.environ, "GIT_ATTR_NOSYSTEM": "1"}) as tar:
                shutil.copyfileobj(tar.stdout, compressed)
                if tar.wait():
                    raise subprocess.CalledProcessError(tar.returncode, command)
        temporary.replace(archive)
    print(f"Packaged {commit} as {archive}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ref", default="HEAD", help="candidate Git revision (default: HEAD)")
    parser.add_argument("--version", help="full release version (default: candidate CMake version)")
    parser.add_argument("--output-dir", required=True, type=Path, help="archive destination directory")
    args = parser.parse_args()
    try:
        package(args.ref, args.version, args.output_dir)
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"{parser.prog}: {error}\n")


if __name__ == "__main__":
    main()
