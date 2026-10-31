# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Marek Cingel

"""Fail-closed Core CI evidence and release artifact assembly (standard library)."""

import argparse
from datetime import date
import gzip
import hashlib
import io
import json
import os
from pathlib import Path
import platform
import re
import subprocess
import tarfile
import urllib.parse
import urllib.request


PR = frozenset(('clang', 'static', 'consumers', 'codeql'))
FULL = PR | frozenset(('msvc', 'gcc', 'properties', 'query', 'fuzz', 'capabilities', 'abi', 'memcheck'))
WORKERS = {'clang': 'build-clang.yml', 'msvc': 'build-msvc.yml',
           'static': 'static-analysis.yml', 'consumers': 'cmake-consumers.yml',
           'codeql': 'codeql.yml', 'gcc': 'build-gcc.yml', 'properties': 'property.yml',
           'query': 'query-hardening.yml', 'fuzz': 'fuzz.yml',
           'capabilities': 'capabilities.yml', 'abi': 'abi.yml', 'memcheck': 'valgrind.yml'}
PERFORMANCE_FILES = frozenset(('performance.json', 'performance.md', 'performance-data.tar.gz'))
VERSION = re.compile(r'(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)(?:-([0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?')
BINARY_IDS = frozenset(('linux-x64-static', 'linux-x64-shared',
                        'windows-x64-static', 'windows-x64-shared'))


def require(condition, message):
    if not condition:
        raise ValueError(message)


def git(*args):
    return subprocess.check_output(['git', *args], text=True).strip()


def version(value):
    match = VERSION.fullmatch(value)
    require(match is not None, 'Expected a SemVer version number without the tag prefix')
    for part in (match[4] or '').split('.'):
        require(not (part.isdigit() and len(part) > 1 and part[0] == '0'),
                'Numeric prerelease identifiers cannot have leading zeroes')
    return value


def release_tag(value):
    require(isinstance(value, str) and value.startswith('v'), 'Release tag must have the form v0.1.0')
    return version(value[1:])


def check_changelog(text, tag):
    release_tag(tag)
    require(re.search(r'\bUnreleased\b', text, re.I) is None,
            'Close CHANGELOG.md before releasing: Unreleased is forbidden')
    headings = list(re.finditer(r'^##[ \t]+([^\r\n]+)', text, re.M))
    latest = re.fullmatch(r'\[([^\]]+)\] - (\d{4}-\d{2}-\d{2})', headings[0][1]) if headings else None
    require(latest is not None and latest[1] == tag,
            'Latest changelog section must match the exact release tag and include YYYY-MM-DD')
    require(sum(item[1].startswith('[' + tag + ']') for item in headings) == 1,
            'Duplicate release changelog section')
    date.fromisoformat(latest[2])
    end = headings[1].start() if len(headings) > 1 else len(text)
    require(re.search(r'^[-*] \S', text[headings[0].end():end], re.M) is not None,
            'Release changelog section must contain changes')


def prepare(tag):
    release_tag(tag)
    commit = revision(os.environ['GITHUB_SHA'])
    require(git('rev-parse', 'HEAD') == commit, 'Wrong candidate checkout')
    # A private lightweight tag gives every build the proposed version without
    # publishing anything. Remote tag creation is the final publisher's job.
    subprocess.run(['git', 'tag', tag, commit], check=True)


def revision(value):
    require(re.fullmatch('[0-9a-f]{40}', value or '') is not None, 'Expected exact commit SHA')
    return value


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write_json(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, indent=2) + '\n', encoding='utf-8')


def check_results(results, full):
    require(isinstance(results, dict) and set(results) == FULL, 'Missing or unexpected CI jobs')
    selected = FULL if full else PR
    for name, result in results.items():
        expected = 'success' if name in selected else 'skipped'
        actual = result.get('result') if isinstance(result, dict) else None
        require(actual == expected, f'{name}: expected {expected}, got {actual}')


