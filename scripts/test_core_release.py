# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Marek Cingel

"""Regression tests for release rejection, evidence identity and archive versioning."""

import io
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tarfile
import tempfile
import unittest
from unittest.mock import patch

import core_release as release
from abi.check import current_major
from ci_timings import summarize
from ci_timings import main as timings_main


ROOT = Path(__file__).resolve().parents[1]
SHA = 'a' * 40
BASE_SHA = 'b' * 40
ENV = {'GITHUB_RUN_ID': '123', 'GITHUB_RUN_ATTEMPT': '1',
       'GITHUB_REPOSITORY': 'OpenTLV/opentlv', 'GITHUB_SHA': SHA,
       'GITHUB_EVENT_NAME': 'workflow_dispatch', 'GITHUB_REF': 'refs/heads/main',
       'RELEASE_TAG': 'v0.8.0'}
CHANGELOG = '# Changelog\n\n## [v0.8.0] - 2026-10-19\n\n### Changed\n\n- Release changes.\n'
BASELINE = {'release_id': 17, 'tag': '0.7.1', 'version': '0.7.1', 'commit': BASE_SHA,
            'url': 'https://github.com/OpenTLV/opentlv/releases/tag/0.7.1',
            'published_at': '2026-10-12T12:00:00Z'}


def published(tag='0.7.1', **changes):
    return dict({'id': 17, 'tag_name': tag, 'draft': False, 'prerelease': False,
                 'published_at': BASELINE['published_at'], 'html_url': BASELINE['url']}, **changes)


def candidate():
    return {'tag': 'v0.8.0', 'version': '0.8.0', 'commit': SHA,
            'repository': ENV['GITHUB_REPOSITORY'], 'run_id': '123', 'run_attempt': '1',
            'query_evidence_run_id': None, 'baseline': BASELINE}


def results(full=True):
    selected = release.FULL if full else release.PR
    return {key: {'result': 'success' if key in selected else 'skipped'} for key in release.FULL}


def quality():
    return {'schema': 2, 'tier': 'full', 'commit': SHA, 'repository': 'OpenTLV/opentlv',
            'run_id': '123', 'run_attempt': '1',
            'configuration': release.configuration('full'),
            'checks': {key: 'success' for key in release.FULL}}


def static_results():
    return {name: {'result': 'success'} for name in ('pre-commit', 'cppcheck', 'clang-tidy')}


class StaticGateTests(unittest.TestCase):
    def test_complete_static_checks_pass(self):
        release.check_static_results(static_results())

    def test_missing_or_extra_static_jobs_are_rejected(self):
        for name in ('pre-commit', 'cppcheck', 'clang-tidy'):
            with self.subTest(missing=name):
                value = static_results()
                del value[name]
                with self.assertRaises(ValueError):
                    release.check_static_results(value)
        value = static_results()
        value['unexpected'] = {'result': 'success'}
        with self.assertRaises(ValueError):
            release.check_static_results(value)

    def test_every_static_check_must_succeed_including_full(self):
        for name in ('pre-commit', 'cppcheck', 'clang-tidy'):
            for state in ('failure', 'cancelled', 'skipped', 'neutral', 'timed_out', None):
                with self.subTest(name=name, state=state):
                    value = static_results()
                    value[name]['result'] = state
                    with self.assertRaises(ValueError):
                        release.check_static_results(value)

    def test_malformed_static_results_are_rejected(self):
        for value in (None, [], 'success'):
            with self.subTest(results=value), self.assertRaises(ValueError):
                release.check_static_results(value)
        for name in ('pre-commit', 'cppcheck', 'clang-tidy'):
            for result in ({}, {'conclusion': 'success'}, None, 'success'):
                with self.subTest(name=name, result=result):
                    value = static_results()
                    value[name] = result
                    with self.assertRaises(ValueError):
                        release.check_static_results(value)

    def test_static_gate_cli_requires_all_checks_without_tier_bypass(self):
        skipped = static_results()
        skipped['cppcheck']['result'] = 'skipped'
        cases = ((json.dumps(static_results()), True), (json.dumps(skipped), False),
                 ('{}', False), ('invalid-json', False))
        for checks, succeeds in cases:
            with self.subTest(checks=checks):
                result = subprocess.run(
                    [sys.executable, '-B', str(ROOT / 'scripts/core_release.py'), 'static-gate'],
                    env=dict(os.environ, FULL='true', CHECK_RESULTS=checks),
                    capture_output=True, text=True)
                self.assertEqual(result.returncode == 0, succeeds, result.stdout + result.stderr)


