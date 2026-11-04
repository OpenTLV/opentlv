# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Marek Cingel

"""Validate package namespaces and links before any archive extraction."""

from pathlib import PurePosixPath
import stat
import tarfile
import zipfile


def require(condition, message):
    if not condition:
        raise ValueError(message)


def parts(name):
    require(isinstance(name, str) and name and not name.startswith('/')
            and not any(char in name for char in ('\\', ':', '\0')),
            'Unsafe archive path or link target')
    return tuple(part for part in name.split('/') if part not in ('', '.'))


def check_archive(path, root):
    """Accept files/directories and safe TAR symlinks; never extract as a probe.

    Hardlinks and special entries are not part of the package contract. No
    archive member may be written below a symlink, even a currently safe one.
    ZIP packages have no supported link representation and must be portable to
    Windows, including case-insensitive path uniqueness.
    """
    require(parts(root) == (root,) and root != '..', 'Invalid expected archive root')
    entries = {}
    links = {}
    aliases = set()
    is_zip = path.suffix == '.zip'

    def add(name, kind, target=None):
        components = parts(name)
        require(components and components[0] == root and '..' not in components,
                'Binary archive root differs from artifact identity or contains an unsafe path')
        key = '/'.join(components)
        alias = key.casefold() if is_zip else key
        require(alias not in aliases, 'Duplicate normalized archive path: ' + key)
        aliases.add(alias)
        require(kind in ('file', 'directory', 'symlink'), 'Unsupported archive entry type: ' + key)
        entries[key] = kind
        if kind == 'symlink':
            links[key] = parts(target)
        if is_zip:
            for component in components:
                require(not any(char in component for char in '<>"|?*') and
                        not component.endswith((' ', '.')) and
                        component.split('.')[0].upper() not in
                        {'CON', 'PRN', 'AUX', 'NUL', *(f'COM{i}' for i in range(1, 10)),
                         *(f'LPT{i}' for i in range(1, 10))}, 'Unsafe Windows archive path')

    if is_zip:
        with zipfile.ZipFile(path) as archive:
            for entry in archive.infolist():
                mode = stat.S_IFMT(entry.external_attr >> 16)
                kind = 'directory' if entry.is_dir() else 'file'
                require(mode in (0, stat.S_IFDIR if entry.is_dir() else stat.S_IFREG),
                        'Unsupported ZIP entry type')
                # filename normalizes backslashes on Windows; inspect raw names.
                add(entry.orig_filename, kind)
            require(archive.testzip() is None, 'Corrupted ZIP entry')
    else:
        with tarfile.open(path) as archive:
            for entry in archive:
                # Exclude sparse/contiguous extensions as well as devices/FIFOs.
                kind = {tarfile.REGTYPE: 'file', tarfile.AREGTYPE: 'file',
                        tarfile.DIRTYPE: 'directory', tarfile.SYMTYPE: 'symlink'}.get(entry.type)
                require(entry.sparse is None, 'Unsupported sparse TAR entry')
                require(entry.size >= 0 and (kind == 'file' or entry.size == 0), 'Invalid TAR entry size')
                add(entry.name, kind, entry.linkname)
                if kind == 'file':
                    stream = archive.extractfile(entry)
                    remaining = entry.size
                    while remaining:
                        chunk = stream.read(min(remaining, 1024 * 1024))
                        require(chunk, 'Truncated TAR entry')
                        remaining -= len(chunk)
            # Consume compression trailers too, detecting bad gzip checksums,
            # truncation and hidden concatenated archives after the TAR end.
            while True:
                trailing = archive.fileobj.read(1024 * 1024)
                if not trailing:
                    break
                require(not trailing.strip(b'\0'), 'Unexpected data after TAR end')

    require(entries and any(name != root for name in entries), 'Empty binary archive root')
    # Materialize implicit directories, rejecting conflicts independently of
    # archive order (file/dir, link/dir, and writes through symlinked ancestors).
    for name in list(entries):
        for parent in PurePosixPath(name).parents:
            key = parent.as_posix()
            if key == '.':
                continue
            require(entries.get(key, 'directory') == 'directory',
                    'Archive entry traverses a non-directory or symlink: ' + name)
            entries.setdefault(key, 'directory')
    require(entries.get(root) == 'directory', 'Archive root must be a directory')
    if is_zip:
        require(len({name.casefold() for name in entries}) == len(entries),
                'Conflicting case-insensitive ZIP directories')

    def resolve(components, active):
        resolved = []
        for index, component in enumerate(components):
            if component == '..':
                require(len(resolved) > 1, 'Symlink escapes archive root')
                resolved.pop()
                continue
            resolved.append(component)
            require(resolved[0] == root, 'Symlink escapes archive root')
            key = '/'.join(resolved)
            require(key in entries, 'Dangling archive symlink: ' + key)
            if key in links:
                require(key not in active, 'Cyclic archive symlink')
                # Resolve before processing '..': lexical normalization could
                # hide a traversal through another link with different depth.
                return resolve(tuple(resolved[:-1]) + links[key] + components[index + 1:], active | {key})
            if index < len(components) - 1:
                require(entries[key] == 'directory', 'Symlink traverses a regular file')
        return '/'.join(resolved)

    for name in links:
        resolve(tuple(name.split('/')), set())