def check_static_results(results):
    require(isinstance(results, dict) and set(results) == {'pre-commit', 'cppcheck', 'clang-tidy'},
            'Missing or unexpected static analysis jobs')
    for name, result in results.items():
        actual = result.get('result') if isinstance(result, dict) else None
        require(actual == 'success', f'{name}: expected success, got {actual}')


def static_gate():
    check_static_results(json.loads(os.environ['CHECK_RESULTS']))


def configuration(tier):
    selected = FULL if tier == 'full' else PR if tier == 'pr' else {'consumers', 'codeql'}
    paths = {WORKERS[name] for name in selected}
    paths.add('core-' + tier + '.yml')
    if tier != 'main':
        paths.add('core-checks.yml')
    if tier == 'full' and os.environ.get('RELEASE_TAG'):
        paths.add('core-release.yml')
    source = Path(__file__).resolve().parents[1]
    root = source / '.github/workflows'
    # Retain the exact matrix expressions, compiler options and steps as tested,
    # rather than a second hand-maintained description that can drift from YAML.
    if tier == 'main':
        # Main CodeQL repeats Full's analysis so default-branch alerts track it.
        inputs = {'smoke': {'full': False}, 'codeql': {'full': True}}
    else:
        inputs = {'full': tier == 'full', 'release_tag': os.environ.get('RELEASE_TAG', '')}
    helpers = ('CMakeLists.txt', 'cmake/compiler.cmake', 'cmake/modules.cmake',
               'tlv/CMakeLists.txt', 'tlv/modules.cmake',
               'scripts/check_cmake_integration.py',
               'scripts/check_capabilities.py', 'scripts/abi/check.py')
    return {'inputs': inputs, 'configuration_sources': {name: digest(source / name) for name in helpers},
            'workflows': {
        name: {'sha256': digest(root / name), 'definition': (root / name).read_text(encoding='utf-8')}
        for name in sorted(paths)}}


def record_gate(output, tier, results):
    commit = revision(os.environ['GITHUB_SHA'])
    require(git('rev-parse', 'HEAD') == commit, 'Checkout differs from CI revision')
    write_json(output, {'schema': 2, 'tier': tier,
                        'commit': commit, 'repository': os.environ['GITHUB_REPOSITORY'],
                        'run_id': os.environ['GITHUB_RUN_ID'],
                        'run_attempt': os.environ['GITHUB_RUN_ATTEMPT'],
                        'configuration': configuration(tier),
                        'checks': {name: item['result'] for name, item in results.items()}})


def main_gate(output):
    results = json.loads(os.environ['CHECK_RESULTS'])
    require(isinstance(results, dict) and set(results) == {'smoke', 'codeql'},
            'Missing or unexpected Main CI jobs')
    for name in ('smoke', 'codeql'):
        require(isinstance(results[name], dict) and results[name].get('result') == 'success',
                'Main integration smoke tests and CodeQL must succeed')
    record_gate(output, 'main', results)


def gate(output):
    require(os.environ['FULL'] in ('true', 'false'), 'FULL must be true or false')
    full = os.environ['FULL'] == 'true'
    results = json.loads(os.environ['CHECK_RESULTS'])
    check_results(results, full)
    record_gate(output, 'full' if full else 'pr', results)


def api(path):
    request = urllib.request.Request('https://api.github.com/' + path, headers={
        'Authorization': 'Bearer ' + os.environ['GH_TOKEN'],
        'Accept': 'application/vnd.github+json', 'X-GitHub-Api-Version': '2022-11-28'})
    with urllib.request.urlopen(request, timeout=60) as response:
        return json.load(response)


def query_evidence_run(runs, commit):
    candidates = [run for run in runs if run.get('head_sha') == commit
                  and run.get('path') == '.github/workflows/query-release.yml'
                  and run.get('event') == 'workflow_dispatch']
    require(candidates, 'A successful Query release candidate run is required for 1.0 and later')
    latest = max(candidates, key=lambda run: run['id'])
    require(latest['status'] == 'completed' and latest['conclusion'] == 'success',
            'Latest Query release candidate run did not pass')
    return latest['id']


