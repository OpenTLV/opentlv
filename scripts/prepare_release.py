"""Validate the Fixed TLV candidate and extract its changelog section."""

import argparse
from pathlib import Path
import subprocess
import sys

from package_source import GIT, ROOT, release_version


def prepare(tools, sha, version, notes):
    head = subprocess.check_output([*GIT, "rev-parse", "--verify", "HEAD"], text=True).strip()
    if head != sha:
        raise ValueError(f"expected candidate {sha}, found checkout {head}")
    release_version(head, version)
    committed = subprocess.check_output([*GIT, "rev-parse", "--verify", f"{head}:CHANGELOG.md"])
    working = subprocess.check_output([*GIT, "hash-object", "--", "CHANGELOG.md"])
    if committed != working:
        raise ValueError(f"CHANGELOG.md differs from the file committed at {head}")
    subprocess.run([sys.executable, "-B", tools / "scripts" / "release_notes.py",
                    "--changelog", ROOT / "CHANGELOG.md", "--version", version,
                    "--output", notes], check=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tools", required=True, type=Path, help="pinned OpenTLV/.github checkout")
    parser.add_argument("--sha", required=True, help="full candidate commit SHA")
    parser.add_argument("--version", required=True, help="release version without a leading v")
    parser.add_argument("--notes", required=True, type=Path, help="output Markdown file")
    args = parser.parse_args()
    try:
        prepare(args.tools, args.sha, args.version, args.notes)
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"{parser.prog}: {error}\n")
