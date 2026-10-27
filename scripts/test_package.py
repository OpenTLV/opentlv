# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Marek Cingel

"""Ensure native package verification rejects absent or rewritten attribution."""

from pathlib import Path
import shutil
import tempfile
import unittest

from check_package import check_license_files


class LicenseFilesTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.source = Path(self.temp.name) / "source"
        self.package = Path(self.temp.name) / "package"
        self.notices = self.package / "custom-data/doc/OpenTLV"
        for name in ("LICENSE", "THIRD_PARTY.md", "LICENSES/MIT.txt", "LICENSES/BSD-3-Clause.txt"):
            path = self.source / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(("Original notice: " + name + "\n").encode())
        shutil.copytree(self.source, self.notices)

    def test_all_license_texts_survive_custom_install_directory(self):
        check_license_files(self.package, self.source, "custom-data")

    def test_missing_notice_is_rejected(self):
        for name in ("LICENSE", "THIRD_PARTY.md", "LICENSES/MIT.txt", "LICENSES/BSD-3-Clause.txt"):
            with self.subTest(name=name):
                path = self.notices / name
                path.unlink()
                with self.assertRaisesRegex(RuntimeError, "Required license file missing"):
                    check_license_files(self.package, self.source, "custom-data")
                shutil.copyfile(self.source / name, path)

    def test_rewritten_notice_is_rejected(self):
        (self.notices / "LICENSE").write_bytes(b"Changed copyright holder\n")
        with self.assertRaisesRegex(RuntimeError, "differs from source"):
            check_license_files(self.package, self.source, "custom-data")


if __name__ == "__main__":
    unittest.main()
