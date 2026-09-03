from __future__ import annotations

import re
import unittest
from pathlib import Path

SOURCE = Path(__file__).with_name("native_sources.bzl")


class NativeSourceHashTests(unittest.TestCase):
    def test_linux_and_mlibc_archives_have_sha256(self) -> None:
        text = SOURCE.read_text(encoding="utf-8")
        linux = re.search(
            r'name = "orlix_linux_source",.*?sha256 = "([0-9a-f]{64})"',
            text,
            re.S,
        )
        mlibc = re.search(
            r'name = "orlix_mlibc_source",.*?sha256 = "([0-9a-f]{64})"',
            text,
            re.S,
        )
        self.assertIsNotNone(linux)
        self.assertIsNotNone(mlibc)
        self.assertEqual(len(linux.group(1)), 64)
        self.assertEqual(len(mlibc.group(1)), 64)
        self.assertIn("linux-6.12.105", text)
        self.assertIn('name = "orlix_linux_source"', text)
        self.assertIn('extra_exports = ["usr/gen_init_cpio.c"]', text)
        self.assertIn("headers_install", Path(__file__).resolve().parents[1].joinpath("feasibility/kernel/kernel_uapi.bzl").read_text(encoding="utf-8"))

    def test_coreutils_archive_has_sha256_and_gnulib_pin(self) -> None:
        text = SOURCE.read_text(encoding="utf-8")
        coreutils = re.search(
            r'name = "orlix_coreutils_source",.*?sha256 = "([0-9a-f]{64})"',
            text,
            re.S,
        )
        self.assertIsNotNone(coreutils)
        self.assertEqual(len(coreutils.group(1)), 64)
        self.assertIn("coreutils-9.11", text)
        self.assertIn("c01fd163a47468a8296fb369f5233853bb551bb6", text)
        self.assertIn("https://github.com/coreutils/gnulib.git", text)
        self.assertIn("https://ftp.gnu.org/gnu/coreutils/coreutils-9.11.tar.xz", text)
