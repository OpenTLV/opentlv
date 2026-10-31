# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Marek Cingel

"""Compare the ABI of the OpenTLV C shared library with the stored ABI baseline.

The baseline is an ``abidw`` dump committed as scripts/abi/baseline.abi (see
docs/development/abi-compatibility.md). The candidate library is built from the
working tree and compared with it using ``abidiff``. ``--update`` regenerates
the baseline from the working tree; do this only for an intentional ABI change
(before 1.0.0) or for a release (from 1.0.0).

Modes: ``report`` prints and stores the report and only warns about
incompatible changes; ``enforce`` fails on them; ``auto`` (default) reports
before 1.0.0 and enforces from 1.0.0.

Exit status: 0 no incompatible change (or report mode), 1 incompatible ABI
change in enforce mode, 2 the check itself failed.
"""

import argparse
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import xml.etree.ElementTree as ET

ABI_DIR = Path(__file__).resolve().parent
REPO_ROOT = ABI_DIR.parent.parent
DEFAULT_BASELINE = ABI_DIR / "baseline.abi"
SUPPRESSIONS = ABI_DIR / "suppressions.abignore"

# Keep machine-specific paths and source locations out of the stored dump.
DUMP_OPTIONS = ["--no-corpus-path", "--no-comp-dir-path", "--no-show-locs"]


class CheckError(Exception):
    pass


def current_major():
    """Major version of the newest stable tag reachable from HEAD (0 if none)."""
    result = subprocess.run(
        ["git", "-C", str(REPO_ROOT), "tag", "--list", "v[0-9]*", "[0-9]*", "--merged", "HEAD",
         "--sort=-version:refname"],
        capture_output=True, text=True, check=False,
    )
    versions = [tuple(map(int, tag.removeprefix("v").split(".")))
                for tag in result.stdout.split() if re.fullmatch(r"v?[0-9]+\.[0-9]+\.[0-9]+", tag)]
    return max(versions)[0] if versions else 0


def run(*args, **kwargs):
    subprocess.run(args, check=True, **kwargs)


def build(source, build_dir):
    """Build only the C shared library, in Debug so abidiff sees debug info."""
    run(
        "cmake", "-S", str(source), "-B", str(build_dir),
        "-DCMAKE_BUILD_TYPE=Debug", "-DOPENTLV_BUILD_SHARED_LIBS=ON",
        "-DOPENTLV_BUILD_CXX=OFF", "-DOPENTLV_BUILD_TESTS=OFF",
        "-DOPENTLV_BUILD_EXAMPLES=OFF",
        "-DOPENTLV_BUILD_BENCHMARKS=OFF", "-DOPENTLV_WARNINGS_AS_ERRORS=OFF",
        stdout=subprocess.DEVNULL,
    )
    run("cmake", "--build", str(build_dir), "--target", "tlv", "--parallel",
        stdout=subprocess.DEVNULL)


def find_library(build_dir, name="tlv"):
    for path in sorted(Path(build_dir).rglob(f"lib{name}.so*")):
        if path.is_file():
            return path
    raise CheckError(f"shared library lib{name}.so was not built")


def stage_headers(source, build_dir, dest):
    """Public headers of one build: source headers plus generated ones."""
    shutil.rmtree(dest, ignore_errors=True)
    shutil.copytree(Path(source) / "tlv" / "include", dest)
    shutil.copytree(Path(build_dir) / "generated" / "include", dest, dirs_exist_ok=True)


def suppression_args():
    return ["--suppressions", str(SUPPRESSIONS)] if SUPPRESSIONS.is_file() else []


