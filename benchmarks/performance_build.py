# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Marek Cingel
"""Read effective options for measured targets from the CMake File API."""

import json
import re


LIBRARIES = ("opentlv_core", "opentlv_io")
CONSUMERS = LIBRARIES + ("opentlv_footprint_c", "opentlv_footprint_cpp")
TARGETS = {
    "static": CONSUMERS,
    "shared": CONSUMERS,
    "callgrind": LIBRARIES + ("opentlv_benchmark_workloads", "opentlv_callgrind"),
    "timing": LIBRARIES + ("opentlv_benchmark_workloads", "opentlv_check_fixtures", "opentlv_benchmark", "benchmark"),
}


def request_codemodel(build):
    query = build / ".cmake/api/v1/query"
    query.mkdir(parents=True, exist_ok=True)
    (query / "codemodel-v2").touch()


def build_signature(build, source, targets):
    # Worktrees and build directories can occur inside include options, rpaths
    # and generated-header paths. Preserve all other paths and flag ordering.
    roots = {build.resolve().as_posix(): "<build>", source.resolve().as_posix(): "<source>"}
    paths = "|".join(re.escape(path) for path in sorted(roots, key=len, reverse=True))
    pattern = re.compile(r"(" + paths + r")(?=$|[/,;:'\"\s])")

    def normalize(value):
        return pattern.sub(lambda match: roots[match[0]], value)

    reply = build / ".cmake/api/v1/reply"
    indexes = sorted(reply.glob("index-*.json"))
    if not indexes:
        raise ValueError(f"{reply}: no CMake File API reply")

    def load(name):
        return json.loads((reply / name).read_text(encoding="utf-8"))

    index = load(indexes[-1].name)
    model = load(index["reply"]["codemodel-v2"]["jsonFile"])
    configuration, = model["configurations"]  # each measurement uses a single-config build
    names = {entry["name"]: entry["jsonFile"] for entry in configuration["targets"]}
    signature = {}
    for name in targets:
        target = load(names[name])
        groups = []
        for group in target["compileGroups"]:
            settings = {
                "language": group["language"],
                "flags": " ".join(normalize(row["fragment"]) for row in group.get("compileCommandFragments", [])),
                "defines": [normalize(row["define"]) for row in group.get("defines", [])],
                "includes": [{"path": normalize(row["path"]), "system": row.get("isSystem", False)}
                             for row in group.get("includes", [])],
            }
            if settings not in groups:
                groups.append(settings)
        command = target["link"] if "link" in target else target["archive"]
        signature[name] = {"compile": sorted(groups, key=json.dumps),
                           "link": [normalize(row["fragment"]) for row in command.get("commandFragments", [])]}
    return signature
