"""Replay the checked-in inputs and run both bounded Fixed TLV fuzz targets."""

import argparse
import os
import subprocess
import tempfile
from pathlib import Path


def run(build, artifacts):
    corpus = Path(__file__).resolve().parent / "corpus"
    for target in ("reader", "writer"):
        executable = build / (f"fuzz_{target}.exe" if os.name == "nt" else f"fuzz_{target}")
        seeds = sorted((corpus / target).iterdir())
        if not seeds:
            raise SystemExit(f"Empty seed corpus for {target}")
        output = artifacts / target
        output.mkdir(parents=True, exist_ok=True)
        command = [executable, "-seed=31", "-max_len=1024", "-timeout=5",
                   "-rss_limit_mb=512", f"-artifact_prefix={output.as_posix()}/"]
        subprocess.run([*command, *seeds], check=True)
        with tempfile.TemporaryDirectory() as generated:
            subprocess.run([*command, "-max_total_time=30", generated, corpus / target], check=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build", type=Path)
    parser.add_argument("artifacts", type=Path)
    args = parser.parse_args()
    run(args.build.resolve(), args.artifacts.resolve())
