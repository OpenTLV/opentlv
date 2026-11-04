# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Marek Cingel

"""Exercise archive attacks without ever extracting the malicious fixtures."""

import io
from pathlib import Path
import stat
import tarfile
import tempfile
import unittest
import zipfile

from archive_safety import check_archive


class ArchiveSafetyTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.path = Path(temporary.name) / 'package.tar.gz'
        self.root = 'OpenTLV-0.9.0-rc.1-linux-x64-shared'

    def tar(self, entries):
        with tarfile.open(self.path, 'w:gz') as archive:
            for name, kind, target in entries:
                entry = tarfile.TarInfo(name)
                entry.type = kind
                entry.linkname = target
                entry.size = 7 if kind == tarfile.REGTYPE else 0
                archive.addfile(entry, io.BytesIO(b'content') if entry.size else None)
        return self.path

    def test_files_directories_and_versioned_shared_library_chain(self):
        for version in ('0.9.0', '0.9.0-rc.1'):
            for linkage in ('static', 'shared'):
                root = f'OpenTLV-{version}-linux-x64-{linkage}'
                entries = [(root, tarfile.DIRTYPE, ''), (root + '/lib', tarfile.DIRTYPE, ''),
                           (root + '/lib/libopentlv_core.so.0.9.0', tarfile.REGTYPE, '')]
                if linkage == 'shared':
                    entries += [(root + '/lib/libopentlv_core.so', tarfile.SYMTYPE, 'libopentlv_core.so.0'),
                                (root + '/lib/libopentlv_core.so.0', tarfile.SYMTYPE, 'libopentlv_core.so.0.9.0'),
                                (root + '/include/core', tarfile.SYMTYPE, '../lib/libopentlv_core.so')]
                with self.subTest(version=version, linkage=linkage):
                    check_archive(self.tar(entries), root)

    def test_rejects_unsafe_entries_links_and_conflicts(self):
        root = self.root
        file = (root + '/lib/real.so', tarfile.REGTYPE, '')
        bad_cases = {
            'absolute entry': [('/outside', tarfile.REGTYPE, '')],
            'traversal entry': [(root + '/../outside', tarfile.REGTYPE, '')],
            'wrong root': [('other/file', tarfile.REGTYPE, '')],
            'absolute symlink': [(root + '/link', tarfile.SYMTYPE, '/outside')],
            'escaping symlink': [(root + '/lib/link', tarfile.SYMTYPE, '../../outside')],
            'hardlink': [(root + '/hard', tarfile.LNKTYPE, root + '/lib/real.so')],
            'escaping hardlink': [(root + '/hard', tarfile.LNKTYPE, '../outside')],
            'device': [(root + '/device', tarfile.CHRTYPE, '')],
            'fifo': [(root + '/fifo', tarfile.FIFOTYPE, '')],
            'unknown type': [(root + '/special', b'Z', '')],
            'duplicate': [(root + '/lib/./real.so', tarfile.REGTYPE, '')],
            'file-directory conflict': [(root + '/lib', tarfile.REGTYPE, '')],
            'unsafe chain': [(root + '/a', tarfile.SYMTYPE, 'b'),
                             (root + '/b', tarfile.SYMTYPE, '../outside')],
            'cycle': [(root + '/a', tarfile.SYMTYPE, 'b'), (root + '/b', tarfile.SYMTYPE, 'a')],
            'dangling link': [(root + '/a', tarfile.SYMTYPE, 'absent')],
            'write through unsafe link': [(root + '/a', tarfile.SYMTYPE, '../outside'),
                                          (root + '/a/file', tarfile.REGTYPE, '')],
            'write through safe link': [(root + '/a', tarfile.SYMTYPE, 'lib'),
                                        (root + '/a/file', tarfile.REGTYPE, '')],
            'symlink before dotdot': [(root + '/nested/a', tarfile.SYMTYPE, '..'),
                                      (root + '/b', tarfile.SYMTYPE, 'nested/a/../outside')],
            'link through file': [(root + '/a', tarfile.SYMTYPE, 'lib/real.so/../real.so')],
        }
        for name, entries in bad_cases.items():
            for reverse in (False, True):
                with self.subTest(case=name, reverse=reverse):
                    ordered = [file, *entries]
                    if reverse:
                        ordered.reverse()
                    with self.assertRaises(ValueError):
                        check_archive(self.tar(ordered), root)

    def test_truncated_tar_data_fails_closed(self):
        self.tar([(self.root + '/file', tarfile.REGTYPE, '')])
        data = self.path.read_bytes()
        self.path.write_bytes(data[:len(data) // 2])
        with self.assertRaises((ValueError, tarfile.TarError, EOFError)):
            check_archive(self.path, self.root)

    def test_zip_rejects_symlinks_case_collisions_and_unsafe_names(self):
        path = self.path.with_suffix('.zip')
        for name, mode in ((self.root + '/link', stat.S_IFLNK),
                           (self.root + '/FILE', stat.S_IFREG),
                           (self.root + '/NUL.txt', stat.S_IFREG),
                           (self.root + '/file.', stat.S_IFREG),
                           (self.root + '/a\\b', stat.S_IFREG)):
            with self.subTest(name=name):
                with zipfile.ZipFile(path, 'w') as archive:
                    archive.writestr(self.root + '/file', 'valid')
                    entry = zipfile.ZipInfo()
                    entry.filename = name
                    entry.create_system = 3
                    entry.external_attr = (mode | 0o644) << 16
                    archive.writestr(entry, '../outside')
                with self.assertRaises(ValueError):
                    check_archive(path, self.root)


if __name__ == '__main__':
    unittest.main()
