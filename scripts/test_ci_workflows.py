# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Marek Cingel

"""Guard Core workflow routing and mandatory job dependencies; actionlint validates YAML."""

from dataclasses import dataclass
import json
from pathlib import Path
import re
import textwrap
import unittest


WORKFLOWS = Path(__file__).resolve().parents[1] / '.github/workflows'
KEY = re.compile(r'''(?:([A-Za-z_][\w-]*)|'([A-Za-z_][\w-]*)'|"([A-Za-z_][\w-]*)"):\s*(.*)$''')
CORE_CHECKS = frozenset(('clang', 'msvc', 'static', 'consumers', 'codeql', 'gcc', 'properties',
                         'query', 'fuzz', 'capabilities', 'abi', 'memcheck'))
PR_CHECKS = frozenset(('clang', 'static', 'consumers', 'codeql'))
STATIC_CHECKS = frozenset(('pre-commit', 'cppcheck', 'clang-tidy'))


def uncomment(value):
    """Remove YAML trailing comments without treating quoted hashes as comments."""
    quote = None
    index = 0
    while index < len(value):
        char = value[index]
        if quote == '"' and char == '\\':
            index += 2
            continue
        if char == quote:
            if quote == "'" and value[index:index + 2] == "''":
                index += 2
                continue
            quote = None
        elif quote is None and char in ("'", '"'):
            quote = char
        elif quote is None and char == '#' and (index == 0 or value[index - 1].isspace()):
            return value[:index].rstrip()
        index += 1
    return value.rstrip()


def scalar(value):
    value = uncomment(value.strip())
    if len(value) >= 2 and value[0] == value[-1] and value[0] in ("'", '"'):
        return value[1:-1]
    return value


@dataclass
class Block:
    """Only the block mappings, step lists and scalar lists used by these workflows.

    Nested shell/script text is never scanned as mapping keys. Unsupported layouts
    fail explicitly; this helper deliberately does not implement general YAML.
    """

    lines: list
    value: str = ''

    def entries(self, sequence=False):
        if self.value not in ('', '{}'):
            raise AssertionError('Expected an indented workflow mapping or step list')
        content = [(index, len(line) - len(line.lstrip()), line.lstrip())
                   for index, line in enumerate(self.lines)
                   if line.strip() and not line.lstrip().startswith('#')]
        if not content:
            return []
        indent = min(item[1] for item in content)
        starts = [item for item in content if item[1] == indent]
        result = []
        for position, (index, _, line) in enumerate(starts):
            end = starts[position + 1][0] if position + 1 < len(starts) else len(self.lines)
            body = self.lines[index + 1:end]
            if sequence:
                if not line.startswith('- '):
                    raise AssertionError('Expected a block-style step list')
                result.append(Block([' ' * (indent + 2) + line[2:], *body]).mapping())
                continue
            match = KEY.fullmatch(line)
            if match is None:
                raise AssertionError(f'Unsupported workflow mapping key: {line}')
            name = next(value for value in match.groups()[:3] if value is not None)
            result.append((name, Block(body, uncomment(match[4]))))
        return result

    def mapping(self):
        pairs = self.entries()
        if len({name for name, _ in pairs}) != len(pairs):
            raise AssertionError('Duplicate workflow mapping key')
        return dict(pairs)

    def steps(self):
        return self.entries(sequence=True)

    def text(self):
        if self.value in ('|', '|-', '>', '>-'):
            return textwrap.dedent('\n'.join(self.lines)).strip()
        if self.lines and any(line.strip() and not line.lstrip().startswith('#') for line in self.lines):
            raise AssertionError('Expected a scalar workflow value')
        return scalar(self.value)

    def names(self):
        value = self.text() if self.value else ''
        if value.startswith('[') and value.endswith(']'):
            names = [scalar(item) for item in value[1:-1].split(',') if item.strip()]
        elif value:
            names = [value]
        else:
            names = []
            for line in self.lines:
                line = uncomment(line.strip())
                if not line:
                    continue
                if not line.startswith('- '):
                    raise AssertionError('Expected a scalar workflow list')
                names.append(scalar(line[2:]))
        if len(set(names)) != len(names):
            raise AssertionError('Duplicate dependency or trigger branch')
        return set(names)


def workflow(name):
    return Block((WORKFLOWS / name).read_text(encoding='utf-8').splitlines()).mapping()


