# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Marek Cingel

"""Verify package attribution and collision-free CPack extraction identities."""

from pathlib import Path
import shutil
import subprocess
import tarfile
import tempfile
import unittest
import zipfile

from check_package import check_license_files
from core_release import check_binary_archive


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


class PackageIdentityTest(unittest.TestCase):
    def test_cpack_archives_coexist_with_linkage_and_prerelease_identity(self):
        packaging = Path(__file__).resolve().parents[1] / 'cmake/packaging.cmake'
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            source = root / 'source'
            source.mkdir()
            (source / 'LICENSE').write_text('Fixture license\n', encoding='utf-8')
            (source / 'CMakeLists.txt').write_text(
                'cmake_minimum_required(VERSION 3.16)\n'
                'project(OpenTLV VERSION 0.9.0 LANGUAGES NONE)\n'
                'set(CMAKE_SYSTEM_NAME "${TEST_SYSTEM}")\n'
                'set(CMAKE_SYSTEM_PROCESSOR "${TEST_PROCESSOR}")\n'
                'set(CMAKE_C_COMPILER_ID "${TEST_COMPILER}")\n'
                'file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/identity.txt" '
                '"${OPENTLV_VERSION_PRERELEASE}/${TEST_SYSTEM}/${OPENTLV_BUILD_SHARED_LIBS}")\n'
                'install(FILES "${CMAKE_CURRENT_BINARY_DIR}/identity.txt" DESTINATION share)\n'
                f'include("{packaging.as_posix()}")\n', encoding='utf-8')
            extracted = root / 'extracted'
            expected = {}
            for prerelease in ('', 'rc.1'):
                for system, processor, compiler, generator, extension in (
                        ('Linux', 'x86_64', 'GNU', 'TGZ', '.tar.gz'),
                        ('Windows', 'AMD64', 'MSVC', 'ZIP', '.zip'),
                        ('Linux', 'x86_64', 'Clang', 'TGZ', '.tar.gz')):
                    for shared in ('ON', 'OFF'):
                        number = '0.9.0' + ('-' + prerelease if prerelease else '')
                        linkage = 'shared' if shared == 'ON' else 'static'
                        compiler_suffix = '-clang' if compiler == 'Clang' else ''
                        name = f'OpenTLV-{number}-{system.lower()}-x64{compiler_suffix}-{linkage}'
                        build = root / name
                        with self.subTest(name=name):
                            subprocess.run(['cmake', '-S', str(source), '-B', str(build), '-G', 'Ninja',
                                            f'-DTEST_SYSTEM={system}', f'-DTEST_PROCESSOR={processor}',
                                            f'-DTEST_COMPILER={compiler}',
                                            f'-DOPENTLV_BUILD_SHARED_LIBS={shared}',
                                            f'-DOPENTLV_VERSION_PRERELEASE={prerelease}',
                                            f'-DCPACK_GENERATOR={generator}'],
                                           check=True, capture_output=True, text=True)
                            subprocess.run(['cpack', '--config', str(build / 'CPackConfig.cmake')],
                                           check=True, capture_output=True, text=True)
                            archive = build / 'packages' / (name + extension)
                            check_binary_archive(archive, name)
                            if extension == '.zip':
                                with zipfile.ZipFile(archive) as package:
                                    package.extractall(extracted)
                            else:
                                with tarfile.open(archive) as package:
                                    package.extractall(extracted, filter='data')
                            expected[name] = f'{prerelease}/{system}/{shared}'
            self.assertEqual({path.name for path in extracted.iterdir()}, set(expected))
            for name, content in expected.items():
                self.assertEqual((extracted / name / 'share/identity.txt').read_text(), content)


if __name__ == "__main__":
    unittest.main()
