"""Check the direct job results for the Fixed TLV workflow's trigger."""

import json
import os


def check_results(event, needs):
    if event in ("pull_request", "push"):
        windows_package = "skipped"
    elif event in ("workflow_dispatch", "schedule"):
        windows_package = "success"
    else:
        raise SystemExit(f"Unsupported CI event {event!r}")

    expected = dict.fromkeys(
        ("fixed-tlv", "resource-usage", "callgrind", "windows-x86", "workflow-analyzers"),
        "success",
    )
    expected["windows-package"] = windows_package
    if not isinstance(needs, dict):
        raise SystemExit("JOB_RESULTS must be an object keyed by job ID")
    if needs.keys() != expected.keys():
        missing = sorted(expected.keys() - needs.keys())
        unexpected = sorted(needs.keys() - expected.keys())
        raise SystemExit(f"{event} dependencies: missing {missing}, unexpected {unexpected}")

    failures = []
    for job, required in expected.items():
        details = needs[job]
        actual = details.get("result") if isinstance(details, dict) else None
        if actual != required:
            failures.append(f"{job}: expected {required}, got {actual!r}")
    if failures:
        raise SystemExit("\n".join(failures))


if __name__ == "__main__":
    event = os.environ.get("GITHUB_EVENT_NAME", "")
    try:
        needs = json.loads(os.environ.get("JOB_RESULTS", ""))
    except json.JSONDecodeError as error:
        raise SystemExit(f"Invalid JOB_RESULTS: {error}") from None
    check_results(event, needs)
    print(f"All required job results match the {event} policy")
