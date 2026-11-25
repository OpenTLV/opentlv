# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Marek Cingel
"""Compare two Fixed TLV performance evidence directories on the same reference host."""

import argparse
import csv
import json
import re
import statistics
import sys
from dataclasses import dataclass
from pathlib import Path

from check_results import checked_samples
from performance_build import TARGETS
from run_callgrind import OPTIONS, VERSION, workload_ids


# The candidate digest is the result of applying clang-format 18.1.8 to
# the C/C++ files in this reference's protocol. See performance-baseline.md.
FORMATTED_REFERENCE = "8753cc3384cd62533a41ba40dc932f3681da9b4b"
FORMAT_ONLY_PROTOCOLS = (
    "39f7a5d54e1f9a4664d20d29b8e9196ddb16a90298b74562aaa5aa81defce4d9",
    "59415b783d5d5f3b6629244a2732fdfc2c84c189f893834c35f519635ca73b41",
)


@dataclass
class Evidence:
    configuration: dict
    metrics: dict
    timing_median_ns: dict
    probe_sha256: str
    protocol_sha256: str


def read_json(path):
    def invalid_number(value):
        raise ValueError(f"{path}: invalid JSON number {value}")

    def unique_object(pairs):
        result = {}
        for name, value in pairs:
            if name in result:
                raise ValueError(f"{path}: duplicate JSON key {name!r}")
            result[name] = value
        return result

    with path.open(encoding="utf-8") as stream:
        return json.load(stream, object_pairs_hook=unique_object, parse_constant=invalid_number)


def same(name, reference, actual):
    if type(reference) is not type(actual):
        raise ValueError(f"{name}: expected {type(reference).__name__}, got {type(actual).__name__}")
    if isinstance(reference, dict):
        if reference.keys() != actual.keys():
            different = sorted(reference.keys() ^ actual.keys())
            raise ValueError(f"{name}: different or missing keys: {', '.join(different)}")
        for key in reference:
            same(f"{name}.{key}", reference[key], actual[key])
    elif reference != actual:
        raise ValueError(f"{name}: expected {reference!r}, got {actual!r}")


def strings(obj, required, empty_allowed=()):
    for key in (*required, *empty_allowed):
        value = obj[key]
        if not isinstance(value, str) or (key in required and not value.strip()):
            raise ValueError(f"invalid metadata field {key!r}")


def target_settings(path):
    modes = read_json(path)
    same("build settings", sorted(TARGETS), sorted(modes))
    for mode, targets in TARGETS.items():
        same(f"{mode} targets", sorted(targets), sorted(modes[mode]))
        for target in targets:
            settings = modes[mode][target]
            groups = settings["compile"]
            link = settings["link"]
            if not isinstance(link, list) or not all(isinstance(flag, str) for flag in link):
                raise ValueError(f"{mode}/{target}: invalid link options")
            if not isinstance(groups, list) or not groups:
                raise ValueError(f"{mode}/{target}: missing compile groups")
            for group in groups:
                strings(group, ("language", "flags"))
                defines, includes = group["defines"], group["includes"]
                if not isinstance(defines, list) or not all(isinstance(item, str) for item in defines):
                    raise ValueError(f"{mode}/{target}: invalid compile definitions")
                if not isinstance(includes, list):
                    raise ValueError(f"{mode}/{target}: invalid include paths")
                for include in includes:
                    strings(include, ("path",))
                    if type(include["system"]) is not bool:
                        raise ValueError(f"{mode}/{target}: invalid system include setting")
    return modes


def build_settings(build, revision, configuration, linkage):
    expected = {"schema_version": 1, "revision": revision, "dirty": False,
                "configuration": configuration, "library_linkage": linkage,
                "system": "Linux", "processor": "x86_64", "pointer_bits": 64,
                "test_coverage": False, "interprocedural_optimization": False}
    for key, value in expected.items():
        same(key, value, build[key])
    strings(build, ("generator",), ("generator_platform", "osx_architectures", "linker_flags",
            "configuration_linker_flags", "static_linker_flags", "configuration_static_linker_flags",
            "shared_linker_flags", "configuration_shared_linker_flags"))
    strings(build["tools"], ("cmake", "git"))
    for language in ("C", "CXX"):
        strings(build["compilers"][language], ("path", "id", "version", "configuration_flags"),
                ("architecture", "target", "flags"))
    return {key: value for key, value in build.items() if key != "revision"}


def catalog(path, size):
    ids = workload_ids(path.read_text(encoding="utf-8"))
    if len(ids) != size:
        raise ValueError(f"{path}: expected {size} v1 workload IDs, got {len(ids)}")
    return ids