def stable_version(tag):
    number = tag.removeprefix('v') if isinstance(tag, str) else ''
    match = VERSION.fullmatch(number)
    return tuple(int(match[i]) for i in (1, 2, 3)) if match and match[4] is None else None


def select_baseline(releases, candidate_version):
    candidate = tuple(int(part) for part in candidate_version.split('-')[0].split('.'))
    eligible = [item for item in releases
                if item.get('draft') is False and item.get('prerelease') is False
                and item.get('published_at') and stable_version(item.get('tag_name')) is not None
                and stable_version(item['tag_name']) < candidate]
    require(eligible, 'No previous published stable release is available for performance verification')
    return max(eligible, key=lambda item: (stable_version(item['tag_name']), item['id']))


def baseline_record(published, candidate_commit):
    tag = published['tag_name']
    require(stable_version(tag) is not None and published.get('draft') is False
            and published.get('prerelease') is False and published.get('published_at'),
            'Performance baseline must be a published stable release')
    require(type(published.get('id')) is int and published['id'] > 0, 'Invalid baseline release ID')
    commit = revision(git('rev-parse', f'refs/tags/{tag}^{{commit}}'))
    remote = git('ls-remote', '--tags', 'origin', f'refs/tags/{tag}', f'refs/tags/{tag}^{{}}')
    refs = dict(line.split()[::-1] for line in remote.splitlines())
    require(refs.get(f'refs/tags/{tag}^{{}}', refs.get(f'refs/tags/{tag}')) == commit,
            'Baseline tag changed or is missing from the remote')
    require(commit != candidate_commit, 'Performance baseline cannot be the candidate itself')
    subprocess.run(['git', 'merge-base', '--is-ancestor', commit, candidate_commit], check=True)
    return {'release_id': published['id'], 'tag': tag, 'version': tag.removeprefix('v'),
            'commit': commit, 'url': published['html_url'], 'published_at': published['published_at']}


def release_baseline(number, commit, locked=None):
    repo = os.environ['GITHUB_REPOSITORY']
    if locked is not None:
        require(type(locked.get('release_id')) is int and locked['release_id'] > 0,
                'Missing locked baseline release ID')
        published = api(f'repos/{repo}/releases/{locked["release_id"]}')
        record = baseline_record(published, commit)
        require(record == locked, 'Locked performance baseline has changed')
        return record
    releases = []
    page = 1
    while True:
        items = api(f'repos/{repo}/releases?per_page=100&page={page}')
        require(isinstance(items, list), 'Invalid releases response')
        releases.extend(items)
        if len(items) < 100:
            break
        page += 1
    return baseline_record(select_baseline(releases, number), commit)


def preflight(output, locked_path=None):
    require(os.environ['GITHUB_EVENT_NAME'] == 'workflow_dispatch', 'Publication requires manual dispatch')
    require(os.environ['GITHUB_REF'] == 'refs/heads/main', 'Dispatch Core Release from main')
    tag = os.environ['RELEASE_TAG']
    number = release_tag(tag)
    check_changelog(Path('CHANGELOG.md').read_text(encoding='utf-8'), tag)
    commit = revision(os.environ['GITHUB_SHA'])
    require(git('rev-parse', 'HEAD') == commit, 'Wrong release checkout')
    remote = git('ls-remote', '--tags', 'origin', f'refs/tags/{tag}', f'refs/tags/{tag}^{{}}')
    require(not remote, 'Release tag already exists; existing tags are immutable')
    subprocess.run(['git', 'merge-base', '--is-ancestor', commit, 'origin/main'], check=True)
    require(not git('status', '--porcelain'), 'Release checkout must be clean')
    evidence = None
    if int(number.split('.')[0]) >= 1:
        repo = os.environ['GITHUB_REPOSITORY']
        query = urllib.parse.urlencode({'head_sha': commit, 'per_page': 100})
        runs = api(f'repos/{repo}/actions/workflows/query-release.yml/runs?{query}')
        evidence = query_evidence_run(runs['workflow_runs'], commit)
    locked = json.loads(locked_path.read_text(encoding='utf-8')) if locked_path else None
    identity = {'tag': tag, 'version': number, 'commit': commit,
                'repository': os.environ['GITHUB_REPOSITORY'],
                'run_id': os.environ['GITHUB_RUN_ID'], 'run_attempt': os.environ['GITHUB_RUN_ATTEMPT']}
    if locked is not None:
        require(all(locked.get(key) == value for key, value in identity.items()),
                'Locked candidate identity differs from this run and attempt')
        require(isinstance(locked.get('baseline'), dict), 'Locked performance baseline is missing')
    baseline = release_baseline(number, commit, locked['baseline'] if locked is not None else None)
    write_json(output, {**identity, 'query_evidence_run_id': evidence, 'baseline': baseline})


