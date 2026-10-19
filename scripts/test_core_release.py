# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Marek Cingel

"""Regression tests for release rejection, evidence identity and archive versioning."""

import io
import json
import os
from pathlib import Path
import shutil
import subprocess
import tarfile
import tempfile
import unittest
from unittest.mock import patch

import core_release as release
from abi.check import current_major
from ci_timings import summarize


ROOT = Path(__file__).resolve().parents[1]
SHA = 'a' * 40
ENV = {'GITHUB_RUN_ID': '123', 'GITHUB_RUN_ATTEMPT': '1',
       'GITHUB_REPOSITORY': 'OpenTLV/opentlv', 'GITHUB_SHA': SHA,
       'GITHUB_EVENT_NAME': 'workflow_dispatch', 'GITHUB_REF': 'refs/heads/main',
       'RELEASE_TAG': 'v0.8.0'}
CHANGELOG = '# Changelog\n\n## [v0.8.0] - 2026-10-19\n\n### Changed\n\n- Release changes.\n'


def results(full=True):
    selected = release.FULL if full else release.FAST
    return {key: {'result': 'success' if key in selected else 'skipped'} for key in release.FULL}


def quality():
    return {'schema': 1, 'tier': 'full', 'commit': SHA, 'repository': 'OpenTLV/opentlv',
            'run_id': '123', 'run_attempt': '1',
            'checks': {key: 'success' for key in release.FULL}}