class CoreGateTests(unittest.TestCase):
    @patch.dict(os.environ, ENV)
    def test_main_gate_requires_exact_successful_smoke_and_codeql_results(self):
        passed = {'smoke': {'result': 'success'}, 'codeql': {'result': 'success'}}
        invalid = [{}, [], {'smoke': None}, {'smoke': {'result': 'success'}},
                   {'codeql': {'result': 'success'}}, dict(passed, extra={})]
        invalid.extend(dict(passed, **{name: {'result': state}}) for name in passed for state in
                       ('failure', 'skipped', 'cancelled', 'neutral', None))
        with tempfile.TemporaryDirectory() as temp, patch('core_release.git', return_value=SHA):
            output = Path(temp) / 'main.json'
            for checks in invalid:
                with self.subTest(checks=checks), \
                        patch.dict(os.environ, {'CHECK_RESULTS': json.dumps(checks)}):
                    with self.assertRaises(ValueError):
                        release.main_gate(output)
                    self.assertFalse(output.exists())
            with patch.dict(os.environ, {'CHECK_RESULTS': json.dumps(passed)}):
                release.main_gate(output)
            evidence = json.loads(output.read_text())
            self.assertEqual(evidence['tier'], 'main')
            self.assertEqual(evidence['checks'], {'smoke': 'success', 'codeql': 'success'})
            configuration = evidence['configuration']
            self.assertEqual(configuration['inputs'],
                             {'smoke': {'full': False}, 'codeql': {'full': True}})
            self.assertEqual(set(configuration['workflows']),
                             {'core-main.yml', 'cmake-consumers.yml', 'codeql.yml'})
            with self.assertRaisesRegex(ValueError, 'Full CI evidence'):
                release.check_quality(evidence, SHA, '123', '1', ENV['GITHUB_REPOSITORY'])

    def test_full_configuration_evidence_cannot_omit_or_change_inputs(self):
        for changes in ({}, {'inputs': {'full': False}}, {'workflows': {}}):
            evidence = quality()
            evidence['configuration'] = changes
            with self.subTest(changes=changes), self.assertRaisesRegex(ValueError, 'configuration evidence'):
                release.check_quality(evidence, SHA, '123', '1', ENV['GITHUB_REPOSITORY'])

    def test_windows_is_full_only(self):
        self.assertNotIn('msvc', release.PR)
        self.assertIn('msvc', release.FULL)
        fast = results(False)
        self.assertEqual(fast['msvc']['result'], 'skipped')
        release.check_results(fast, False)

    def test_selected_checks_must_pass(self):
        for full in (True, False):
            release.check_results(results(full), full)
            for name in release.FULL if full else release.PR:
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
                patch('core_release.subprocess.run'), \
                patch('core_release.release_baseline', return_value=BASELINE):
            output = Path(temp) / 'candidate.json'
            with patch('core_release.Path.read_text', return_value=CHANGELOG):
                release.preflight(output)
            self.assertEqual(json.loads(output.read_text()), candidate())

    @patch.dict(os.environ, ENV)
    def test_publish_revalidates_frozen_baseline_without_selecting_again(self):
        with tempfile.TemporaryDirectory() as temp:
            locked = Path(temp) / 'locked.json'
            release.write_json(locked, candidate())
            output = Path(temp) / 'candidate.json'
            original_read = Path.read_text
            def read(path, *args, **kwargs):
                return CHANGELOG if path == Path('CHANGELOG.md') else original_read(path, *args, **kwargs)
            with patch('core_release.Path.read_text', read), \
                    patch('core_release.git', side_effect=[SHA, '', '']), \
                    patch('core_release.subprocess.run'), \
                    patch('core_release.release_baseline', return_value=BASELINE) as baseline:
                release.preflight(output, locked)
            baseline.assert_called_once_with('0.8.0', SHA, BASELINE)
            self.assertEqual(json.loads(output.read_text()), candidate())

    @patch.dict(os.environ, ENV)
    def test_locked_candidate_from_other_attempt_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            locked = Path(temp) / 'locked.json'
            release.write_json(locked, dict(candidate(), run_attempt='2'))
            original_read = Path.read_text
            def read(path, *args, **kwargs):
                return CHANGELOG if path == Path('CHANGELOG.md') else original_read(path, *args, **kwargs)
            with patch('core_release.Path.read_text', read), \
                    patch('core_release.git', side_effect=[SHA, '', '']), \
                    patch('core_release.subprocess.run'), patch('core_release.release_baseline') as baseline:
                with self.assertRaisesRegex(ValueError, 'Locked candidate identity'):
                    release.preflight(Path(temp) / 'result.json', locked)
            baseline.assert_not_called()

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

    @patch.dict(os.environ, ENV)
    def test_whole_run_timings_include_performance_and_publication(self):
        jobs = [{'name': name, 'conclusion': 'success', 'started_at': '2026-10-19T00:00:00Z',
                 'completed_at': '2026-10-19T00:00:20Z'} for name in
                ('full / checks / Core quality gate', 'performance', 'publish')]
        with tempfile.TemporaryDirectory() as temp:
            output = Path(temp) / 'timings.json'
            for scope, count in (('core', 1), ('run', 3)):
                with self.subTest(scope=scope), \
                        patch.object(sys, 'argv', ['ci_timings.py', '--scope', scope, '--output', str(output)]), \
                        patch('ci_timings.api', return_value={'jobs': jobs}), patch('builtins.print'):
                    timings_main()
                report = json.loads(output.read_text())
                self.assertEqual(report['job_count'], count)
                self.assertEqual(report['scope'], scope)


