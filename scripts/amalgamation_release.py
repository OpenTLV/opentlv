# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Marek Cingel

"""Package and verify the tested, reproducible C source release distribution."""

import argparse
import gzip
import hashlib
import io
import json
import os
from pathlib import Path
import re
import shutil
import tarfile

from archive_safety import check_archive
from check_amalgamation_consumers import compile_object, run
from core_release import ARTIFACT_CONTRACT, digest, git, require, revision, version, write_json


ROOT = Path(__file__).resolve().parents[1]
FEATURES = {'query_frontend': True, 'query_set_operations': True}
EXTRAS = {name: name for name in ('LICENSE', 'LICENSES/MIT.txt', 'THIRD_PARTY.md')}
EXTRAS.update({f'examples/{name}': f'tests/amalgamation/consumer/{name}'
               for name in ('consumer.c', 'consumer.cpp', 'README.md')})


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def payload(generated, source=ROOT):
    files = {}
    for path in sorted(generated.rglob('*')):
        require(not path.is_symlink(), 'Amalgamation cannot contain links')
        if path.is_file():
            files[path.relative_to(generated).as_posix()] = path.read_bytes()
    for name, path in EXTRAS.items():
        require(name not in files, 'Generated output shadows a notice or sample')
        files[name] = (source / path).read_bytes().replace(b'\r\n', b'\n')
    return files


def check_metadata(metadata, number, commit):
    require(metadata.get('schema') == 1 and metadata.get('library') == 'OpenTLV',
            'Unsupported amalgamation metadata')
    short = metadata.get('commit')
    require(isinstance(short, str) and re.fullmatch('[0-9a-f]{7,40}', short)
            and commit.startswith(short), 'Amalgamation commit mismatch')
    require(metadata.get('version') == number and metadata.get('tag') == 'v' + number
            and type(metadata.get('revision')) is int and metadata['revision'] == 0,
            'Amalgamation release version mismatch')
    require(metadata.get('configuration') == FEATURES
            and all(type(value) is bool for value in metadata['configuration'].values()),
            'Unapproved amalgamation Query configuration')
    require(metadata.get('targets') and metadata.get('inputs'), 'Missing amalgamation inventory')


def check_equivalence(report, files, commit):
    require(report.get('status') == 'passed' and report.get('commit') == commit
            and report.get('working_tree') == '', 'Clean matching distribution evidence required')
    variants = report.get('variants', {})
    require(set(variants) == {'conventional', 'installed', 'amalgamation'},
            'Missing distribution variants')
    for item in variants.values():
        require(item.get('status') == 'passed' and all(type(item.get(key)) is int and item[key] > 0
                for key in ('ctest', 'gtest', 'fuzz_targets')), 'Incomplete distribution tests')
    require(len({tuple(item[key] for key in ('ctest', 'gtest', 'fuzz_targets'))
                 for item in variants.values()}) == 1, 'Distribution coverage differs')
    expected = {name: sha256(data) for name, data in files.items() if name not in EXTRAS}
    require(report.get('artifact_hashes') == expected,
            'Release sources differ from the distribution-tested bytes')


def archive_bytes(files, root):
    output = io.BytesIO()
    with gzip.GzipFile(filename='', mode='wb', fileobj=output, mtime=0) as compressed:
        with tarfile.open(fileobj=compressed, mode='w', format=tarfile.PAX_FORMAT) as archive:
            for name, data in sorted(files.items()):
                entry = tarfile.TarInfo(root + '/' + name)
                entry.size = len(data)
                entry.mode = 0o644
                archive.addfile(entry, io.BytesIO(data))
    return output.getvalue()


def read_archive(path, root):
    check_archive(path, root)
    files = {}
    with tarfile.open(path) as archive:
        for entry in archive:
            require(entry.isfile(), 'Amalgamation archives contain only regular files')
            name = entry.name.removeprefix(root + '/')
            require(entry.name == root + '/' + name and name not in files,
                    'Noncanonical amalgamation path')
            files[name] = archive.extractfile(entry).read()
    return files


def verify(directory, number, commit, source=ROOT):
    version(number)
    revision(commit)
    root = f'OpenTLV-{number}-amalgamation'
    record = json.loads((directory / 'amalgamation.json').read_text(encoding='utf-8'))
    require(record.get('schema') == 1 and record.get('artifact_contract') == ARTIFACT_CONTRACT
            and record.get('kind') == 'amalgamation' and record.get('version') == number
            and record.get('commit') == commit and record.get('archive_root') == root
            and record.get('filename') == root + '.tar.gz', 'Amalgamation record identity mismatch')
    path = directory / record['filename']
    require(digest(path) == record.get('sha256') == record.get('reproduction_sha256'),
            'Amalgamation checksum or reproduction mismatch')
    files = read_archive(path, root)
    require(record.get('files') == {name: sha256(data) for name, data in files.items()},
            'Amalgamation file checksum inventory mismatch')
    for name, original in EXTRAS.items():
        require(files.get(name) == (source / original).read_bytes().replace(b'\r\n', b'\n'),
                'Missing or changed amalgamation notice/sample: ' + name)
    require({'opentlv.c', 'opentlv.h', 'opentlv-amalgamation.json',
             'include/opentlv/tlv.hpp'} <= files.keys(), 'Missing amalgamation sources or headers')
    require(all(name in EXTRAS or name in ('opentlv.c', 'opentlv.h', 'opentlv-amalgamation.json')
                or name.startswith('include/opentlv/') for name in files), 'Unexpected amalgamation file')
    metadata = json.loads(files['opentlv-amalgamation.json'])
    check_metadata(metadata, number, commit)
    require(record.get('metadata') == metadata, 'Amalgamation metadata differs from record')
    check_equivalence(record.get('equivalence', {}), files, commit)
    require(record.get('consumers') == ['c99', 'c++17'], 'Missing standalone consumer checks')
    # This also rejects timestamp/owner/mode changes, extra TAR metadata and noncanonical paths.
    require(path.read_bytes() == archive_bytes(files, root), 'Non-reproducible archive encoding')
    return record