class CoreGateTests(unittest.TestCase):
    def test_windows_is_full_only(self):
        self.assertNotIn('msvc', release.FAST)
        self.assertIn('msvc', release.FULL)
        fast = results(False)
        self.assertEqual(fast['msvc']['result'], 'skipped')
        release.check_results(fast, False)

    def test_selected_checks_must_pass(self):
        for full in (True, False):
            release.check_results(results(full), full)
            for name in release.FULL if full else release.FAST:
                for state in ('skipped', 'cancelled', 'failure', 'neutral', None):
                    with self.subTest(full=full, name=name, state=state):
                        value = results(full)
                        value[name]['result'] = state
                        with self.assertRaises(ValueError):
                            release.check_results(value, full)

    def test_missing_or_extra_job_is_rejected(self):
        value = results()
        del value['abi']
        with self.assertRaises(ValueError):
            release.check_results(value, True)
        value['unexpected'] = {'result': 'success'}
        with self.assertRaises(ValueError):
            release.check_results(value, True)

    def test_evidence_must_match_full_run_identity(self):
        release.check_quality(quality(), SHA, '123', '1', 'OpenTLV/opentlv')
        for key, value in (('tier', 'fast'), ('schema', 0), ('commit', 'b' * 40),
                           ('run_id', '122'), ('run_attempt', '2'), ('repository', 'fork/opentlv')):
            with self.subTest(key=key):
                evidence = quality()
                evidence[key] = value
                with self.assertRaises(ValueError):
                    release.check_quality(evidence, SHA, '123', '1', 'OpenTLV/opentlv')

    def test_evidence_cannot_hide_skipped_full_check(self):
        evidence = quality()
        evidence['checks']['query'] = 'skipped'
        with self.assertRaises(ValueError):
            release.check_quality(evidence, SHA, '123', '1', 'OpenTLV/opentlv')

    @patch.dict(os.environ, ENV)
    def test_gate_records_checkout_and_exact_run(self):
        with tempfile.TemporaryDirectory() as temp:
            output = Path(temp) / 'quality.json'
            with patch.dict(os.environ, {'FULL': 'true', 'CHECK_RESULTS': json.dumps(results())}), \
                    patch('core_release.git', return_value=SHA):
                release.gate(output)
            release.check_quality(json.loads(output.read_text()), SHA, '123', '1', 'OpenTLV/opentlv')
            output.unlink()
            with patch.dict(os.environ, {'FULL': 'true', 'CHECK_RESULTS': json.dumps(results())}), \
                    patch('core_release.git', return_value='b' * 40):
                with self.assertRaises(ValueError):
                    release.gate(output)
            self.assertFalse(output.exists())

    def test_semver_validation(self):
        for value in ('0.8.0', '1.0.0-rc.1', '2.3.4-alpha-beta.0'):
            self.assertEqual(release.version(value), value)
        for value in ('v1.0.0', '1.0', '01.0.0', '1.0.0-rc.01', '../1.0.0', '1.0.0\n', '1.0.0+foo'):
            with self.subTest(value=value), self.assertRaises(ValueError):
                release.version(value)

    def test_release_requires_v_prefix(self):
        for value in ('v0.1.0', 'v1.0.0-rc.1'):
            self.assertEqual(release.release_tag(value), value[1:])
        for value in ('0.1.0', 'vv0.1.0', 'v01.0.0', 'v1.0', 'v1.0.0\n', None):
            with self.subTest(value=value), self.assertRaises(ValueError):
                release.release_tag(value)

    def test_changelog_must_be_closed_for_exact_latest_tag(self):
        release.check_changelog(CHANGELOG, 'v0.8.0')
        invalid = (
            '## [Unreleased]\n\n' + CHANGELOG,
            CHANGELOG + '\n**Unreleased**\n',
            CHANGELOG + '\n<!-- unreleased -->\n',
            CHANGELOG.replace('v0.8.0', 'v0.7.1'),
            CHANGELOG.replace('[v0.8.0]', '[0.8.0]'),
            CHANGELOG.replace('2026-10-19', '2026-02-30'),
            CHANGELOG.replace(' - 2026-10-19', ''),
            CHANGELOG + CHANGELOG,
            '## [v0.9.0]\n\n' + CHANGELOG,
            CHANGELOG.replace('- Release changes.', ''),
            CHANGELOG.replace('- Release changes.', '') + '## [0.7.1] - 2026-10-12\n- Old changes.\n',
        )
        for value in invalid:
            with self.subTest(changelog=value), self.assertRaises(ValueError):
                release.check_changelog(value, 'v0.8.0')

    @patch.dict(os.environ, ENV)
    def test_prepare_uses_exact_sha_and_only_local_tag(self):
        with patch('core_release.git', return_value=SHA), patch('core_release.subprocess.run') as run:
            release.prepare('v0.8.0')
            run.assert_called_once_with(['git', 'tag', 'v0.8.0', SHA], check=True)
        with patch('core_release.git', return_value='b' * 40), patch('core_release.subprocess.run') as run:
            with self.assertRaises(ValueError):
                release.prepare('v0.8.0')
            run.assert_not_called()

    def test_abi_policy_recognizes_v_tags_and_legacy_stable_tags(self):
        for tags, major in (('v1.0.0\n0.7.1\n', 1), ('v0.9.0\n2.0.0\n', 2),
                            ('v1.0.0-rc.1\n0.7.1\n', 0), ('', 0)):
            with self.subTest(tags=tags), patch('abi.check.subprocess.run') as run:
                run.return_value.stdout = tags
                self.assertEqual(current_major(), major)

    def test_query_gate_requires_latest_success_for_exact_commit(self):
        good = {'id': 12, 'path': '.github/workflows/query-release.yml',
                'head_sha': SHA, 'event': 'workflow_dispatch',
                'status': 'completed', 'conclusion': 'success'}
        self.assertEqual(release.query_evidence_run([good], SHA), 12)
        for field, value in (('head_sha', 'b' * 40), ('event', 'pull_request'),
                             ('path', '.github/workflows/query-hardening.yml'),
                             ('status', 'in_progress'), ('conclusion', 'failure')):
            candidate = dict(good, **{field: value})
            with self.subTest(field=field), self.assertRaises(ValueError):
                release.query_evidence_run([candidate], SHA)
        with self.assertRaises(ValueError):
            release.query_evidence_run([good, dict(good, id=13, conclusion='failure')], SHA)

    @patch.dict(os.environ, ENV)
    def test_preflight_rejects_wrong_event_or_branch(self):
        with tempfile.TemporaryDirectory() as temp:
            output = Path(temp) / 'candidate.json'
            for changes in ({'GITHUB_EVENT_NAME': 'push'}, {'GITHUB_REF': 'refs/heads/feature/test'}):
                with patch.dict(os.environ, changes), self.assertRaises(ValueError):
                    release.preflight(output)
            self.assertFalse(output.exists())

    @patch.dict(os.environ, ENV)
    def test_preflight_rejects_unreleased_before_git_or_evidence(self):
        with tempfile.TemporaryDirectory() as temp, \
                patch('core_release.Path.read_text', return_value='**Unreleased**\n' + CHANGELOG), \
                patch('core_release.git') as git, patch('core_release.api') as api:
            with self.assertRaisesRegex(ValueError, 'Unreleased'):
                release.preflight(Path(temp) / 'candidate.json')
            git.assert_not_called()
            api.assert_not_called()

    @patch.dict(os.environ, ENV)
    @patch('core_release.Path.read_text', return_value=CHANGELOG)
    def test_preflight_rejects_dirty_checkout(self, _read):
        with tempfile.TemporaryDirectory() as temp:
            output = Path(temp) / 'candidate.json'
            with patch('core_release.git', side_effect=[SHA, '', ' M CMakeLists.txt']), \
                    patch('core_release.subprocess.run'):
                with self.assertRaises(ValueError):
                    release.preflight(output)
            self.assertFalse(output.exists())

    @patch.dict(os.environ, ENV)
    @patch('core_release.Path.read_text', return_value=CHANGELOG)
    def test_preflight_rejects_wrong_checkout(self, _read):
        with tempfile.TemporaryDirectory() as temp, patch('core_release.git', return_value='b' * 40):
            with self.assertRaises(ValueError):
                release.preflight(Path(temp) / 'candidate.json')

    @patch.dict(os.environ, ENV)
    @patch('core_release.Path.read_text', return_value=CHANGELOG)
    def test_preflight_rejects_commit_outside_main(self, _read):
        with tempfile.TemporaryDirectory() as temp, \
                patch('core_release.git', side_effect=[SHA, '']), \
                patch('core_release.subprocess.run', side_effect=subprocess.CalledProcessError(1, 'git')):
            with self.assertRaises(subprocess.CalledProcessError):
                release.preflight(Path(temp) / 'candidate.json')

    @patch.dict(os.environ, ENV)
    @patch('core_release.Path.read_text', return_value=CHANGELOG)
    def test_preflight_rejects_existing_remote_tag(self, _read):
        with tempfile.TemporaryDirectory() as temp, \
                patch('core_release.git', side_effect=[SHA, f'{SHA}\trefs/tags/v0.8.0']):
            with self.assertRaises(ValueError):
                release.preflight(Path(temp) / 'candidate.json')

    @patch.dict(os.environ, ENV)
    def test_preflight_records_closed_candidate(self):
        with tempfile.TemporaryDirectory() as temp, \
                patch('core_release.git', side_effect=[SHA, '', '']), \
                patch('core_release.subprocess.run'):
            output = Path(temp) / 'candidate.json'
            with patch('core_release.Path.read_text', return_value=CHANGELOG):
                release.preflight(output)
            self.assertEqual(json.loads(output.read_text()), {
                'tag': 'v0.8.0', 'version': '0.8.0', 'commit': SHA, 'query_evidence_run_id': None})

    @patch.dict(os.environ, dict(ENV, RELEASE_TAG='v1.0.0-rc.1'))
    @patch('core_release.Path.read_text', return_value=CHANGELOG.replace('v0.8.0', 'v1.0.0-rc.1'))
    def test_one_point_zero_preserves_query_evidence_requirement(self, _read):
        with tempfile.TemporaryDirectory() as temp, \
                patch('core_release.git', side_effect=[SHA, '', '']), \
                patch('core_release.subprocess.run'), \
                patch('core_release.api', return_value={'workflow_runs': []}):
            with self.assertRaisesRegex(ValueError, 'Query release candidate'):
                release.preflight(Path(temp) / 'candidate.json')

    def test_timings_distinguish_parallel_cost_and_elapsed_span(self):
        jobs = [{'name': 'first', 'conclusion': 'success',
                 'started_at': '2026-10-19T00:00:00Z', 'completed_at': '2026-10-19T00:00:20Z'},
                {'name': 'second', 'conclusion': 'failure',
                 'started_at': '2026-10-19T00:00:10Z', 'completed_at': '2026-10-19T00:00:25Z'},
                {'name': 'omitted', 'conclusion': 'skipped',
                 'started_at': None, 'completed_at': None}]
        report = summarize(jobs)
        self.assertEqual(report['job_seconds'], 35)
        self.assertEqual(report['execution_span_seconds'], 25)
        self.assertEqual(report['job_count'], 2)


class ArchiveTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def source(self):
        stream = io.BytesIO()
        with tarfile.open(fileobj=stream, mode='w') as archive:
            entry = tarfile.TarInfo('OpenTLV-0.8.0/README.md')
            data = b'test source\n'
            entry.size = len(data)
            archive.addfile(entry, io.BytesIO(data))
        with patch('core_release.git', side_effect=[SHA, '']), \
                patch('core_release.subprocess.check_output', return_value=stream.getvalue()):
            return release.source_archive(self.root, '0.8.0', SHA)

    def test_source_bytes_and_metadata_roundtrip(self):
        archive = self.source()
        first = archive.read_bytes()
        self.assertEqual(first, self.source().read_bytes())
        with tarfile.open(archive) as source:
            self.assertEqual(source.extractfile('OpenTLV-0.8.0/README.md').read(), b'test source\n')
            self.assertEqual(source.extractfile('OpenTLV-0.8.0/opentlv-source.txt').read(),
                             f'version=0.8.0\ncommit={SHA}\n'.encode())

    def test_dirty_source_is_rejected(self):
        with patch('core_release.git', side_effect=[SHA, '?? unexpected-file']):
            with self.assertRaises(ValueError):
                release.source_archive(self.root, '0.8.0', SHA)

    def fixture(self):
        self.source()
        for identifier in release.BINARY_IDS:
            suffix = '.zip' if identifier.startswith('windows-') else '.tar.gz'
            archive = self.root / f'OpenTLV-0.8.0-{identifier}{suffix}'
            archive.write_bytes(identifier.encode())
            release.write_json(self.root / (identifier + '.json'), {
                'kind': 'binary', 'id': identifier, 'version': '0.8.0', 'commit': SHA,
                'filename': archive.name, 'sha256': release.digest(archive)})
        evidence = self.root / 'evidence' / 'quality.json'
        release.write_json(evidence, quality())
        release.write_json(evidence.parent / 'release-candidate.json', {
            'tag': 'v0.8.0', 'version': '0.8.0', 'commit': SHA, 'query_evidence_run_id': None})
        # Evidence is deliberately outside the publish directory.
        destination = self.root / 'artifacts'
        destination.mkdir()
        for item in list(self.root.iterdir()):
            if item.is_file():
                shutil.move(str(item), destination / item.name)
        return destination, evidence

    @patch.dict(os.environ, ENV)
    def test_complete_manifest_and_checksum_set(self):
        destination, evidence = self.fixture()
        release.manifest(destination, evidence, '0.8.0', SHA)
        manifest = json.loads((destination / 'release-manifest.json').read_text())
        self.assertEqual(len(manifest['artifacts']), 5)
        for line in (destination / 'SHA256SUMS').read_text().splitlines():
            checksum, name = line.split('  ')
            self.assertEqual(checksum, release.digest(destination / name))

    @patch.dict(os.environ, ENV)
    def test_modified_binary_is_rejected(self):
        destination, evidence = self.fixture()
        next(destination.glob('*.zip')).write_bytes(b'modified')
        with self.assertRaises(ValueError):
            release.manifest(destination, evidence, '0.8.0', SHA)

    @patch.dict(os.environ, ENV)
    def test_source_from_other_revision_is_rejected(self):
        destination, evidence = self.fixture()
        source = destination / 'OpenTLV-0.8.0-source.tar.gz'
        with tarfile.open(source, 'w:gz') as archive:
            metadata = f'version=0.8.0\ncommit={"b" * 40}\n'.encode()
            entry = tarfile.TarInfo('OpenTLV-0.8.0/opentlv-source.txt')
            entry.size = len(metadata)
            archive.addfile(entry, io.BytesIO(metadata))
        with self.assertRaisesRegex(ValueError, 'Source archive identity'):
            release.manifest(destination, evidence, '0.8.0', SHA)

    @patch.dict(os.environ, ENV)
    def test_missing_configuration_is_rejected(self):
        destination, evidence = self.fixture()
        (destination / 'linux-x64-static.json').unlink()
        with self.assertRaises(FileNotFoundError):
            release.manifest(destination, evidence, '0.8.0', SHA)

    @patch.dict(os.environ, ENV)
    def test_unexpected_artifact_is_rejected(self):
        destination, evidence = self.fixture()
        (destination / 'unverified.zip').write_bytes(b'unverified')
        with self.assertRaises(ValueError):
            release.manifest(destination, evidence, '0.8.0', SHA)

    @patch.dict(os.environ, ENV)
    def test_record_path_escape_is_rejected(self):
        destination, evidence = self.fixture()
        record_path = destination / 'linux-x64-static.json'
        record = json.loads(record_path.read_text())
        record['filename'] = '../unverified.zip'
        release.write_json(record_path, record)
        with self.assertRaises(ValueError):
            release.manifest(destination, evidence, '0.8.0', SHA)

    def cmake_metadata(self, metadata):
        source = self.root / 'source'
        (source / 'cmake').mkdir(parents=True, exist_ok=True)
        shutil.copy2(ROOT / 'cmake/version.cmake', source / 'cmake/version.cmake')
        (source / 'opentlv-source.txt').write_text(metadata, encoding='utf-8')
        script = self.root / 'check.cmake'
        script.write_text('include("' + (source / 'cmake/version.cmake').as_posix() + '")\n'
                          'if(NOT OPENTLV_VERSION_STRING STREQUAL "1.2.3-rc.1" OR\n'
                          '   NOT OPENTLV_GIT_TAG STREQUAL "v1.2.3-rc.1" OR\n'
                          f'   NOT OPENTLV_GIT_COMMIT_HASH STREQUAL "{SHA}")\n'
                          'message(FATAL_ERROR "Incorrect archive identity")\nendif()\n', encoding='utf-8')
        return subprocess.run(['cmake', '-P', str(script)], capture_output=True, text=True)

    def test_cmake_preserves_archive_version_without_git(self):
        result = self.cmake_metadata(f'version=1.2.3-rc.1\ncommit={SHA}\n')
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_cmake_reads_prefixed_git_prerelease_and_stable_tags(self):
        self.cmake_metadata(f'version=1.2.3-rc.1\ncommit={SHA}\n')
        source = self.root / 'source'
        (source / 'opentlv-source.txt').unlink()

        def fixture_git(*args):
            return subprocess.check_output([
                'git', '-C', str(source), '-c', 'user.name=Version fixture',
                '-c', 'user.email=fixture@example.invalid', '-c', 'commit.gpgsign=false',
                '-c', 'core.hooksPath=' + str(self.root / 'no-hooks'), *args], text=True).strip()

        fixture_git('init', '--quiet')
        fixture_git('add', 'cmake/version.cmake')
        fixture_git('commit', '--quiet', '-m', 'Version fixture')
        commit = fixture_git('rev-parse', '--short', 'HEAD')
        script = self.root / 'check.cmake'
        script.write_text(script.read_text().replace(SHA, commit), encoding='utf-8')
        fixture_git('tag', 'v1.2.3-rc.1')
        result = subprocess.run(['cmake', '-P', str(script)], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        fixture_git('tag', 'v1.2.3')
        script.write_text(script.read_text().replace('1.2.3-rc.1', '1.2.3'), encoding='utf-8')
        result = subprocess.run(['cmake', '-P', str(script)], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_cmake_rejects_malformed_archive_metadata(self):
        for metadata in ('version=1.2.3-rc.1\ncommit=short\n',
                         f'version=1.2.3-rc.01\ncommit={SHA}\n',
                         f'version=1.2.3-rc.1\ncommit={SHA}\ninclude(evil)\n'):
            with self.subTest(metadata=metadata):
                self.assertNotEqual(self.cmake_metadata(metadata).returncode, 0)


if __name__ == '__main__':
    unittest.main()