def read_counts(path, ids):
    counts = {}
    with path.open(encoding="utf-8", newline="") as stream:
        rows = csv.reader(stream, delimiter="\t")
        same(str(path), ["workload_id", "Ir"], next(rows, None))
        for workload, value in rows:
            if workload not in ids or workload in counts:
                raise ValueError(f"{path}: unknown or duplicate workload {workload!r}")
            if not re.fullmatch(r"[1-9][0-9]*", value):
                raise ValueError(f"{path}: invalid Ir count for {workload}")
            counts[workload] = int(value)
    if ids != counts.keys():
        raise ValueError(f"{path}: missing Ir counts for {', '.join(sorted(ids - counts.keys()))}")
    return counts


def callgrind(directory, revision):
    metadata = directory / "callgrind-build/benchmarks/RelWithDebInfo/metadata.json"
    settings = build_settings(read_json(metadata), revision, "RelWithDebInfo", "static")
    directory = directory / "callgrind-reports"
    runtime = read_json(directory / "runtime.json")
    for key, value in (("valgrind", f"valgrind-{VERSION}"), ("options", list(OPTIONS)), ("LD_BIND_NOW", "1")):
        same(key, value, runtime[key])
    host = runtime["host"]
    strings(host, ("kernel", "machine"))
    same("host.machine", settings["processor"], host["machine"])
    strings(host["cpu"], ("model name", "flags"))
    if not isinstance(host["libc"], list) or len(host["libc"]) != 2 or not all(
            isinstance(value, str) and value for value in host["libc"]):
        raise ValueError("expected libc implementation and version")
    if not isinstance(host["os_release"], list) or not any(
            isinstance(line, str) and line.startswith("ID=") for line in host["os_release"]):
        raise ValueError("missing operating system identity")
    # Ubuntu may append a multiarch qualifier to library package names.
    if not isinstance(host["dpkg_versions"], dict):
        raise ValueError("missing runtime package versions")
    packages = {name.split(":", 1)[0]: version for name, version in host["dpkg_versions"].items()}
    strings(packages, ("libc6", "libstdc++6"))
    probe_digest = runtime.pop("probe_sha256")
    if not re.fullmatch(r"[0-9a-f]{64}", probe_digest):
        raise ValueError("invalid Callgrind probe_sha256")
    ids = catalog(directory / "workloads.txt", 56)
    counts = read_counts(directory / "run-1" / "counts.tsv", ids)
    same("repeat Ir counts", counts, read_counts(directory / "run-2" / "counts.tsv", ids))
    return {"build": settings, "runtime": runtime}, counts, probe_digest


def footprint(path, revision, linkage):
    report = read_json(path)
    build = build_settings(report["build"], revision, "Release", linkage)
    tool = report["section_tool"]
    strings(tool, ("method", "path", "version"))
    sizes = {}
    files = {}
    roles = ("core", "io", "c99-write", "cpp17-write")
    same("artifact roles", sorted(roles), sorted(report["artifacts"]))
    for role in roles:
        artifact = report["artifacts"][role]
        strings(artifact, ("file",))
        files[role] = artifact["file"]
        name = f"footprint/{linkage}/{role}"
        size = artifact["file_bytes"]
        if type(size) is not int or size <= 0:
            raise ValueError(f"{name}: expected positive file_bytes")
        sizes[f"{name}/file_bytes"] = size
        sections = artifact["section_bytes"]
        if not isinstance(sections, dict) or ".text" not in sections:
            raise ValueError(f"{name}: missing section sizes")
        for section, count in sections.items():
            if not section or type(count) is not int or count < 0:
                raise ValueError(f"{name}: invalid size for section {section!r}")
            sizes[f"{name}/section_bytes/{section}"] = count
    return {"build": build, "section_tool": tool, "files": files}, sizes


def timing(directory, revision, counted_workloads):
    settings = build_settings(read_json(directory / "metadata.json"), revision, "Release", "static")
    ids = catalog(directory / "workloads.txt", 3584)
    if not counted_workloads <= ids:
        raise ValueError("Callgrind workload missing from timing catalog")
    report = read_json(directory / "results.json")
    samples = checked_samples(report, ids)
    context = report["context"]
    same("Google Benchmark JSON schema", 1, context["json_schema_version"])
    same("Google Benchmark version", "v" + settings["tools"]["google_benchmark"], context["library_version"])
    same("Google Benchmark build", "release", context["library_build_type"])
    strings(context, ("host_name",))
    if type(context["num_cpus"]) is not int or context["num_cpus"] < 1:
        raise ValueError("invalid Google Benchmark num_cpus")
    for row in report["benchmarks"]:
        same("timing threads", 1, row["threads"])
    # checked_samples verified that the repetitions of each workload agree
    # on their iteration count. The CI protocol fixes it across workloads too.
    sample_shape = {(rows[0]["iterations"], len(rows)) for rows in samples.values()}
    if len(sample_shape) != 1:
        raise ValueError("expected a fixed iteration and repetition count for all timing workloads")
    iterations, repetitions = sample_shape.pop()
    config = {"build": settings, "host_name": context["host_name"], "num_cpus": context["num_cpus"],
              "iterations": iterations, "repetitions": repetitions}
    return config, {workload: statistics.median(row["real_time"] for row in rows.values())
                    for workload, rows in samples.items()}