def consumers(files, work, cc, cxx):
    artifact = work / 'extracted'
    artifact.mkdir(parents=True)
    for name, data in files.items():
        path = artifact / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    msvc = Path(cc).stem.lower() == 'cl'
    implementation = work / 'opentlv.obj'
    compile_object(cc, artifact / 'opentlv.c', implementation, msvc=msvc)
    for language, compiler in (('c', cc), ('cpp', cxx)):
        obj = work / f'{language}.obj'
        includes = [artifact] if language == 'c' else [artifact / 'include']
        compile_object(compiler, artifact / f'examples/consumer.{language}', obj, includes,
                       cxx=language == 'cpp', msvc=msvc)
        binary = work / (language + ('.exe' if os.name == 'nt' else ''))
        flags = ['/nologo', f'/Fe{binary}'] if msvc else ['-o', binary]
        run(compiler, obj, implementation, *flags, cwd=work)
        run(binary, cwd=work)


def produce(output, work, evidence, number, commit, cc, cxx):
    version(number)
    revision(commit)
    require(git('-C', str(ROOT), 'rev-parse', 'HEAD') == commit
            and not git('-C', str(ROOT), 'status', '--porcelain'),
            'Amalgamation release requires the clean candidate checkout')
    require(not work.exists(), 'Use a fresh amalgamation work directory')
    require(not output.exists() or not list(output.iterdir()), 'Release output must be empty')
    cc, cxx = shutil.which(cc), shutil.which(cxx)
    require(cc and cxx, 'C and C++ compiler drivers must be on PATH')
    work = work.resolve()
    work.mkdir(parents=True)
    root = f'OpenTLV-{number}-amalgamation'
    archives = []
    report = json.loads(evidence.read_text(encoding='utf-8'))
    for attempt in ('first', 'second'):
        build = work / attempt
        run('cmake', '-S', ROOT, '-B', build, '-G', 'Ninja', f'-DCMAKE_C_COMPILER={cc}',
            '-DCMAKE_BUILD_TYPE=Release', '-DOPENTLV_BUILD_TESTS=OFF',
            '-DOPENTLV_BUILD_CXX=OFF', '-DOPENTLV_BUILD_EXAMPLES=OFF',
            '-DOPENTLV_BUILD_SHARED_LIBS=OFF',
            '-DOPENTLV_QUERY_FRONTEND=ON', '-DOPENTLV_QUERY_SET_OPERATIONS=ON')
        run('cmake', '--build', build, '--target', 'opentlv_amalgamation')
        files = payload(build / 'amalgamation')
        metadata = json.loads(files['opentlv-amalgamation.json'])
        check_metadata(metadata, number, commit)
        check_equivalence(report, files, commit)
        archives.append(archive_bytes(files, root))
    require(archives[0] == archives[1], 'Amalgamation archives are not reproducible')
    output.mkdir(parents=True, exist_ok=True)
    archive = output / (root + '.tar.gz')
    archive.write_bytes(archives[0])
    files = read_archive(archive, root)
    consumers(files, work, cc, cxx)
    equivalence = {key: report[key] for key in
                   ('status', 'commit', 'working_tree', 'variants', 'artifact_hashes')}
    write_json(output / 'amalgamation.json', {
        'schema': 1, 'artifact_contract': ARTIFACT_CONTRACT, 'kind': 'amalgamation',
        'version': number, 'commit': commit, 'archive_root': root, 'filename': archive.name,
        'sha256': digest(archive), 'reproduction_sha256': sha256(archives[1]),
        'metadata': metadata, 'files': {name: sha256(data) for name, data in files.items()},
        'equivalence': equivalence, 'consumers': ['c99', 'c++17']})
    verify(output, number, commit)
    print(f'Verified reproducible amalgamation: {archive}')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command', choices=('produce', 'verify'))
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--version', required=True)
    parser.add_argument('--commit', required=True)
    parser.add_argument('--work', type=Path)
    parser.add_argument('--equivalence', type=Path)
    parser.add_argument('--cc', default='cc')
    parser.add_argument('--cxx', default='c++')
    args = parser.parse_args()
    if args.command == 'produce':
        require(args.work and args.equivalence, 'Production requires --work and --equivalence')
        produce(args.output, args.work, args.equivalence, args.version, args.commit, args.cc, args.cxx)
    else:
        verify(args.output, args.version, args.commit)


if __name__ == '__main__':
    main()