def source_archive(output, tag, commit):
    version(tag)
    revision(commit)
    require(git('rev-parse', 'HEAD') == commit, 'Source checkout differs from release commit')
    require(not git('status', '--porcelain'), 'Source archive requires a clean checkout')
    prefix = f'OpenTLV-{tag}/'
    data = subprocess.check_output(['git', 'archive', '--format=tar', '--prefix=' + prefix, commit])
    with tarfile.open(fileobj=io.BytesIO(data)) as original:
        for name in ('LICENSE', 'LICENSES/MIT.txt', 'REUSE.toml', 'THIRD_PARTY.md'):
            member = next((item for item in original.getmembers() if item.name == prefix + name), None)
            require(member is not None and member.isfile() and member.size > 0,
                    f'Source archive is missing licensing file: {name}')
    output.mkdir(parents=True, exist_ok=True)
    path = output / f'OpenTLV-{tag}-source.tar.gz'
    # Archive bytes are stable across retries; Git's commit time supplies entry mtimes.
    with path.open('wb') as raw, gzip.GzipFile(filename='', mode='wb', fileobj=raw, mtime=0) as compressed:
        with tarfile.open(fileobj=compressed, mode='w') as target, tarfile.open(fileobj=io.BytesIO(data)) as original:
            for member in original:
                require(member.name != prefix + 'opentlv-source.txt', 'Tracked archive metadata is forbidden')
                target.addfile(member, original.extractfile(member) if member.isfile() else None)
            metadata = f'version={tag}\ncommit={commit}\n'.encode()
            entry = tarfile.TarInfo(prefix + 'opentlv-source.txt')
            entry.size = len(metadata)
            entry.mode = 0o644
            target.addfile(entry, io.BytesIO(metadata))
    return path


def check_quality(quality, commit, run_id, attempt, repository):
    require(quality.get('schema') == 2 and quality.get('tier') == 'full', 'Full CI evidence required')
    require(quality.get('commit') == commit and quality.get('repository') == repository,
            'Wrong source identity in Full CI evidence')
    require(quality.get('run_id') == run_id and quality.get('run_attempt') == attempt,
            'Full CI evidence must come from this release run and attempt')
    check_results({name: {'result': result} for name, result in quality.get('checks', {}).items()}, True)
    require(quality.get('configuration') == configuration('full'),
            'Full CI configuration evidence is missing or differs from the release checkout')


