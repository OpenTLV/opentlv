"""Run all CTest cases, save their results and verify required suite coverage."""

import argparse
import json
import subprocess
import xml.etree.ElementTree as ET
from pathlib import Path


def run(build, config, reports):
    reports.mkdir(parents=True, exist_ok=True)
    junit = reports / "junit.xml"
    ctest = ["ctest", "--test-dir", build, "-C", config]
    subprocess.run([*ctest, "--output-on-failure", "--no-tests=error",
                    "--output-junit", junit, "--output-log", reports / "ctest.log"],
                   check=True)

    # A discovered case can still be disabled or skipped at runtime.
    passed = {case.attrib["name"] for case in ET.parse(junit).iter("testcase")
              if case.get("status") == "run" and case.find("skipped") is None}
    inventory = subprocess.check_output([*ctest, "--show-only=json-v1"], text=True)
    labels = set()
    for test in json.loads(inventory)["tests"]:
        if test["name"] in passed:
            for prop in test["properties"]:
                if prop["name"] == "LABELS":
                    labels.update(prop["value"])
    missing = {"core", "io", "integration"} - labels
    if missing:
        raise SystemExit("No passing CTest case for required suite(s): " + ", ".join(sorted(missing)))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build", type=Path)
    parser.add_argument("config", choices=("Debug", "Release"))
    parser.add_argument("reports", type=Path)
    args = parser.parse_args()
    run(args.build.resolve(), args.config, args.reports.resolve())
