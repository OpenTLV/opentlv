"""Exercise candidate preparation and signed publication with local test identities."""

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys
import unittest
from unittest.mock import patch


ROOT = Path(__file__).resolve().parents[1]
REPOSITORY = "example/opentlv"


def rehearse(tools):
    # The pinned organization fixture owns the temporary key, local Git origin,
    # and in-memory Releases API used by its publisher tests.
    sys.path[:0] = [str(tools / "tests"), str(tools)]
    from test_publish_release import PublishTests, command, publisher

    class ReleaseRehearsal(PublishTests):
        def commit_candidate(self, changelog):
            Path("CHANGELOG.md").write_text(changelog, encoding="utf-8")
            command("git", "add", "--", "scripts", "CMakeLists.txt", "CHANGELOG.md")
            command("git", "commit", "-m", "Prepare candidate")
            return command("git", "rev-parse", "HEAD")

        def prepare(self, version, sha):
            return subprocess.run(
                [sys.executable, "-B", "scripts/prepare_release.py", "--tools", str(tools),
                 "--version", version, "--sha", sha, "--notes", str(self.notes.resolve())],
                capture_output=True, text=True,
            )

        def test_release(self):
            shutil.copytree(ROOT / "scripts", "scripts", ignore=shutil.ignore_patterns("__pycache__"))
            Path("CMakeLists.txt").write_text("project(OpenTLV VERSION 0.1.0 LANGUAGES C)\n",
                                              encoding="utf-8")
            heading = "# Changelog\n\n## [Unreleased]\n\n"
            undated_sha = self.commit_candidate(heading + "### Added\n\n- Details.\n")
            result = self.prepare("0.1.0", undated_sha)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("expected a dated release heading", result.stderr)
            wrong_version_sha = self.commit_candidate(heading + "## [0.2.0] - 2026-01-01\n\n- Details.\n")
            result = self.prepare("0.2.0", wrong_version_sha)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("expected a release version with CMake version 0.1.0", result.stderr)
            history = ""
            for version, prerelease in [("0.1.0-rc.1", True), ("0.1.0", False)]:
                with self.subTest(version=version):
                    body = f"### Added\n\n- Verify {version} with [CMake](CMakeLists.txt).\n"
                    history = f"## [{version}] - 2026-01-02\n\n{body}\n{history}"
                    sha = self.commit_candidate(heading + history)
                    result = self.prepare(version, wrong_version_sha)
                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn(f"found checkout {sha}", result.stderr)
                    result = self.prepare(version, sha)
                    self.assertEqual(result.returncode, 0, result.stderr)
                    expected = body.replace("(CMakeLists.txt)",
                                            f"(https://github.com/{REPOSITORY}/blob/{sha}/CMakeLists.txt)")
                    self.assertEqual(self.notes.read_bytes().decode(), expected)

                    args = (REPOSITORY, version, sha, "OpenTLV", self.fingerprint, self.notes)
                    oid = publisher.publish(*args)
                    tag = "v" + version
                    git = ["git", "--git-dir=../remote.git"]
                    self.assertEqual(command(*git, "rev-parse", tag + "^{commit}"), sha)
                    signature = subprocess.check_output([*git, "verify-tag", "--raw", tag],
                                                        stderr=subprocess.STDOUT, text=True)
                    self.assertIn(f"[GNUPG:] VALIDSIG {self.fingerprint} ", signature)
                    self.assertEqual(self.releases[-1], {
                        "tag_name": tag, "name": "OpenTLV " + version, "body": expected,
                        "prerelease": prerelease, "draft": False,
                    })
                    self.assertEqual(publisher.publish(*args), oid)
            self.assertEqual(self.creates, 2)
            with Path("CHANGELOG.md").open("a", encoding="utf-8") as changelog:
                changelog.write("\n- Uncommitted edit.\n")
            result = self.prepare("0.1.0", sha)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn(f"CHANGELOG.md differs from the file committed at {sha}", result.stderr)

    env = {"GIT_ALLOW_PROTOCOL": "file", "GITHUB_REPOSITORY": REPOSITORY,
           "GITHUB_SERVER_URL": "https://github.com"}
    with patch.dict(os.environ, env):
        result = unittest.TextTestRunner(verbosity=2).run(unittest.TestSuite([
            ReleaseRehearsal("test_release"),
        ]))
    if result.skipped:
        raise SystemExit("Release rehearsal requires Git, gpg and gpgconf")
    return result.wasSuccessful()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("tools", type=Path, help="pinned OpenTLV/.github checkout")
    args = parser.parse_args()
    sys.exit(0 if rehearse(args.tools.resolve()) else 1)