def binary_record(build, output, identifier, tag, commit):
    # check_package.py produces these archives only after install/consumer verification.
    from check_package import compiler_settings, read_cache
    require(identifier in BINARY_IDS, 'Unapproved binary configuration')
    cache = read_cache(build)
    require(cache['CMAKE_BUILD_TYPE'] == 'Release', 'Binary must use Release configuration')
    require(cache['OPENTLV_BUILD_CXX'] == 'ON', 'Release binaries must include the C++ headers')
    header = (build / 'generated/include/tlv/version.h').read_text()
    require(f'#define OPENTLV_VERSION_STRING "{version(tag)}"' in header,
            'Compiled version differs from the release tag')
    compiled_commit = re.search(r'#define OPENTLV_GIT_COMMIT_HASH "([0-9a-f]+)"', header)
    require(compiled_commit is not None and len(compiled_commit[1]) >= 7
            and revision(commit).startswith(compiled_commit[1]), 'Compiled source revision mismatch')
    cmake_version = '.'.join(cache[f'CMAKE_CACHE_{part}_VERSION'] for part in ('MAJOR', 'MINOR', 'PATCH'))
    compiler_file = build / 'CMakeFiles' / cmake_version / 'CMakeCCompiler.cmake'
    require('set(CMAKE_C_SIZEOF_DATA_PTR "8")' in compiler_file.read_text(),
            'Approved binary requires a 64-bit compiler target')
    system_file = (build / 'CMakeFiles' / cmake_version / 'CMakeSystem.cmake').read_text()
    processor = re.search(r'set\(CMAKE_SYSTEM_PROCESSOR "([^"]+)"\)', system_file)
    system = re.search(r'set\(CMAKE_SYSTEM_NAME "([^"]+)"\)', system_file)
    require(processor is not None and processor[1].lower() in ('amd64', 'x86_64', 'x64'),
            'Approved binary requires an x64 target, not merely a 64-bit pointer')
    expected_system = 'Windows' if identifier.startswith('windows-') else 'Linux'
    require(system is not None and system[1] == expected_system, 'Binary operating system mismatch')
    if expected_system == 'Windows':
        require(cache.get('CMAKE_MSVC_RUNTIME_LIBRARY') == 'MultiThreadedDLL',
                'Windows release binaries require the dynamic MSVC runtime')
    linkage = 'shared' if cache['OPENTLV_BUILD_SHARED_LIBS'] == 'ON' else 'static'
    require(identifier.endswith('-' + linkage), 'Binary linkage mismatch')
    archives = list((build / 'packages').glob('*.zip')) + list((build / 'packages').glob('*.tar.gz'))
    require(len(archives) == 1, 'Expected one freshly verified binary archive')
    archive = archives[0]
    suffix = '.zip' if archive.suffix == '.zip' else '.tar.gz'
    output.mkdir(parents=True, exist_ok=True)
    target = output / f'OpenTLV-{version(tag)}-{identifier}{suffix}'
    target.write_bytes(archive.read_bytes())
    compiler = compiler_settings(build, cache, 'C')
    require(compiler['CMAKE_C_COMPILER_ID'] == ('MSVC' if expected_system == 'Windows' else 'GNU'),
            'Compiler is outside the approved binary matrix')
    record = {'schema': 1, 'kind': 'binary', 'id': identifier, 'version': tag,
              'commit': revision(commit), 'filename': target.name, 'sha256': digest(target),
              'os': identifier.split('-')[0], 'architecture': 'x64', 'linkage': linkage,
              'build_type': 'Release', 'compiler': compiler,
              'msvc_runtime': cache.get('CMAKE_MSVC_RUNTIME_LIBRARY') if identifier.startswith('windows-') else None,
              'libc': platform.libc_ver() if expected_system == 'Linux' else None,
              'runner_image': {'os': os.environ.get('ImageOS'), 'version': os.environ.get('ImageVersion')},
              'options': {key: val for key, val in cache.items() if key.startswith('OPENTLV_')}}
    write_json(output / (identifier + '.json'), record)