def build_candidate(candidate_dir, work_dir, name="tlv"):
    """Build the candidate; return its library and staged public headers."""
    build(candidate_dir, work_dir / "candidate-build")
    headers = work_dir / "candidate-headers"
    stage_headers(candidate_dir, work_dir / "candidate-build", headers)
    registry = work_dir / "candidate-build/modules-Debug.json"
    if name == "tlv" and registry.is_file():
        # Compare the union of actual module binaries with the historic C ABI.
        # Once builtins are registered, tlv is empty. Ignore any stale legacy DSO
        # left in a reused build directory by an older source revision.
        entries = json.loads(registry.read_text(encoding="utf-8"))
        extracted_builtins = any(entry["key"] == "asn1" for entry in entries)
        libraries = ([] if extracted_builtins else
                     sorted((work_dir / "candidate-build/tlv").glob("libtlv.so")))
        libraries += [Path(entry["library"]) for entry in entries
                      if "library" in entry]
        if libraries:
            return libraries, headers
    return find_library(work_dir / "candidate-build", name), headers


def dump_abi(library, headers, out_file):
    """Write the public ABI of the library as an abidw dump, without machine-specific paths."""
    out_file.parent.mkdir(parents=True, exist_ok=True)
    libraries = library if isinstance(library, list) else [library]
    group = (["--added-binaries-dir", str(libraries[0].parent),
              "--add-binaries", ",".join(path.name for path in libraries[1:])]
             if len(libraries) > 1 else [])
    run("abidw", "--headers-dir", str(headers), "--drop-private-types",
        *DUMP_OPTIONS, *suppression_args(), *group, "--out-file", str(out_file), str(libraries[0]))
    flatten_corpus_group(out_file)
    text = out_file.read_text(encoding="utf-8")
    # abidw silently emits a symbols-only dump when debug info is missing.
    if "<function-decl" not in text and "<var-decl" not in text:
        raise CheckError("the ABI dump has no public declarations; was debug info built?")
    # Translation unit paths are absolute build paths; keep them relative to the repository.
    text = re.sub(r"""path=(['"])[^'"]*?/((?:tlv|generated)/[^'"]*)\1""", r"path='\2'", text)
    out_file.write_text(text, encoding="utf-8", newline="\n")


def flatten_corpus_group(out_file):
    """Compare declarations across module moves without comparing DSO names.

    abidw generates the whole group in one context, so its type IDs are shared
    across the member corpora. Preserve those IDs and declarations verbatim in
    one corpus. Ownership and DSO exports remain the isolation gate's concern.
    """
    root = ET.parse(out_file).getroot()
    if root.tag != "abi-corpus-group":
        return
    corpus = ET.Element("abi-corpus", version=root.get("version", "2.1"),
                        architecture=root.get("architecture", ""))
    for kind in ("elf-function-symbols", "elf-variable-symbols"):
        symbols = ET.SubElement(corpus, kind)
        seen = set()
        for member in root.findall("abi-corpus"):
            for symbol in member.findall(kind + "/elf-symbol"):
                name = symbol.get("name")
                if name in seen:
                    raise CheckError(f"duplicate public symbol in ABI modules: {name}")
                seen.add(name)
                symbols.append(symbol)
    for member in root.findall("abi-corpus"):
        corpus.extend(member.findall("abi-instr"))
    # libabigail detects ABIXML by its root, not an XML declaration.
    ET.ElementTree(corpus).write(out_file, encoding="utf-8")


def abidiff(args, baseline_file, candidate_file, *extra):
    result = subprocess.run(
        ("abidiff", *args, *extra, str(baseline_file), str(candidate_file)),
        capture_output=True, text=True, check=False,
    )
    return result.returncode, result.stdout + result.stderr


def write_step_summary(summary):
    """Append the comparison summary to the GitHub Actions step summary, if any."""
    path = os.environ.get("GITHUB_STEP_SUMMARY")
    if path:
        with open(path, "a", encoding="utf-8") as handle:
            handle.write(f"### C ABI comparison\n\n```\n{summary}```\n")