class WorkflowContracts(unittest.TestCase):
    def assert_unconditional(self, mapping):
        self.assertNotIn('if', mapping, 'Required check must not be filtered or optional')
        self.assertNotIn('continue-on-error', mapping, 'Required failure must propagate')

    def assert_gate(self, job, dependencies, command):
        self.assertEqual(job['if'].text(), 'always()')
        self.assertEqual(job['needs'].names(), set(dependencies))
        self.assertNotIn('continue-on-error', job)
        steps = job['steps'].steps()
        matching = [step for step in steps if 'run' in step and step['run'].text().startswith(command)]
        self.assertEqual(len(matching), 1, 'Gate must execute the result validator exactly once')
        self.assert_unconditional(matching[0])
        self.assertNotIn('||', matching[0]['run'].text(), 'Gate failure must propagate')
        self.assertEqual(matching[0]['env'].mapping()['CHECK_RESULTS'].text(), '${{ toJSON(needs) }}')

    def test_pr_runs_only_on_pull_requests_targeting_main(self):
        data = workflow('core-pr.yml')
        events = data['on'].mapping()
        self.assertEqual(set(events), {'pull_request'})
        event = events['pull_request'].mapping()
        self.assertEqual(set(event), {'branches'})
        self.assertEqual(event['branches'].names(), {'main'})
        jobs = data['jobs'].mapping()
        self.assertEqual(set(jobs), {'checks'})
        checks = jobs['checks'].mapping()
        self.assert_unconditional(checks)
        self.assertEqual(checks['uses'].text(), './.github/workflows/core-checks.yml')
        self.assertEqual(set(checks['with'].mapping()), {'full'})
        self.assertEqual(checks['with'].mapping()['full'].text(), 'false')

    def test_main_runs_one_consumer_smoke_configuration_and_full_codeql_after_push(self):
        data = workflow('core-main.yml')
        events = data['on'].mapping()
        self.assertEqual(set(events), {'push'})
        event = events['push'].mapping()
        self.assertEqual(set(event), {'branches'})
        self.assertEqual(event['branches'].names(), {'main'})
        jobs = data['jobs'].mapping()
        self.assertEqual(set(jobs), {'smoke', 'codeql', 'gate', 'timings'})
        codeql = jobs['codeql'].mapping()
        self.assert_unconditional(codeql)
        self.assertEqual(codeql['uses'].text(), './.github/workflows/codeql.yml')
        self.assertEqual(set(codeql['with'].mapping()), {'full'})
        self.assertEqual(codeql['with'].mapping()['full'].text(), 'true',
                         'Main must match the Full CodeQL configuration for default-branch alerts')
        permissions = codeql['permissions'].mapping()
        self.assertEqual({name: value.text() for name, value in permissions.items()},
                         {'actions': 'read', 'contents': 'read', 'security-events': 'write'})
        smoke = jobs['smoke'].mapping()
        self.assert_unconditional(smoke)
        self.assertEqual(smoke['uses'].text(), './.github/workflows/cmake-consumers.yml')
        self.assertEqual(set(smoke['with'].mapping()), {'full'})
        self.assertEqual(smoke['with'].mapping()['full'].text(), 'false')
        self.assert_gate(jobs['gate'].mapping(), {'smoke', 'codeql'},
                         'python -B scripts/core_release.py main-gate --output ')
        worker_jobs = workflow('cmake-consumers.yml')['jobs'].mapping()
        self.assertEqual(set(worker_jobs), {'consumers'})
        consumer = worker_jobs['consumers'].mapping()
        self.assert_unconditional(consumer)
        self.assertEqual(consumer['runs-on'].text(), '${{ matrix.os }}')
        matrix = consumer['strategy'].mapping()['matrix'].mapping()
        self.assertEqual(set(matrix), {'os', 'shared', 'cxx'})
        for axis in matrix:
            expression = re.fullmatch(
                r"\$\{\{\s*fromJSON\(inputs\.full && '[^']*' \|\| '([^']*)'\)\s*\}\}",
                matrix[axis].text())
            self.assertIsNotNone(expression, f'Unrecognized conditional {axis} matrix')
            selected = json.loads(expression[1])
            self.assertEqual(len(selected), 1, 'Main must run one consumer configuration')
            if axis == 'os':
                self.assertTrue(selected[0].startswith('ubuntu-'), 'Main smoke must remain on Linux')

    def test_full_requires_manual_dispatch_or_explicit_reuse(self):
        data = workflow('core-full.yml')
        events = data['on'].mapping()
        self.assertEqual(set(events), {'workflow_dispatch', 'workflow_call'})
        self.assertEqual(events['workflow_dispatch'].mapping(), {})
        inputs = events['workflow_call'].mapping()['inputs'].mapping()
        self.assertEqual(set(inputs), {'release_tag'})
        self.assertEqual(inputs['release_tag'].mapping()['type'].text(), 'string')
        self.assertEqual(inputs['release_tag'].mapping()['default'].text(), '')
        jobs = data['jobs'].mapping()
        self.assertEqual(set(jobs), {'checks'})
        checks = jobs['checks'].mapping()
        self.assert_unconditional(checks)
        self.assertEqual(checks['uses'].text(), './.github/workflows/core-checks.yml')
        self.assertEqual(checks['with'].mapping()['full'].text(), 'true')
        self.assertEqual(checks['with'].mapping()['release_tag'].text(), "${{ inputs.release_tag || '' }}")

    def test_release_accepts_only_one_required_manual_tag(self):
        data = workflow('core-release.yml')
        events = data['on'].mapping()
        self.assertEqual(set(events), {'workflow_dispatch'})
        dispatch = events['workflow_dispatch'].mapping()
        self.assertEqual(set(dispatch), {'inputs'})
        inputs = dispatch['inputs'].mapping()
        self.assertEqual(set(inputs), {'tag'})
        tag = inputs['tag'].mapping()
        self.assertEqual(tag['type'].text(), 'string')
        self.assertEqual(tag['required'].text(), 'true')
        self.assertEqual(data['env'].mapping()['RELEASE_TAG'].text(), '${{ inputs.tag }}')

    def test_release_publication_requires_full_and_every_artifact_producer(self):
        jobs = workflow('core-release.yml')['jobs'].mapping()
        producers = {'candidate', 'full', 'performance', 'source', 'binaries'}
        self.assertEqual(set(jobs), producers | {'publish', 'timings'})
        for name in producers | {'publish'}:
            with self.subTest(job=name):
                self.assert_unconditional(jobs[name].mapping())
        full = jobs['full'].mapping()
        self.assertEqual(full['needs'].names(), {'candidate'})
        self.assertEqual(full['uses'].text(), './.github/workflows/core-full.yml')
        self.assertEqual(set(full['with'].mapping()), {'release_tag'})
        self.assertEqual(full['with'].mapping()['release_tag'].text(), '${{ inputs.tag }}')
        self.assertEqual(jobs['performance'].mapping()['needs'].names(), {'candidate', 'full'})
        for name in ('source', 'binaries'):
            self.assertEqual(jobs[name].mapping()['needs'].names(), {'full'})
        self.assertEqual(jobs['publish'].mapping()['needs'].names(), producers)

    def test_main_and_release_collect_timings_after_all_checks_finish(self):
        contracts = (
            ('core-main.yml', {'gate'}, 'core-main-timings'),
            ('core-release.yml', {'candidate', 'full', 'performance', 'source', 'binaries', 'publish'},
             'core-release-timings'),
        )
        for filename, dependencies, artifact in contracts:
            with self.subTest(workflow=filename):
                job = workflow(filename)['jobs'].mapping()['timings'].mapping()
                self.assertEqual(job['if'].text(), 'always()')
                self.assertEqual(job['needs'].names(), dependencies)
                self.assertNotIn('continue-on-error', job)
                self.assertEqual(job['runs-on'].text(), 'ubuntu-24.04')
                permissions = job['permissions'].mapping()
                self.assertEqual(set(permissions), {'contents', 'actions'})
                self.assertEqual(permissions['contents'].text(), 'read')
                self.assertEqual(permissions['actions'].text(), 'read')
                steps = job['steps'].steps()
                for step in steps:
                    self.assert_unconditional(step)
                runs = [step for step in steps if 'run' in step]
                self.assertEqual(len(runs), 1, 'Timing collection must not add another build or analysis')
                self.assertEqual(runs[0]['run'].text(),
                                 'python -B scripts/ci_timings.py --scope run --output build/ci-timings.json')
                self.assertEqual(runs[0]['env'].mapping()['GH_TOKEN'].text(), '${{ github.token }}')
                uploads = [step for step in steps if 'uses' in step
                           and step['uses'].text().startswith('actions/upload-artifact@')]
                self.assertEqual(len(uploads), 1)
                settings = uploads[0]['with'].mapping()
                self.assertEqual(settings['name'].text(),
                                 artifact + '-${{ github.sha }}-${{ github.run_attempt }}')
                self.assertEqual(settings['path'].text(), 'build/ci-timings.json')
                self.assertEqual(settings['if-no-files-found'].text(), 'error')

    def test_release_performance_is_required_and_attests_the_complete_report(self):
        job = workflow('core-release.yml')['jobs'].mapping()['performance'].mapping()
        steps = job['steps'].steps()
        for step in steps:
            self.assert_unconditional(step)
        comparison = [step for step in steps if 'run' in step
                      and step['run'].text().startswith('python -B scripts/core_performance.py run')]
        self.assertEqual(len(comparison), 1)
        command = ' '.join(comparison[0]['run'].text().splitlines())
        self.assertIn('--candidate-record build/candidate/release-candidate.json', command)
        self.assertIn('--output build/release-performance', command)
        records = [step for step in steps if 'uses' in step
                   and step['uses'].text().startswith('actions/download-artifact@')]
        self.assertEqual(len(records), 1)
        self.assertEqual(records[0]['with'].mapping()['name'].text(),
                         'release-candidate-${{ github.run_attempt }}')
        expected = {f'build/release-performance/{name}' for name in
                    ('performance.json', 'performance.md', 'performance-data.tar.gz')}
        for action, field in (('actions/attest@', 'subject-path'), ('actions/upload-artifact@', 'path')):
            matches = [step for step in steps if 'uses' in step and step['uses'].text().startswith(action)]
            self.assertEqual(len(matches), 1)
            self.assertEqual(set(matches[0]['with'].mapping()[field].text().splitlines()), expected)

    def test_publisher_rechecks_the_original_candidate_before_manifest_and_upload(self):
        steps = workflow('core-release.yml')['jobs'].mapping()['publish'].mapping()['steps'].steps()
        runs = [step for step in steps if 'run' in step]
        preflight = [step for step in runs
                     if step['run'].text().startswith('python -B scripts/core_release.py preflight')]
        self.assertEqual(len(preflight), 1)
        self.assert_unconditional(preflight[0])
        command = preflight[0]['run'].text()
        self.assertIn('--locked-candidate "build/downloads/release-candidate-$ATTEMPT/release-candidate.json"',
                      command)
        self.assertIn('python -B scripts/core_release.py manifest --output build/publish', command)
        publication = [step for step in runs if 'gh release create ' in step['run'].text()]
        self.assertEqual(len(publication), 1)
        self.assert_unconditional(publication[0])
        self.assertLess(steps.index(preflight[0]), steps.index(publication[0]))

    def test_core_quality_gate_covers_the_entire_check_inventory(self):
        jobs = workflow('core-checks.yml')['jobs'].mapping()
        self.assertEqual(set(jobs), CORE_CHECKS | {'gate', 'timings'})
        self.assert_gate(jobs['gate'].mapping(), CORE_CHECKS,
                         'python -B scripts/core_release.py gate --output ')
        for name in CORE_CHECKS:
            with self.subTest(check=name):
                job = jobs[name].mapping()
                self.assertNotIn('continue-on-error', job)
                if name in PR_CHECKS:
                    self.assert_unconditional(job)
                else:
                    self.assertEqual(job['if'].text(), 'inputs.full')
        self.assertEqual(jobs['static'].mapping()['uses'].text(), './.github/workflows/static-analysis.yml')
        self.assertEqual(set(jobs['static'].mapping()['with'].mapping()), {'release_tag'})
        self.assertEqual(jobs['codeql'].mapping()['uses'].text(), './.github/workflows/codeql.yml')
        codeql = workflow('codeql.yml')['jobs'].mapping()
        self.assertEqual(set(codeql), {'analyze'})
        self.assert_unconditional(codeql['analyze'].mapping())

    def test_codeql_traces_first_party_tests_but_not_fetched_googletest(self):
        steps = workflow('codeql.yml')['jobs'].mapping()['analyze'].mapping()['steps'].steps()
        names = [step['name'].text() for step in steps]
        order = ['Configure CMake', 'Build third-party test dependencies outside CodeQL',
                 'Initialize CodeQL', 'Build', 'Perform CodeQL analysis']
        self.assertEqual([name for name in names if name in order], order)
        step = dict(zip(names, steps))
        for name in order:
            self.assert_unconditional(step[name])
        self.assertIn('-DOPENTLV_BUILD_TESTS=ON', step['Configure CMake']['run'].text())
        self.assertEqual(step['Build third-party test dependencies outside CodeQL']['run'].text(),
                         'cmake --build build --parallel --target gtest gtest_main gmock gmock_main')
        init = step['Initialize CodeQL']['with'].mapping()
        self.assertEqual(set(init), {'languages', 'queries'}, 'No CodeQL query filters')
        self.assertEqual(init['queries'].text(), "${{ inputs.full && 'security-extended' || '' }}")
        build = step['Build']['run'].text()
        self.assertTrue(build.startswith('set -o pipefail\n'))
        self.assertIn("grep -E -- ' -c [^ ]*/_deps/' build/codeql-build.log", build)
        self.assertIn('exit 1', build)
        self.assertEqual(step['Perform CodeQL analysis']['with'].mapping()['category'].text(),
                         '/language:cpp')

    def test_static_checks_have_no_tier_or_path_filter(self):
        data = workflow('static-analysis.yml')
        events = data['on'].mapping()
        self.assertEqual(set(events), {'workflow_call', 'workflow_dispatch'})
        self.assertEqual(events['workflow_dispatch'].mapping(), {})
        self.assertEqual(set(events['workflow_call'].mapping()['inputs'].mapping()), {'release_tag'})
        jobs = data['jobs'].mapping()
        self.assertEqual(set(jobs), STATIC_CHECKS | {'gate'})
        for name in STATIC_CHECKS:
            with self.subTest(check=name):
                job = jobs[name].mapping()
                self.assert_unconditional(job)
                self.assertEqual(job['runs-on'].text(), 'ubuntu-24.04')
                for step in job['steps'].steps():
                    self.assertNotIn('continue-on-error', step)
        self.assert_gate(jobs['gate'].mapping(), STATIC_CHECKS,
                         'python -B scripts/core_release.py static-gate')
        gate_steps = jobs['gate'].mapping()['steps'].steps()
        validator = next(step for step in gate_steps if 'run' in step)
        self.assertEqual(set(validator['env'].mapping()), {'CHECK_RESULTS'})

    def test_analyzer_commands_cannot_be_skipped_or_hide_failure(self):
        jobs = workflow('static-analysis.yml')['jobs'].mapping()
        for analyzer in ('cppcheck', 'clang-tidy'):
            with self.subTest(analyzer=analyzer):
                steps = jobs[analyzer].mapping()['steps'].steps()
                candidates = [step for step in steps if 'run' in step
                              and f'--target {analyzer}' in step['run'].text()]
                self.assertEqual(len(candidates), 1)
                self.assert_unconditional(candidates[0])
                self.assertEqual(candidates[0]['run'].text().splitlines(), [
                    'set -o pipefail',
                    f'cmake --build build --target {analyzer} 2>&1 | tee "$RUNNER_TEMP/{analyzer}.log"'])

    def test_static_pre_commit_runs_the_workflow_and_release_regressions(self):
        steps = workflow('static-analysis.yml')['jobs'].mapping()['pre-commit'].mapping()['steps'].steps()
        hooks = [step for step in steps if 'uses' in step
                 and step['uses'].text().startswith('pre-commit/action@')]
        self.assertEqual(len(hooks), 1)
        self.assert_unconditional(hooks[0])
        required = {'python -B scripts/test_core_release.py',
                    'python -B scripts/test_core_performance.py',
                    'python -B scripts/test_ci_workflows.py'}
        commands = set()
        for step in steps:
            if 'run' in step and required.intersection(step['run'].text().splitlines()):
                self.assert_unconditional(step)
                commands.update(step['run'].text().splitlines())
        self.assertLessEqual(required, commands)

    def test_legacy_trigger_wrappers_are_removed(self):
        for name in ('core-fast.yml', 'benchmarks.yml', 'callgrind.yml'):
            with self.subTest(workflow=name):
                self.assertFalse((WORKFLOWS / name).exists())

    def test_mapping_reader_ignores_comments_and_nested_script_keys(self):
        data = Block(textwrap.dedent('''\
            # push: is only a comment
            "on":
              pull_request:
                branches: [main] # push: is still a comment
            jobs:
              check:
                steps:
                  - run: |
                      echo 'push: trigger'
                      on: pretend
        ''').splitlines()).mapping()
        self.assertEqual(set(data['on'].mapping()), {'pull_request'})
        self.assertEqual(set(data['jobs'].mapping()), {'check'})
        self.assertEqual(data['on'].mapping()['pull_request'].mapping()['branches'].names(), {'main'})
        self.assertEqual(uncomment("'literal # hash' # comment"), "'literal # hash'")


if __name__ == '__main__':
    unittest.main()