def evidence(directory, revision):
    if not re.fullmatch(r"[0-9a-f]{40}", revision):
        raise ValueError("expected a full Git revision")
    protocol = (directory / "protocol.sha256").read_text(encoding="ascii").strip()
    if not re.fullmatch(r"[0-9a-f]{64}", protocol):
        raise ValueError("invalid protocol.sha256")
    probe_config, counts, probe_digest = callgrind(directory / "callgrind", revision)
    timing_config, observations = timing(directory / "timing", revision, counts.keys())
    config = {"callgrind": probe_config, "timing": timing_config,
              "targets": target_settings(directory / "build-settings.json")}
    metrics = {f"Ir/{workload}": value for workload, value in counts.items()}
    for linkage in ("static", "shared"):
        path = directory / "footprint" / f"{linkage}.json"
        config[linkage], sizes = footprint(path, revision, linkage)
        metrics.update(sizes)
    return Evidence(config, metrics, observations, probe_digest, protocol)


def differences(kind, reference, candidate):
    if reference.keys() != candidate.keys():
        different = sorted(reference.keys() ^ candidate.keys())
        raise ValueError(f"incomparable {kind}: different or missing measurements: {', '.join(different)}")
    return {name: {"baseline": value, "candidate": candidate[name], "delta": candidate[name] - value}
            for name, value in sorted(reference.items())}


def compare(args, result):
    reports = []
    for role in ("baseline", "candidate"):
        try:
            reports.append(evidence(getattr(args, role), result[f"{role}_revision"]))
        except (ValueError, TypeError, KeyError, OSError, OverflowError, IndexError, StopIteration, csv.Error) as error:
            raise ValueError(f"{role}: {error}") from error
    baseline, candidate = reports
    protocols = (baseline.protocol_sha256, candidate.protocol_sha256)
    if result["baseline_revision"] != FORMATTED_REFERENCE or protocols != FORMAT_ONLY_PROTOCOLS:
        same("configuration.protocol_sha256", *protocols)
    same("configuration", baseline.configuration, candidate.configuration)
    metrics = differences("deterministic metrics", baseline.metrics, candidate.metrics)
    observations = differences("timing workloads", baseline.timing_median_ns, candidate.timing_median_ns)
    regressions = [name for name, values in metrics.items() if values["delta"] > 0]
    result.update(status="regression" if regressions else "pass",
                  configuration={"protocol_sha256": baseline.protocol_sha256, **baseline.configuration},
                  probe_sha256={"baseline": baseline.probe_sha256, "candidate": candidate.probe_sha256},
                  metrics=metrics, regressions=regressions, timing_median_ns=observations)
    if baseline.protocol_sha256 != candidate.protocol_sha256:
        result["protocol_sha256"] = {"baseline": baseline.protocol_sha256, "candidate": candidate.protocol_sha256}


def summary(result):
    lines = [f"## Fixed TLV performance comparison — {result['status']}", "",
             f"Baseline `{result['baseline_revision']}`  ", f"Candidate `{result['candidate_revision']}`", ""]
    if "error" in result:
        lines.append(result["error"])
    else:
        lines.extend([f"{len(result['metrics'])} deterministic metrics compared, "
                      f"{len(result['regressions'])} increased. Allowed increase is zero.",
                      f"Median wall times for {len(result['timing_median_ns'])} workloads are recorded "
                      "in `comparison.json` as observations."])
        if result["regressions"]:
            lines.extend(["", "| Increased metric | Baseline | Candidate | Increase |", "| --- | ---: | ---: | ---: |"])
            for name in result["regressions"]:
                values = result["metrics"][name]
                lines.append(f"| `{name}` | {values['baseline']} | {values['candidate']} | +{values['delta']} |")
    return "\n".join(lines) + "\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for role in ("baseline", "candidate"):
        parser.add_argument(f"--{role}", type=Path, required=True)
        parser.add_argument(f"--{role}-revision", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    result = {"schema_version": 1, "baseline_revision": args.baseline_revision,
              "candidate_revision": args.candidate_revision}
    try:
        compare(args, result)
    except ValueError as error:
        result.update(status="invalid", error=str(error))
    args.output.mkdir(parents=True, exist_ok=True)
    with (args.output / "comparison.json").open("w", encoding="utf-8", newline="\n") as stream:
        json.dump(result, stream, indent=2)
        stream.write("\n")
    message = summary(result)
    with (args.output / "summary.md").open("w", encoding="utf-8", newline="\n") as stream:
        stream.write(message)
    print(message)
    return 0 if result["status"] == "pass" else 1


if __name__ == "__main__":
    sys.exit(main())
