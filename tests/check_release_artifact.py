"""Build the release archive twice, inspect it and exercise the packaged source."""

import argparse
import hashlib
import os
from pathlib import Path
import subprocess
import sys
import tarfile
import tempfile


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from package_source import GIT, release_version


def package_checkout(directory, commit, version, autocrlf):
    checkout = directory / "checkout"
    archive_dir = directory / "archive"
    subprocess.run(["git", "init", "--quiet", checkout], check=True)
    subprocess.run(["git", "config", "core.autocrlf", autocrlf], cwd=checkout, check=True)
    subprocess.run(["git", "fetch", "--quiet", "--depth=1", "--no-tags", ROOT, commit],
                   cwd=checkout, check=True)
    subprocess.run(["git", "checkout", "--quiet", "--detach", commit], cwd=checkout, check=True)
    command = [sys.executable, "-B", "scripts/package_source.py", "--output-dir", archive_dir]
    if version is not None:
        command.extend(["--version", version])
    subprocess.run(command, cwd=checkout, check=True)
    [archive] = archive_dir.iterdir()
    return archive


def unpack_and_check(archive, work, commit, prefix):
    # Include trees as well as blobs so extra directories and duplicate names
    # fail the same inventory check as unexpected files.
    entries = subprocess.check_output([*GIT, "ls-tree", "-r", "-t", "-z", commit])
    expected = {prefix: ("040000", None)}
    for entry in entries.rstrip(b"\0").split(b"\0"):
        metadata, path = entry.split(b"\t", 1)
        mode, _, oid = metadata.decode("ascii").split()
        expected[f"{prefix}/{path.decode('utf-8')}"] = (mode, oid)
    if f"{prefix}/LICENSE" not in expected:
        raise SystemExit("Release candidate is missing LICENSE")
    timestamp = int(subprocess.check_output([*GIT, "show", "-s", "--format=%ct", commit]))

    with tarfile.open(archive, "r:gz") as package:
        if package.pax_headers.get("comment") != commit:
            raise SystemExit("Archive commit metadata differs from the candidate")
        for member in package:
            if member.name not in expected:
                raise SystemExit(f"Unexpected or duplicate archive member: {member.name}")
            mode, oid = expected.pop(member.name)
            permissions = 0o755 if mode == "040000" else int(mode, 8) & 0o777
            if member.mode != permissions or member.mtime != timestamp:
                raise SystemExit(f"Archive permissions or timestamp differ for {member.name}")
            if mode == "040000" and member.isdir():
                continue
            if mode not in ("100644", "100755") or not member.isfile():
                raise SystemExit(f"Unexpected member type: {member.name}")
            data = package.extractfile(member).read()
            digest = hashlib.new("sha1" if len(oid) == 40 else "sha256")
            digest.update(f"blob {len(data)}\0".encode("ascii"))
            digest.update(data)
            if digest.hexdigest() != oid:
                raise SystemExit(f"Archive contents differ from Git: {member.name}")
            path = work / member.name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
            path.chmod(permissions)
    if expected:
        raise SystemExit(f"Missing archive members: {sorted(expected)}")
    return work / prefix


def verify(output_dir, ref, version):
    commit = subprocess.check_output([*GIT, "rev-parse", "--verify", "--end-of-options",
                                      ref + "^{commit}"], text=True).strip()
    prefix = f"opentlv-{release_version(commit, version)}"
    output_dir.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=output_dir) as directory:
        work = Path(directory)
        first = package_checkout(work / "first", commit, version, "false")
        second = package_checkout(work / "second", commit, version, "true")
        if first.name != f"{prefix}-source.tar.gz" or first.name != second.name:
            raise SystemExit(f"Unexpected archive names: {first.name}, {second.name}")
        data = first.read_bytes()
        if data != second.read_bytes():
            raise SystemExit(f"The two builds produced different bytes for {first.name}")

        source = unpack_and_check(first, work, commit, prefix)
        env = dict(os.environ, TMPDIR=directory, TEMP=directory, TMP=directory)
        for linkage in ([], ["--shared"]):
            subprocess.run([sys.executable, "-B", "tests/check_cmake_package.py", "--check-symbols",
                            *linkage], cwd=source, env=env, check=True)
        first.replace(output_dir / first.name)
        print(f"{hashlib.sha256(data).hexdigest()}  {first.name}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output_dir", type=Path, help="destination for the verified archive")
    parser.add_argument("--ref", default="HEAD", help="candidate Git ref (default: HEAD)")
    parser.add_argument("--version", help="release version passed to the packager")
    args = parser.parse_args()
    verify(args.output_dir.resolve(), args.ref, args.version)