def manifest(directory, quality_path, tag, commit):
    from core_performance import validate_report
    version(tag)
    revision(commit)
    quality = json.loads(quality_path.read_text())
    check_quality(quality, commit, os.environ['GITHUB_RUN_ID'], os.environ['GITHUB_RUN_ATTEMPT'],
                  os.environ['GITHUB_REPOSITORY'])
    candidate = json.loads((quality_path.parent / 'release-candidate.json').read_text())
    require(candidate.get('version') == tag and candidate.get('commit') == commit,
            'Release candidate identity mismatch')
    require(candidate.get('tag') == 'v' + tag, 'Release candidate tag mismatch')
    require(all(candidate.get(key) == os.environ['GITHUB_' + env] for key, env in
                (('repository', 'REPOSITORY'), ('run_id', 'RUN_ID'), ('run_attempt', 'RUN_ATTEMPT'))),
            'Release candidate must come from this repository, run and attempt')
    if int(tag.split('.')[0]) >= 1:
        query_run = candidate.get('query_evidence_run_id')
        require(type(query_run) is int and query_run > 0, 'Query candidate evidence reference is missing')
    records = []
    for identifier in sorted(BINARY_IDS):
        record = json.loads((directory / (identifier + '.json')).read_text())
        require(record.get('id') == identifier and record.get('kind') == 'binary'
                and record.get('version') == tag and record.get('commit') == commit,
                'Wrong binary artifact identity')
        name = record['filename']
        suffix = '.zip' if identifier.startswith('windows-') else '.tar.gz'
        require(name == f'OpenTLV-{tag}-{identifier}{suffix}', 'Unexpected binary filename')
        require(digest(directory / name) == record['sha256'], 'Binary checksum mismatch')
        records.append(record)
    source = directory / f'OpenTLV-{tag}-source.tar.gz'
    with tarfile.open(source) as archive:
        metadata = archive.extractfile(f'OpenTLV-{tag}/opentlv-source.txt')
        require(metadata is not None and metadata.read() == f'version={tag}\ncommit={commit}\n'.encode(),
                'Source archive identity mismatch')
    records.append({'kind': 'source', 'filename': source.name, 'sha256': digest(source),
                    'version': tag, 'commit': commit})
    performance = validate_report(directory, commit, 'v' + tag, os.environ['GITHUB_REPOSITORY'],
                                  os.environ['GITHUB_RUN_ID'], os.environ['GITHUB_RUN_ATTEMPT'],
                                  candidate['baseline'])
    records.extend({'kind': 'performance', 'filename': name, 'sha256': digest(directory / name),
                    'version': tag, 'commit': commit} for name in sorted(PERFORMANCE_FILES))
    expected = {r['filename'] for r in records} | {name + '.json' for name in BINARY_IDS}
    require({p.name for p in directory.iterdir()} == expected, 'Unexpected or missing release files')
    write_json(directory / 'release-manifest.json', {'schema': 2, 'version': tag, 'commit': commit,
                                                    'candidate': candidate, 'quality': quality,
                                                    'performance': performance, 'artifacts': records})
    files = sorted(p for p in directory.iterdir() if p.is_file())
    (directory / 'SHA256SUMS').write_text(''.join(f'{digest(p)}  {p.name}\n' for p in files), encoding='utf-8')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='command', required=True)
    for name in ('gate', 'main-gate'):
        sub.add_parser(name).add_argument('--output', type=Path, required=True)
    preflight_parser = sub.add_parser('preflight')
    preflight_parser.add_argument('--output', type=Path, required=True)
    preflight_parser.add_argument('--locked-candidate', type=Path)
    sub.add_parser('static-gate')
    sub.add_parser('prepare').add_argument('--tag', default=os.environ.get('RELEASE_TAG'), required=False)
    for name in ('source', 'binary', 'manifest'):
        command = sub.add_parser(name)
        command.add_argument('--output', type=Path, required=True)
        command.add_argument('--version')
        command.add_argument('--commit', default=os.environ.get('GITHUB_SHA'))
        if name == 'binary':
            command.add_argument('--build', type=Path, required=True)
            command.add_argument('--id', required=True)
        if name == 'manifest':
            command.add_argument('--quality', type=Path, required=True)
    args = parser.parse_args()
    if args.command == 'gate':
        gate(args.output)
    elif args.command == 'main-gate':
        main_gate(args.output)
    elif args.command == 'static-gate':
        static_gate()
    elif args.command == 'preflight':
        preflight(args.output, args.locked_candidate)
    elif args.command == 'prepare':
        prepare(args.tag)
    elif args.command == 'source':
        source_archive(args.output, args.version or release_tag(os.environ.get('RELEASE_TAG')), args.commit)
    elif args.command == 'binary':
        binary_record(args.build, args.output, args.id,
                      args.version or release_tag(os.environ.get('RELEASE_TAG')), args.commit)
    elif args.command == 'manifest':
        manifest(args.output, args.quality, args.version or release_tag(os.environ.get('RELEASE_TAG')), args.commit)


if __name__ == '__main__':
    main()
