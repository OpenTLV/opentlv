"""Regression checks for required GitHub Actions job results."""

import unittest

from check_ci_gate import check_results


def job_results(windows_package="success"):
    return {
        "fixed-tlv": {"result": "success"},
        "callgrind": {"result": "success"},
        "resource-usage": {"result": "success"},
        "windows-x86": {"result": "success"},
        "workflow-analyzers": {"result": "success"},
        "windows-package": {"result": windows_package},
    }


class QualityGate(unittest.TestCase):
    def test_each_job_conclusion_on_each_trigger(self):
        for event, windows_package in (
            ("pull_request", "skipped"),
            ("push", "skipped"),
            ("workflow_dispatch", "success"),
            ("schedule", "success"),
        ):
            passing = job_results(windows_package)
            check_results(event, passing)
            for job, details in passing.items():
                for result in ("success", "failure", "cancelled", "skipped"):
                    if result == details["result"]:
                        continue
                    with self.subTest(event=event, job=job, result=result):
                        with self.assertRaisesRegex(SystemExit, job):
                            check_results(event, {**passing, job: {"result": result}})

    def test_missing_and_unexpected_dependencies(self):
        for job in job_results():
            needs = job_results()
            del needs[job]
            with self.subTest(job=job), self.assertRaisesRegex(SystemExit, job):
                check_results("schedule", needs)
        with self.assertRaisesRegex(SystemExit, "extra-job"):
            check_results("schedule", {**job_results(), "extra-job": {"result": "success"}})

    def test_unknown_event_or_malformed_results(self):
        with self.assertRaisesRegex(SystemExit, "release"):
            check_results("release", job_results())
        with self.assertRaisesRegex(SystemExit, "JOB_RESULTS"):
            check_results("schedule", [])
        for details in ({}, None, {"result": "neutral"}):
            with self.subTest(details=details), self.assertRaisesRegex(SystemExit, "fixed-tlv"):
                check_results("schedule", {**job_results(), "fixed-tlv": details})
