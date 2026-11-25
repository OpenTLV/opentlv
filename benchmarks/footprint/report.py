# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Marek Cingel
"""Report on-disk bytes and tool-reported section sizes for built artifacts."""

import argparse
import json
import os
import re
import subprocess
from collections import Counter
from pathlib import Path


def tool_output(command):
    return subprocess.check_output(command, env=dict(os.environ, LC_ALL="C", VSLANG="1033"),
                                   text=True, errors="replace")


def sysv_sections(output):
    # size -A emits one table per archive member. Validate each total before
    # combining like-named sections across the archive.
    sections = Counter()
    member_total = None
    for line in output.splitlines():
        fields = line.split()
        if fields == ["section", "size", "addr"] and member_total is None:
            member_total = 0
        elif member_total is not None:
            if len(fields) == 2 and fields[0] == "Total":
                if int(fields[1]) != member_total:
                    raise ValueError("section sizes disagree with size's member total")
                member_total = None
            else:
                name, count, address = line.rsplit(None, 2)
                if not count.isdecimal() or not address.isdecimal():
                    raise ValueError(f"invalid size row: {line}")
                sections[name.strip()] += int(count)
                member_total += int(count)
    if not sections or member_total is not None:
        raise ValueError("size returned an incomplete section table")
    return dict(sorted(sections.items()))


def dumpbin_sections(output):
    sections = {}
    lines = output.splitlines()
    summary = next((i for i, line in enumerate(lines) if line.strip() == "Summary"), len(lines))
    for line in lines[summary + 1:]:
        if line.strip():
            count, name = line.strip().split(None, 1)
            sections[name] = int(count, 16)
    if not sections:
        raise ValueError("dumpbin returned an empty section summary")
    return sections


def report(args):
    build = json.loads(args.metadata.read_text(encoding="utf-8"))
    tool = args.size_tool
    method = None
    version = None
    if tool:
        if tool.stem.lower() == "dumpbin":
            method = "dumpbin /SUMMARY"
        else:
            banner = tool_output([tool, "--version"])
            version = next((line.strip() for line in banner.splitlines()
                            if re.search(r"^GNU size\b|\bLLVM version \d", line, re.IGNORECASE)), None)
            if version is None:
                raise ValueError(f"{tool}: expected GNU size, llvm-size or dumpbin")
            method = "size --format=sysv --radix=10"

    artifacts = {}
    for role, filename in args.artifact:
        path = Path(filename)
        sections = None
        if method == "dumpbin /SUMMARY":
            output = tool_output([tool, "/SUMMARY", path])
            version = output.splitlines()[0].strip()
            sections = dumpbin_sections(output)
        elif method:
            sections = sysv_sections(tool_output([tool, "--format=sysv", "--radix=10", path]))
        artifacts[role] = {"file": path.name, "file_bytes": path.stat().st_size,
                           "section_bytes": sections}

    result = {"build": build, "section_tool": {"path": str(tool) if tool else None,
              "method": method, "version": version}, "artifacts": artifacts}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(f"{args.output}: measured {len(artifacts)} artifacts"
          + (f" with {method}" if method else " (section tool unavailable)"))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--metadata", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--size-tool", type=Path)
    parser.add_argument("--artifact", nargs=2, action="append", required=True, metavar=("ROLE", "FILE"))
    try:
        report(parser.parse_args())
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"{error}\n")
