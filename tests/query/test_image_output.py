# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Marek Cingel
"""Native `image` output: plain new names in the current directory, owner-only."""
import argparse
import os
from pathlib import Path
import stat
import subprocess
import sys
import tempfile
import unittest

NATIVE = None
QUERY, WIRE = "//5A", "5a00"


def image(directory, name, **kwargs):
    return subprocess.run([NATIVE, QUERY, WIRE, "image", name], cwd=directory,
                          capture_output=True, timeout=30, **kwargs).returncode


def tree(root):
    return {(path.relative_to(root).as_posix(), path.read_bytes() if path.is_file() else None)
            for path in Path(root).rglob("*")}


def windows_dacl(path):
    """Return (protected, [(ace type, mask, SID string)]) for a file DACL."""
    import ctypes
    from ctypes import wintypes
    advapi32 = ctypes.WinDLL("advapi32", use_last_error=True)
    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)

    class Acl(ctypes.Structure):
        _fields_ = [("revision", ctypes.c_ubyte), ("sbz1", ctypes.c_ubyte),
                    ("size", wintypes.WORD), ("count", wintypes.WORD), ("sbz2", wintypes.WORD)]

    class Ace(ctypes.Structure):
        _fields_ = [("type", ctypes.c_ubyte), ("flags", ctypes.c_ubyte),
                    ("size", wintypes.WORD), ("mask", wintypes.DWORD), ("sid", wintypes.DWORD)]

    dacl, descriptor = ctypes.POINTER(Acl)(), ctypes.c_void_p()
    # SE_FILE_OBJECT, DACL_SECURITY_INFORMATION
    if advapi32.GetNamedSecurityInfoW(str(path), 1, 4, None, None, ctypes.byref(dacl), None,
                                      ctypes.byref(descriptor)):
        raise OSError("GetNamedSecurityInfoW failed")
    try:
        control, revision = wintypes.WORD(), wintypes.DWORD()
        if not advapi32.GetSecurityDescriptorControl(descriptor, ctypes.byref(control),
                                                     ctypes.byref(revision)):
            raise ctypes.WinError(ctypes.get_last_error())
        aces = []
        for index in range(dacl.contents.count):
            ace = ctypes.POINTER(Ace)()
            if not advapi32.GetAce(dacl, index, ctypes.byref(ace)):
                raise ctypes.WinError(ctypes.get_last_error())
            text = wintypes.LPWSTR()
            sid = ctypes.addressof(ace.contents) + Ace.sid.offset
            if not advapi32.ConvertSidToStringSidW(ctypes.c_void_p(sid), ctypes.byref(text)):
                raise ctypes.WinError(ctypes.get_last_error())
            aces.append((ace.contents.type, ace.contents.mask, text.value))
            kernel32.LocalFree(text)
        return bool(control.value & 0x1000), aces  # SE_DACL_PROTECTED
    finally:
        kernel32.LocalFree(descriptor)


