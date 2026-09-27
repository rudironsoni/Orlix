"""Declared-byte keys change when a C file changes."""

from __future__ import annotations

import tempfile
import tarfile
import unittest
from pathlib import Path

from bazel.feasibility.packages.source_key import write_file_key, write_tree_tar


class SourceKeyTests(unittest.TestCase):
    def test_c_edit_changes_tar_and_stamp(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "getconf.c"
            source.write_bytes(b"int main(void) { return 0; }\n")
            source.chmod(0o755)
            archive = root / "sources.tar"
            stamp = root / "sources.stamp"
            write_file_key(archive, stamp, [("pkg/getconf.c", source)])
            first_archive = archive.read_bytes()
            first_stamp = stamp.read_text(encoding="utf-8")
            with tarfile.open(archive) as tar:
                member = tar.getmember("pkg/getconf.c")
                self.assertEqual(member.mtime, 0)
                self.assertEqual(member.uid, 0)
                self.assertEqual(member.gid, 0)
                self.assertEqual(member.mode & 0o777, 0o755)
            source.write_bytes(b"int main(void) { return 1; }\n")
            write_file_key(archive, stamp, [("pkg/getconf.c", source)])
            self.assertNotEqual(archive.read_bytes(), first_archive)
            self.assertNotEqual(stamp.read_text(encoding="utf-8"), first_stamp)
            self.assertIn("pkg/getconf.c", stamp.read_text(encoding="utf-8"))

    def test_tree_tar_follows_file_bytes(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            tree = root / "headers"
            nested = tree / "include"
            nested.mkdir(parents=True)
            header = nested / "limits.h"
            header.write_bytes(b"#define N 1\n")
            archive = root / "headers.tar"
            write_tree_tar(archive, tree)
            with tarfile.open(archive) as tar:
                member = tar.getmember("include/limits.h")
                self.assertEqual(member.mtime, 0)
                self.assertEqual(tar.extractfile(member).read(), b"#define N 1\n")
            header.write_bytes(b"#define N 2\n")
            write_tree_tar(archive, tree)
            with tarfile.open(archive) as tar:
                self.assertEqual(tar.extractfile("include/limits.h").read(), b"#define N 2\n")


if __name__ == "__main__":
    unittest.main()
