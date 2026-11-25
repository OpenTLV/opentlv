"""Measure the checked Fixed TLV workloads twice and compare their Callgrind Ir counts."""

import argparse
import hashlib
import json
import os
import platform
import re
import shutil
import subprocess
from pathlib import Path


VERSION = "3.22.0"
OPTIONS = ("--command-line-only=yes", "--tool=callgrind", "--collect-atstart=no",
           "--cache-sim=no", "--branch-sim=no", "--callgrind-out-file=callgrind.out",
           "--log-file=valgrind.log")


def workload_ids(listing):
    ids = listing.splitlines()
    catalog = set(ids)
    if not catalog or len(catalog) != len(ids):
        raise ValueError("probe --list returned an empty catalog or duplicate IDs")
    for workload in catalog:
        if not re.fullmatch(r"(c99|cpp17)/v1(?:/[a-z0-9-]+)+", workload):
            raise ValueError(f"invalid workload ID {workload!r}")
        language, case = workload.split("/", 1)
        other = "cpp17" if language == "c99" else "c99"
        if f"{other}/{case}" not in catalog:
            raise ValueError(f"missing {other} counterpart for {workload}")
    return catalog


def read_counts(directory, catalog):
    counts = {}
    terminated = False
    for path in sorted(directory.glob("callgrind.out*")):
        lines = path.read_text(encoding="utf-8").splitlines()

        def field(prefix):
            values = [line[len(prefix):].strip() for line in lines if line.startswith(prefix)]
            if len(values) != 1:
                raise ValueError(f"{path}: expected one {prefix} line")
            return values[0]

        if field("creator:") != f"callgrind-{VERSION}" or field("events:") != "Ir":
            raise ValueError(f"{path}: expected Callgrind {VERSION} with only the Ir event")
        trigger = field("desc: Trigger:")
        total = field("totals:")
        if trigger == "Program termination":
            if terminated or path.name != "callgrind.out" or total not in ("", "0"):
                raise ValueError(f"{path}: unexpected termination dump or unscoped instructions")
            terminated = True
            continue
        prefix = "Client Request: "
        if not trigger.startswith(prefix):
            raise ValueError(f"{path}: unexpected trigger {trigger!r}")
        workload = trigger[len(prefix):]
        if workload not in catalog or workload in counts:
            raise ValueError(f"{path}: unexpected or duplicate workload {workload!r}")
        if not re.fullmatch(r"[0-9]+", total) or int(total) <= 0:
            raise ValueError(f"{path}: expected a positive Ir total for {workload}, got {total!r}")
        counts[workload] = int(total)
    if not terminated:
        raise ValueError(f"{directory}: missing program termination dump")
    missing = catalog - counts.keys()
    if missing:
        raise ValueError(f"{directory}: missing workload(s): {', '.join(sorted(missing))}")
    return counts


def compare_runs(first, second):
    differences = [f"{workload}: {first[workload]} -> {second[workload]}"
                   for workload in sorted(first) if first[workload] != second[workload]]
    if differences:
        raise ValueError("Ir changed between runs:\n" + "\n".join(differences))


def host_metadata():
    cpu = Path("/proc/cpuinfo").read_text().split("\n\n", 1)[0]
    fields = dict(line.split(":", 1) for line in cpu.splitlines() if ":" in line)
    host = {"kernel": platform.release(), "machine": platform.machine(), "libc": platform.libc_ver(),
            "cpu": {key.strip(): value.strip() for key, value in fields.items()
                    if key.strip() in ("model name", "flags")}}
    os_release = Path("/etc/os-release")
    if os_release.is_file():
        host["os_release"] = os_release.read_text().splitlines()
    if shutil.which("dpkg-query"):
        # Record installed package revisions on the reference Ubuntu host.
        # A locally built toolchain can be absent from the package database.
        query = subprocess.run(["dpkg-query", "-W", "-f=${binary:Package}\t${Version}\n",
                                "gcc-13", "g++-13", "libc6", "libstdc++6", "valgrind"],
                               stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True)
        host["dpkg_versions"] = dict(row.split("\t", 1) for row in query.stdout.splitlines())
    return host


def measure(probe, output):
    if platform.system() != "Linux" or platform.machine() != "x86_64":
        raise ValueError("the reference configuration requires Linux x86-64")
    version = subprocess.check_output(["valgrind", "--command-line-only=yes", "--version"],
                                      text=True).strip()
    if version != f"valgrind-{VERSION}":
        raise ValueError(f"expected Valgrind {VERSION}, got {version!r}")
    probe = probe.resolve(strict=True)
    catalog = workload_ids(subprocess.check_output([probe, "--list"], text=True))

    # A fresh directory keeps stale dumps from satisfying a missing workload.
    output.mkdir(parents=True)
    env = dict(os.environ, LD_BIND_NOW="1", LC_ALL="C")
    metadata = {"valgrind": version, "options": OPTIONS, "LD_BIND_NOW": env["LD_BIND_NOW"],
                "host": host_metadata(), "probe_sha256": hashlib.sha256(probe.read_bytes()).hexdigest()}
    (output / "runtime.json").write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")

    runs = []
    for index in (1, 2):
        directory = output / f"run-{index}"
        directory.mkdir()
        subprocess.run(["valgrind", *OPTIONS, probe], cwd=directory, env=env, check=True)
        counts = read_counts(directory, catalog)
        rows = [f"{workload}\t{counts[workload]}\n" for workload in sorted(counts)]
        (directory / "counts.tsv").write_text("workload_id\tIr\n" + "".join(rows), encoding="utf-8")
        runs.append(counts)
    compare_runs(*runs)
    print(f"{output}: identical Callgrind Ir counts for {len(catalog)} workloads in both runs")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--probe", type=Path, required=True, help="path to opentlv_callgrind")
    parser.add_argument("--output", type=Path, required=True, help="new directory for profiles and counts")
    args = parser.parse_args()
    try:
        measure(args.probe, args.output)
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"{error}\n")