def compare(baseline_file, candidate_file, output_dir, mode):
    args = suppression_args()

    # abidiff's exit status is a bit mask: 1 tool error, 2 usage error, 4 ABI
    # change, 8 incompatible ABI change. Bit 8 is only set for some changes (for
    # example removed functions), not for changed signatures or type layouts, so
    # the verdict comes from a run with --no-added-syms: any remaining
    # difference is a change to or removal of existing public ABI. Added
    # exported functions are compatible.
    verdict, _ = abidiff(args, baseline_file, candidate_file, "--no-added-syms")
    report_status, report = abidiff(
        args, baseline_file, candidate_file,
        "--leaf-changes-only", "--impacted-interfaces", "--no-show-locs",
    )
    _, summary = abidiff(args, baseline_file, candidate_file, "--stat")
    if not summary.strip():
        # abidiff --stat prints nothing when the ABI is unchanged.
        summary = (
            "Functions changes summary: 0 Removed, 0 Changed, 0 Added function\n"
            "Variables changes summary: 0 Removed, 0 Changed, 0 Added variable\n"
        )
    output_dir.mkdir(parents=True, exist_ok=True)
    (output_dir / "abidiff.txt").write_text(report, encoding="utf-8")
    (output_dir / "abidiff-summary.txt").write_text(summary, encoding="utf-8")

    write_step_summary(summary)

    if (verdict | report_status) & 3:
        print(report, file=sys.stderr)
        raise CheckError(f"abidiff failed (status {verdict}/{report_status})")

    print("--- ABI summary ---")
    print(summary)
    if report_status == 0:
        print("No ABI changes.")
    else:
        print("--- Affected symbols and types ---")
        print(report)
    print(f"Report: {output_dir / 'abidiff.txt'}")

    if verdict & 12:
        message = "Incompatible public C ABI change detected against the ABI baseline."
        if mode == "enforce":
            print(f"::error::{message}")
            print(message, file=sys.stderr)
            return 1
        print(f"::warning::{message} Not blocking before 1.0.0; document it in the pull request.")
        print(f"{message} (report mode: not blocking)")
    elif report_status:
        print("Compatible ABI changes only (added exported functions).")
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--baseline-file", type=Path, default=DEFAULT_BASELINE,
                        help="Stored ABI baseline (abidw dump)")
    parser.add_argument("--candidate-dir", type=Path, default=REPO_ROOT, help="Candidate source tree")
    parser.add_argument("--library", default="tlv",
                        choices=("tlv", "opentlv_core", "opentlv_io", "opentlv_document",
                                 "opentlv_generator", "opentlv_query", "opentlv_query_document",
                                 "opentlv_codec", "opentlv_query_codec", "opentlv_schema",
                                 "opentlv_schema_number", "opentlv_schema_query",
                                 "opentlv_schema_query_document", "opentlv_codec_structure",
                                 "opentlv_asn1", "opentlv_emv", "opentlv_bluetooth",
                                 "opentlv_nfc", "opentlv_dhcp", "opentlv_lldp"),
                        help="tlv compares the complete C module ABI; other names need a matching per-library baseline")
    parser.add_argument("--update", action="store_true",
                        help="Regenerate the baseline from the candidate instead of comparing")
    parser.add_argument("--mode", choices=("auto", "report", "enforce"), default="auto")
    parser.add_argument("--work-dir", type=Path, default=REPO_ROOT / "build" / "abi")
    parser.add_argument("--output-dir", type=Path)
    options = parser.parse_args()

    try:
        for tool in ("abidiff", "abidw", "cmake"):
            if shutil.which(tool) is None:
                raise CheckError(f"{tool} not found (install abigail-tools)")
        work_dir = options.work_dir.resolve()
        library, headers = build_candidate(options.candidate_dir.resolve(), work_dir, options.library)

        if options.update:
            dump_abi(library, headers, options.baseline_file.resolve())
            print(f"Updated ABI baseline: {options.baseline_file}")
            return 0
        candidate_file = work_dir / "candidate.abi"
        dump_abi(library, headers, candidate_file)

        if not options.baseline_file.is_file():
            raise CheckError(f"ABI baseline {options.baseline_file} not found; "
                             "create it with --update")
        mode = options.mode
        if mode == "auto":
            mode = "enforce" if current_major() >= 1 else "report"
        print(f"ABI check: candidate vs {options.baseline_file.name} (mode: {mode})")
        output_dir = (options.output_dir or work_dir / "report").resolve()
        return compare(options.baseline_file.resolve(), candidate_file, output_dir, mode)
    except (CheckError, subprocess.CalledProcessError) as error:
        print(f"check: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