class ImageOutputTest(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name)
        self.work = self.root / "work"
        self.work.mkdir()

    def test_new_file_is_owner_only_and_repeatable(self):
        if os.name == "nt":
            self.assertEqual(image(self.work, "descendant.bin"), 0)
        else:
            previous = os.umask(0)
            try:
                self.assertEqual(image(self.work, "descendant.bin"), 0)
            finally:
                os.umask(previous)
        self.assertEqual(image(self.work, "Descendant_v7-copy.bin"), 0)
        first = (self.work / "descendant.bin").read_bytes()
        self.assertTrue(first)
        self.assertEqual((self.work / "Descendant_v7-copy.bin").read_bytes(), first)
        if os.name == "nt":
            protected, aces = windows_dacl(self.work / "descendant.bin")
            self.assertTrue(protected)
            # One access-allowed FILE_ALL_ACCESS entry for OWNER RIGHTS, nothing inherited.
            self.assertEqual(aces, [(0, 0x1F01FF, "S-1-3-4")])
        else:
            self.assertEqual(stat.S_IMODE((self.work / "descendant.bin").stat().st_mode), 0o600)

    def test_existing_file_is_never_truncated(self):
        existing = self.work / "existing.bin"
        existing.write_bytes(b"keep")
        self.assertEqual(image(self.work, "existing.bin"), 1)
        self.assertEqual(existing.read_bytes(), b"keep")

    def test_paths_and_invalid_names_are_rejected(self):
        (self.work / "sub").mkdir()
        names = ["", "x", "x.txt", ".bin", ".hidden.bin", "x.bin.tmp", "x.BIN", "x .bin",
                 "../escape.bin", "sub/x.bin", "sub\\x.bin", "./x.bin", "C:x.bin",
                 "x.bin:stream", "x" + chr(0xE9) + ".bin","a" * 60 + ".bin",
                 str(self.root / "absolute.bin")]
        before = tree(self.root)
        for name in names:
            with self.subTest(name=name):
                self.assertEqual(image(self.work, name), 1)
                self.assertEqual(tree(self.root), before)
        self.assertEqual(image(self.work, "a" * 59 + ".bin"), 0)

    @unittest.skipUnless(os.name == "nt", "Windows reserved device names")
    def test_device_names_never_open_devices(self):
        self.assertEqual(image(self.work, "expected.bin"), 0)
        expected = (self.work / "expected.bin").read_bytes()
        # Windows 11 treats these as ordinary names; earlier versions resolve devices.
        for name in ["CON.bin", "nul.bin", "COM1.bin"]:
            with self.subTest(name=name):
                created = image(self.work, name) == 0
                self.assertEqual((self.work / name).is_file(), created)
                if created:
                    self.assertEqual((self.work / name).read_bytes(), expected)
                    (self.work / name).unlink()

    def test_links_are_not_followed(self):
        outside, dangling = self.root / "outside.bin", self.root / "missing.bin"
        outside.write_bytes(b"keep")
        try:
            (self.work / "link.bin").symlink_to(outside)
            (self.work / "dangling.bin").symlink_to(dangling)
        except OSError as error:
            self.skipTest(f"symbolic links unavailable: {error}")
        self.assertEqual(image(self.work, "link.bin"), 1)
        self.assertEqual(image(self.work, "dangling.bin"), 1)
        self.assertEqual(outside.read_bytes(), b"keep")
        self.assertFalse(dangling.exists())
        self.assertTrue((self.work / "dangling.bin").is_symlink())

    @unittest.skipIf(os.name == "nt", "POSIX directory permissions")
    def test_creation_failure_reports_error(self):
        self.work.chmod(stat.S_IRUSR | stat.S_IXUSR)
        self.addCleanup(self.work.chmod, stat.S_IRWXU)
        if os.access(self.work, os.W_OK):
            self.skipTest("directory permissions are not enforced for this user")
        self.assertEqual(image(self.work, "denied.bin"), 1)
        self.assertEqual(list(self.work.iterdir()), [])

    @unittest.skipIf(os.name == "nt", "POSIX file size limit")
    def test_write_failure_reports_error_and_keeps_partial_file(self):
        import resource
        import signal

        def limit():
            signal.signal(signal.SIGXFSZ, signal.SIG_IGN)
            resource.setrlimit(resource.RLIMIT_FSIZE, (16, 16))
        # POSIX cannot remove a file through its descriptor, so nothing is removed by name.
        partial = self.work / "partial.bin"
        self.assertEqual(image(self.work, "partial.bin", preexec_fn=limit), 1)
        self.assertEqual(list(self.work.iterdir()), [partial])
        self.assertEqual(partial.stat().st_size, 16)
        self.assertEqual(stat.S_IMODE(partial.stat().st_mode), 0o600)
        self.assertEqual(image(self.work, "partial.bin"), 1)
        self.assertEqual(partial.stat().st_size, 16)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--native", required=True)
    args, rest = parser.parse_known_args()
    NATIVE = str(Path(args.native).resolve())
    unittest.main(argv=[sys.argv[0]] + rest)
