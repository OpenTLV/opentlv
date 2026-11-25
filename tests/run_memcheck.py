"""Check Fixed TLV Debug contract binaries and two negative controls with Memcheck."""

import argparse
import os
import subprocess
import tempfile
import xml.etree.ElementTree as ET
from pathlib import Path


ERROR_EXIT = 97
CLEAN_SUMMARY = "ERROR SUMMARY: 0 errors from 0 contexts"
CONTROL = """\
#include <stdlib.h>
#include <string.h>
#include <valgrind/memcheck.h>

int main(int argc, char **argv)
{
    if (argc != 2) return 1;
    unsigned char *byte = malloc(1);
    if (byte == NULL) return 1;
    if (strcmp(argv[1], "leak") == 0) return 0;
    if (strcmp(argv[1], "undefined") != 0) *byte = 0;
    (void)VALGRIND_CHECK_MEM_IS_DEFINED(byte, 1);
    free(byte);
    return 0;
}
"""


def memcheck(log, command):
    return subprocess.run([
        "valgrind", "--command-line-only=yes", "--tool=memcheck", f"--error-exitcode={ERROR_EXIT}",
        "--track-origins=yes", "--leak-check=full",
        "--show-leak-kinds=all", "--errors-for-leak-kinds=definite,indirect,possible",
        f"--log-file={log}", *command,
    ]).returncode


def run(binaries, reports):
    # Refuse stale reports that might otherwise satisfy a missing run.
    reports.mkdir(parents=True)
    with tempfile.TemporaryDirectory() as directory:
        probe = Path(directory) / "control"
        source = probe.with_suffix(".c")
        source.write_text(CONTROL, encoding="utf-8")
        subprocess.run([os.environ.get("CC", "cc"), "-std=c99", "-g", "-O0", source, "-o", probe],
                       check=True)
        for mode, expected, diagnostic in (
            ("clean", 0, CLEAN_SUMMARY),
            ("undefined", ERROR_EXIT, "Uninitialised byte(s) found during client check request"),
            ("leak", ERROR_EXIT, "definitely lost: 1 bytes in 1 blocks"),
        ):
            log = reports / f"control-{mode}.log"
            status = memcheck(log, [probe, mode])
            summary = CLEAN_SUMMARY if expected == 0 else "ERROR SUMMARY: 1 errors from 1 contexts"
            report = log.read_text(encoding="utf-8")
            if status != expected or diagnostic not in report or summary not in report:
                raise SystemExit(f"{mode}: expected status {expected} and {diagnostic!r}, "
                                 f"got status {status}. See {log}")
            print(f"Control {mode}: status {status}, found {diagnostic!r}", flush=True)

    for suite in ("core", "io", "integration"):
        log = reports / f"{suite}.log"
        junit = reports / f"{suite}.xml"
        binary = binaries / f"opentlv_{suite}_tests"
        status = memcheck(log, [binary, "--gtest_filter=*", f"--gtest_output=xml:{junit}"])
        if status or CLEAN_SUMMARY not in log.read_text(encoding="utf-8"):
            raise SystemExit(f"{suite}: expected a clean Memcheck run, got status {status}. See {log}")
        completed = ET.parse(junit).findall(".//testcase[@result='completed']")
        if not completed:
            raise SystemExit(f"{suite}: no completed GoogleTest cases in {junit}")
        print(f"{suite}: {len(completed)} cases completed with zero Memcheck errors", flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binaries", type=Path, help="directory containing the three unsanitized Debug test binaries")
    parser.add_argument("reports", type=Path, help="new directory for Memcheck logs and GoogleTest reports")
    args = parser.parse_args()
    try:
        run(args.binaries.resolve(), args.reports.resolve())
    except (OSError, ET.ParseError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"{error}\n")
