# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Marek Cingel

"""Release rejection tests for amalgamation identity, tested bytes and notices."""

import copy
import io
import json
from pathlib import Path
import tarfile
import tempfile
import unittest
from unittest.mock import patch

import amalgamation_release as amalgamation
from core_release import write_json


SHA = 'a' * 40


def fixture(directory, number='0.8.0'):
    metadata = {'schema': 1, 'library': 'OpenTLV', 'version': number, 'tag': 'v' + number,
                'revision': 0, 'commit': SHA[:8], 'branch': 'HEAD',
                'configuration': dict(amalgamation.FEATURES),
                'targets': [{'key': 'core'}], 'inputs': [{'path': 'src/core/version.c'}]}
    # REUSE-IgnoreStart
    files = {'opentlv.c': b'// SPDX-License-Identifier: MIT\n',
             'opentlv.h': b'// SPDX-License-Identifier: MIT\n',
             'include/opentlv/tlv.hpp': b'// SPDX-License-Identifier: MIT\n',
             'opentlv-amalgamation.json': json.dumps(metadata).encode()}
    # REUSE-IgnoreEnd
    for name, source in amalgamation.EXTRAS.items():
        files[name] = (amalgamation.ROOT / source).read_bytes().replace(b'\r\n', b'\n')
    tested = {name: amalgamation.sha256(data) for name, data in files.items()
              if name not in amalgamation.EXTRAS}
    evidence = {'status': 'passed', 'commit': SHA, 'working_tree': '', 'artifact_hashes': tested,
                'variants': {name: {'status': 'passed', 'ctest': 10, 'gtest': 20, 'fuzz_targets': 2}
                             for name in ('conventional', 'installed', 'amalgamation')}}
    root = f'OpenTLV-{number}-amalgamation'
    record = {'schema': 1, 'artifact_contract': 2, 'kind': 'amalgamation', 'version': number,
              'commit': SHA, 'archive_root': root, 'filename': root + '.tar.gz',
              'metadata': metadata, 'equivalence': evidence, 'consumers': ['c99', 'c++17']}
    save(directory, record, files)
    return record, files


def save(directory, record, files):
    data = amalgamation.archive_bytes(files, record['archive_root'])
    (directory / record['filename']).write_bytes(data)
    record['sha256'] = record['reproduction_sha256'] = amalgamation.sha256(data)
    record['files'] = {name: amalgamation.sha256(data) for name, data in files.items()}
    write_json(directory / 'amalgamation.json', record)


class AmalgamationReleaseTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.record, self.files = fixture(self.root)

    def verify(self):
        return amalgamation.verify(self.root, '0.8.0', SHA)

    def test_reproducible_archive_roundtrip_and_prerelease(self):
        self.assertEqual(self.verify(), self.record)
        data = amalgamation.archive_bytes(self.files, self.record['archive_root'])
        self.assertEqual(data, amalgamation.archive_bytes(dict(reversed(list(self.files.items()))),
                                                        self.record['archive_root']))
        self.assertEqual(amalgamation.read_archive(self.root / self.record['filename'],
                                                  self.record['archive_root']), self.files)
        record, _ = fixture(self.root, '0.9.0-rc.1')
        self.assertEqual(amalgamation.verify(self.root, '0.9.0-rc.1', SHA), record)

    def test_archive_or_record_corruption_rejected(self):
        path = self.root / self.record['filename']
        path.write_bytes(path.read_bytes() + b'corruption')
        with self.assertRaisesRegex(ValueError, 'checksum'):
            self.verify()
        for key, value in (('commit', 'b' * 40), ('version', '0.9.0'), ('schema', 2),
                           ('artifact_contract', 99), ('filename', '../escape.tar.gz'),
                           ('reproduction_sha256', '0' * 64), ('files', {}), ('consumers', [])):
            with self.subTest(key=key):
                record, _ = fixture(self.root)
                record[key] = value
                write_json(self.root / 'amalgamation.json', record)
                with self.assertRaises(ValueError):
                    self.verify()

    def test_rechecksummed_changed_or_missing_files_still_rejected(self):
        for name in self.files:
            for remove in (True, False):
                with self.subTest(name=name, remove=remove):
                    record, files = fixture(self.root)
                    if remove:
                        del files[name]
                    else:
                        files[name] += b'\nchanged\n'
                    save(self.root, record, files)
                    with self.assertRaises((ValueError, KeyError)):
                        self.verify()

    def test_wrong_metadata_even_with_updated_test_hashes_rejected(self):
        for key, value in (('version', '0.9.0'), ('commit', 'b' * 8), ('tag', 'v0.7.0'),
                           ('revision', 1), ('configuration', {'query_frontend': False}),
                           ('inputs', []), ('targets', [])):
            with self.subTest(key=key):
                record, files = fixture(self.root)
                record['metadata'][key] = value
                files['opentlv-amalgamation.json'] = json.dumps(record['metadata']).encode()
                record['equivalence']['artifact_hashes']['opentlv-amalgamation.json'] = \
                    amalgamation.sha256(files['opentlv-amalgamation.json'])
                save(self.root, record, files)
                with self.assertRaises(ValueError):
                    self.verify()

    def test_failed_dirty_wrong_revision_or_incomplete_equivalence_rejected(self):
        original = self.record['equivalence']
        cases = [dict(original, **{key: value}) for key, value in
                 (('status', 'failed'), ('commit', 'b' * 40), ('working_tree', ' M file'),
                  ('variants', {}), ('artifact_hashes', {}))]
        for variant in original['variants']:
            for key, value in (('status', 'failed'), ('ctest', 0), ('gtest', 21), ('fuzz_targets', 0)):
                report = copy.deepcopy(original)
                report['variants'][variant][key] = value
                cases.append(report)
        for report in cases:
            with self.subTest(report=report):
                self.record['equivalence'] = report
                write_json(self.root / 'amalgamation.json', self.record)
                with self.assertRaises(ValueError):
                    self.verify()

    def test_unsafe_links_duplicates_and_wrong_roots_rejected_before_extraction(self):
        root = self.record['archive_root']
        for name, kind, duplicate in ((root + '/../escape', tarfile.REGTYPE, False),
                                       ('wrong/file', tarfile.REGTYPE, False),
                                       (root + '/link', tarfile.SYMTYPE, False),
                                       (root + '/link', tarfile.LNKTYPE, False),
                                       (root + '/file', tarfile.REGTYPE, True)):
            with self.subTest(name=name, kind=kind, duplicate=duplicate):
                path = self.root / 'unsafe.tar.gz'
                with tarfile.open(path, 'w:gz') as archive:
                    entry = tarfile.TarInfo(name)
                    entry.type = kind
                    if kind in (tarfile.SYMTYPE, tarfile.LNKTYPE):
                        entry.linkname = '../outside'
                    archive.addfile(entry, io.BytesIO())
                    if duplicate:
                        archive.addfile(entry, io.BytesIO())
                with self.assertRaises(ValueError):
                    amalgamation.read_archive(path, root)

    def test_timestamp_tampering_rejected_even_with_updated_archive_checksums(self):
        path = self.root / self.record['filename']
        with tarfile.open(path, 'w:gz') as archive:
            for name, data in self.files.items():
                entry = tarfile.TarInfo(self.record['archive_root'] + '/' + name)
                entry.size, entry.mtime = len(data), 123456
                archive.addfile(entry, io.BytesIO(data))
        self.record['sha256'] = self.record['reproduction_sha256'] = amalgamation.digest(path)
        write_json(self.root / 'amalgamation.json', self.record)
        with self.assertRaisesRegex(ValueError, 'archive encoding'):
            self.verify()

    def test_producer_rejects_dirty_or_wrong_checkout_before_building(self):
        for responses in ([SHA, ' M source.c'], ['b' * 40]):
            with patch('amalgamation_release.git', side_effect=responses), \
                    patch('amalgamation_release.run') as run:
                with self.assertRaisesRegex(ValueError, 'clean candidate'):
                    amalgamation.produce(self.root, self.root / 'work', self.root / 'report.json',
                                          '0.8.0', SHA, 'cc', 'c++')
                run.assert_not_called()

    def test_producer_rejects_changed_regeneration_before_consumers(self):
        report = self.root / 'report.json'
        write_json(report, self.record['equivalence'])
        changed = dict(self.files)
        changed['opentlv.c'] += b'changed'
        with patch('amalgamation_release.git', side_effect=[SHA, '']), \
                patch('amalgamation_release.shutil.which', side_effect=lambda value: value), \
                patch('amalgamation_release.run'), \
                patch('amalgamation_release.payload', side_effect=[self.files, changed]), \
                patch('amalgamation_release.consumers') as consumers:
            with self.assertRaisesRegex(ValueError, 'distribution-tested bytes'):
                amalgamation.produce(self.root / 'output', self.root / 'work', report,
                                      '0.8.0', SHA, 'cc', 'c++')
            consumers.assert_not_called()


if __name__ == '__main__':
    unittest.main()
