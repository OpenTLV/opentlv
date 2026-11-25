"""Run all CTest cases, save their results and verify required suite coverage."""

import argparse
import json
import subprocess
import xml.etree.ElementTree as ET
from pathlib import Path


CPP_EQUIVALENCE_CASES = {
    "CppCore.ValidatesBorrowedValuesAndPreservesTheDestinationOnFailure",
    "CppFixed.MeasuresElementsAndPreservesSizeOnError",
    "WireLayouts/FixedLength.ReadsExactlyOneField/BigEndian8",
    "WireLayouts/FixedLength.InvalidWritesPreserveTheBufferAndCount/LittleEndian1",
    "CppReader.OneShotMatchesCAtEveryPrefix",
    "CppWriter.AppendsEmptyAndNonEmptyValuesAcrossAnInvalidElement",
} | {
    f"WireLayouts/FixedRoundTrip.{case}/{order}{width}"
    for case in ("RoundTripsBoundedBinaryValuesAtLengthBoundaries", "CommitsOnlyCompleteRecordsInAnAdjacentStream")
    for order in ("BigEndian", "LittleEndian")
    for width in range(1, 9)
}

NATIVE_SIZE_CASES = {
    "abi-layout",
    "CoreSize.ConvertsRepresentableCountsToNative",
    "CoreValue.ChecksTheLargestRepresentableSizeWithoutReadingThePayload",
    "FixedElementSize.EnforcesWireAndNativePayloadLimitsForEachWidth",
    "FixedMeasurement.MeasuresAnExactNativeHeaderAndRejectsTheNextWidth",
    "CoreSize.AddsCountsThroughTheLogicalLimit",
    "CoreSize.ReportsOverflowAndPreservesOutput",
    "ReaderBounds.ComparesHugeLengthsBeforeAnyNativeSizeConversion",
    "IOWrite.SizingQueriesValidateWireAndNativeLimits",
}

ARCHITECTURE_CASES = {
    "x86": NATIVE_SIZE_CASES | {
        "CoreSize.RejectsUint64MaxOnX86",
        "CoreSize.RejectsTheFirstUnrepresentableNativeCountAndPreservesOutput",
        "CoreValue.RejectsANonNativeSizeAndPreservesTheDestination",
        "FixedElementSize.RejectsAWireLengthAboveTheNativeSizeRange",
    },
    "x64": NATIVE_SIZE_CASES | {"CoreSize.ConvertsCountsAboveUint32MaxOnX64"},
}


def run(build, config, reports, architecture=None):
    reports.mkdir(parents=True, exist_ok=True)
    toolchain = (build / "tests" / "toolchain.txt").read_text(encoding="utf-8")
    (reports / "toolchain.txt").write_text(toolchain, encoding="utf-8")
    print(toolchain, end="", flush=True)
    junit = reports / "junit.xml"
    ctest = ["ctest", "--test-dir", build, "-C", config]
    subprocess.run([*ctest, "--output-on-failure", "--no-tests=error",
                    "--output-junit", junit, "--output-log", reports / "ctest.log"],
                   check=True)

    # Count only executed cases without a JUnit skip, failure or error.
    passed = {case.attrib["name"] for case in ET.parse(junit).iter("testcase")
              if case.get("status") == "run"
              and all(case.find(tag) is None for tag in ("skipped", "failure", "error"))}
    inventory = json.loads(subprocess.check_output([*ctest, "--show-only=json-v1"], text=True))["tests"]
    print(f"{config}: {len(passed)}/{len(inventory)} discovered CTest cases passed")
    labels = set()
    for test in inventory:
        if test["name"] in passed:
            for prop in test["properties"]:
                if prop["name"] == "LABELS":
                    labels.update(prop["value"])
    missing = {"core", "io", "integration"} - labels
    if missing:
        raise SystemExit("No passing CTest case for required suite(s): " + ", ".join(sorted(missing)))
    missing = CPP_EQUIVALENCE_CASES - passed
    if missing:
        raise SystemExit("Missing passing C/C++ equivalence cases: " + ", ".join(sorted(missing)))
    print(f"Verified {len(CPP_EQUIVALENCE_CASES)} C/C++ equivalence cases")
    if architecture:
        missing = ARCHITECTURE_CASES[architecture] - passed
        if missing:
            raise SystemExit(f"Missing passing {architecture} cases: " + ", ".join(sorted(missing)))
        print(f"Verified {architecture} ABI layout and native-size boundary cases")
    return passed


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build", type=Path)
    parser.add_argument("config", choices=("Debug", "Release"))
    parser.add_argument("reports", type=Path)
    profiles = parser.add_mutually_exclusive_group()
    for architecture in ARCHITECTURE_CASES:
        profiles.add_argument(f"--require-{architecture}", dest="architecture", action="store_const",
                              const=architecture, help=f"require passing {architecture} ABI and native-size cases")
    args = parser.parse_args()
    run(args.build.resolve(), args.config, args.reports.resolve(), args.architecture)
