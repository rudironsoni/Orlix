from __future__ import annotations

import tempfile
import os
import unittest
from pathlib import Path

import kbuild_persist as persist


class KbuildPersistTests(unittest.TestCase):
    def test_archive_bytes_do_not_depend_on_source_timestamps(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "source"
            root.mkdir()
            source = root / "header.h"
            source.write_bytes(b"#define VALUE 1\n")
            first, second = Path(tmp) / "first.tar", Path(tmp) / "second.tar"
            persist.write_archive(str(root), str(first))
            os.utime(source, (1000, 1000))
            persist.write_archive(str(root), str(second))
            self.assertEqual(first.read_bytes(), second.read_bytes())
            self.assertEqual(source.stat().st_mtime, 1000)

    def test_identity_includes_linux_revision_tag_and_xcode_build(self) -> None:
        ident = persist.identity("6.12.105", "14c37ff05f22da2fa7076d10f6a07c7ede330c83", "17F113")
        self.assertIn("6.12.105", ident)
        self.assertIn("14c37ff05f22da2fa7076d10f6a07c7ede330c83", ident)
        self.assertIn("17F113", ident)
        other = persist.identity("6.12.105", "14c37ff05f22da2fa7076d10f6a07c7ede330c83", "27A5252f")
        self.assertNotEqual(ident, other)

    def test_matching_stamp_copies_hdr_and_archive_without_rebuild(self) -> None:
        ident = persist.identity("6.12.105", "14c37ff05f22da2fa7076d10f6a07c7ede330c83", "17F113")
        with tempfile.TemporaryDirectory() as tmp:
            persist_dir = Path(tmp) / "persist"
            src_headers = Path(tmp) / "src-headers"
            src_archive = Path(tmp) / "src.tar"
            (src_headers / "include" / "linux").mkdir(parents=True)
            (src_headers / "include" / "linux" / "unistd.h").write_text("#define __NR_exit 93\n", encoding="utf-8")
            src_archive.write_bytes(b"kbuild-archive")
            persist.store_outputs(str(persist_dir), ident, str(src_headers), str(src_archive))
            out_headers = Path(tmp) / "out-headers"
            out_archive = Path(tmp) / "out.tar"
            self.assertTrue(
                persist.reuse_outputs(str(persist_dir), ident, str(out_headers), str(out_archive))
            )
            self.assertEqual(
                (out_headers / "include" / "linux" / "unistd.h").read_text(encoding="utf-8"),
                "#define __NR_exit 93\n",
            )
            self.assertEqual(out_archive.read_bytes(), b"kbuild-archive")

    def test_mismatched_stamp_does_not_copy(self) -> None:
        ident = persist.identity("6.12.105", "14c37ff05f22da2fa7076d10f6a07c7ede330c83", "17F113")
        with tempfile.TemporaryDirectory() as tmp:
            persist_dir = Path(tmp) / "persist"
            src_headers = Path(tmp) / "src-headers"
            src_archive = Path(tmp) / "src.tar"
            (src_headers / "include" / "linux").mkdir(parents=True)
            (src_headers / "include" / "linux" / "unistd.h").write_text("uapi\n", encoding="utf-8")
            src_archive.write_bytes(b"kbuild-archive")
            persist.store_outputs(str(persist_dir), ident, str(src_headers), str(src_archive))
            out_headers = Path(tmp) / "out-headers"
            out_archive = Path(tmp) / "out.tar"
            self.assertFalse(
                persist.reuse_outputs(str(persist_dir), ident + "-other", str(out_headers), str(out_archive))
            )
            self.assertFalse(out_archive.exists())
            self.assertFalse((out_headers / "include").exists())

    def test_empty_persist_path_is_noop(self) -> None:
        ident = persist.identity("6.12.105", "14c37ff05f22da2fa7076d10f6a07c7ede330c83", "17F113")
        self.assertFalse(persist.can_reuse("", ident))
        self.assertFalse(persist.reuse_outputs("", ident, "/unused", "/unused.tar"))
        persist.store_outputs("", ident, "/unused", "/unused.tar")


if __name__ == "__main__":
    unittest.main()
