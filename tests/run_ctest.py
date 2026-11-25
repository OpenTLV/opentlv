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

X86_REQUIRED_CASES = {
    "CoreSize.ConvertsRepresentableCountsToNative",
    "CoreSize.RejectsUint64MaxOnX86",
    "CoreSize.RejectsTheFirstUnrepresentableNativeCountAndPreservesOutput",
    "CoreValue.RejectsANonNativeSizeAndPreservesTheDestination",
    "FixedElementSize.RejectsAWireLengthAboveTheNativeSizeRange",
    "FixedMeasurement.MeasuresAnExactNativeHeaderAndRejectsTheNextWidth",
    "CoreSize.AddsCountsThroughTheLogicalLimit",
    "CoreSize.ReportsOverflowAndPreservesOutput",
    "ReaderBounds.ComparesHugeLengthsBeforeAnyNativeSizeConversion",
    "IOWrite.SizingQueriesValidateWireAndNativeLimits",
}


def run(build, config, reports):
    reports.mkdir(parents=True, exist_ok=True)
    junit = reports / "junit.xml"
    ctest = ["ctest", "--test-dir", build, "-C", config]
    subprocess.run([*ctest, "--output-on-failure", "--no-tests=error",
                    "--output-junit", junit, "--output-log", reports / "ctest.log"],
                   check=True)

    # Count only executed cases without a JUnit skip, failure or error.
    passed = {case.attrib["name"] for case in ET.parse(junit).iter("testcase")
              if case.get("status") == "run"
              and all(case.find(tag) is None for tag in ("skipped", "failure", "error"))}
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
    missing = CPP_EQUIVALENCE_CASES - passed
    if missing:
        raise SystemExit("Missing passing C/C++ equivalence cases: " + ", ".join(sorted(missing)))
    print(f"Verified {len(CPP_EQUIVALENCE_CASES)} C/C++ equivalence cases")
    return passed


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build", type=Path)
    parser.add_argument("config", choices=("Debug", "Release"))
    parser.add_argument("reports", type=Path)
    parser.add_argument("--require-x86", action="store_true",
                        help="require passing x86 ABI and native-size boundary cases")
    args = parser.parse_args()
    passed = run(args.build.resolve(), args.config, args.reports.resolve())
    if args.require_x86:
        missing = X86_REQUIRED_CASES - passed
        if missing:
            raise SystemExit("Missing passing x86 cases: " + ", ".join(sorted(missing)))
        print("Verified x86 execution with sizeof(size_t) == 4 and all required boundary cases")