class BaselineTests(unittest.TestCase):
    def test_selects_latest_eligible_stable_version_not_mutable_main_or_prerelease(self):
        releases = [published('0.7.0'), published('v0.7.1'), published('v0.8.0-rc.1'),
                    published('v0.8.0'), published('main'), published('v0.7.2', draft=True),
                    published('v0.7.3', prerelease=True), published('v0.7.4', published_at=None)]
        self.assertEqual(release.select_baseline(releases, '0.8.0-rc.2')['tag_name'], 'v0.7.1')

    def test_no_previous_stable_release_is_a_failure(self):
        for releases in ([], [published('v0.8.0')], [published('v0.8.0-rc.1')]):
            with self.subTest(releases=releases), self.assertRaisesRegex(ValueError, 'No previous'):
                release.select_baseline(releases, '0.8.0')

    def test_baseline_locks_annotated_and_lightweight_tags_to_commits(self):
        for remote in (f'{BASE_SHA}\trefs/tags/0.7.1',
                       f'{"c" * 40}\trefs/tags/0.7.1\n{BASE_SHA}\trefs/tags/0.7.1^{{}}'):
            with self.subTest(remote=remote), patch('core_release.git', side_effect=[BASE_SHA, remote]), \
                    patch('core_release.subprocess.run') as ancestry:
                self.assertEqual(release.baseline_record(published(), SHA), BASELINE)
                ancestry.assert_called_once_with(
                    ['git', 'merge-base', '--is-ancestor', BASE_SHA, SHA], check=True)

    def test_missing_changed_or_self_baseline_is_rejected(self):
        for commit, remote in ((BASE_SHA, ''), (BASE_SHA, f'{SHA}\trefs/tags/0.7.1'),
                               (SHA, f'{SHA}\trefs/tags/0.7.1')):
            with self.subTest(commit=commit, remote=remote), \
                    patch('core_release.git', side_effect=[commit, remote]), \
                    patch('core_release.subprocess.run') as ancestry:
                with self.assertRaises(ValueError):
                    release.baseline_record(published(), SHA)
                ancestry.assert_not_called()

    def test_baseline_must_be_ancestor_of_candidate(self):
        with patch('core_release.git', side_effect=[BASE_SHA, f'{BASE_SHA}\trefs/tags/0.7.1']), \
                patch('core_release.subprocess.run', side_effect=subprocess.CalledProcessError(1, 'git')):
            with self.assertRaises(subprocess.CalledProcessError):
                release.baseline_record(published(), SHA)

    @patch.dict(os.environ, ENV)
    def test_release_pagination_is_not_limited_to_first_page(self):
        with patch('core_release.api', side_effect=[[published('v0.6.0')] * 100, [published()]]), \
                patch('core_release.baseline_record', return_value=BASELINE) as record:
            self.assertEqual(release.release_baseline('0.8.0', SHA), BASELINE)
            record.assert_called_once_with(published(), SHA)

    @patch.dict(os.environ, ENV)
    def test_frozen_baseline_is_revalidated_by_release_id(self):
        with patch('core_release.api', return_value=published()) as api, \
                patch('core_release.baseline_record', return_value=BASELINE):
            self.assertEqual(release.release_baseline('0.8.0', SHA, BASELINE), BASELINE)
            api.assert_called_once_with('repos/OpenTLV/opentlv/releases/17')
        with patch('core_release.api', return_value=published()), \
                patch('core_release.baseline_record', return_value=dict(BASELINE, commit='c' * 40)):
            with self.assertRaisesRegex(ValueError, 'has changed'):
                release.release_baseline('0.8.0', SHA, BASELINE)


class ArchiveTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def source(self, missing=None):
        stream = io.BytesIO()
        with tarfile.open(fileobj=stream, mode='w') as archive:
            for name in ('README.md', 'LICENSE', 'LICENSES/MIT.txt', 'REUSE.toml', 'THIRD_PARTY.md'):
                if name != missing:
                    entry = tarfile.TarInfo('OpenTLV-0.8.0/' + name)
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
            for name in ('LICENSE', 'LICENSES/MIT.txt', 'REUSE.toml', 'THIRD_PARTY.md'):
                self.assertEqual(source.extractfile('OpenTLV-0.8.0/' + name).read(), b'test source\n')

    def test_source_requires_local_licensing_files(self):
        for name in ('LICENSE', 'LICENSES/MIT.txt', 'REUSE.toml', 'THIRD_PARTY.md'):
            with self.subTest(name=name), self.assertRaisesRegex(ValueError, 'missing licensing file'):
                self.source(missing=name)

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
        release.write_json(evidence.parent / 'release-candidate.json', candidate())
        # Evidence is deliberately outside the publish directory.
        destination = self.root / 'artifacts'
        destination.mkdir()
        for item in list(self.root.iterdir()):
            if item.is_file():
                shutil.move(str(item), destination / item.name)
        from test_core_performance import performance_fixture
        performance_fixture(destination, candidate())
        return destination, evidence

    @patch.dict(os.environ, ENV)
    def test_complete_manifest_and_checksum_set(self):
        destination, evidence = self.fixture()
        release.manifest(destination, evidence, '0.8.0', SHA)
        manifest = json.loads((destination / 'release-manifest.json').read_text())
        self.assertEqual(len(manifest['artifacts']), 8)
        for line in (destination / 'SHA256SUMS').read_text().splitlines():
            checksum, name = line.split('  ')
            self.assertEqual(checksum, release.digest(destination / name))

    @patch.dict(os.environ, ENV)
    def test_missing_or_changed_performance_blocks_publication(self):
        destination, evidence = self.fixture()
        (destination / 'performance.json').unlink()
        with self.assertRaises((ValueError, FileNotFoundError)):
            release.manifest(destination, evidence, '0.8.0', SHA)

    @patch.dict(os.environ, ENV)
    def test_corrupted_performance_archive_blocks_publication(self):
        destination, evidence = self.fixture()
        (destination / 'performance-data.tar.gz').write_bytes(b'corrupted')
        with self.assertRaises((ValueError, tarfile.ReadError)):
            release.manifest(destination, evidence, '0.8.0', SHA)

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
