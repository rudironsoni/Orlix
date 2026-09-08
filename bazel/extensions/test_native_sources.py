from __future__ import annotations

import json
import re
import unittest
from pathlib import Path

SOURCE = Path(__file__).with_name("native_sources.bzl")
PACKAGES = Path(__file__).with_name("ghostty_zig_packages.json")


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

    def test_bash_archive_has_sha256(self) -> None:
        text = SOURCE.read_text(encoding="utf-8")
        bash = re.search(
            r'name = "orlix_bash_source",.*?sha256 = "([0-9a-f]{64})"',
            text,
            re.S,
        )
        self.assertIsNotNone(bash)
        self.assertEqual(bash.group(1), "0d5cd86965f869a26cf64f4b71be7b96f90a3ba8b3d74e27e8e9d9d5550f31ba")
        self.assertIn("bash-5.3", text)
        self.assertIn("https://ftp.gnu.org/gnu/bash/bash-5.3.tar.gz", text)

    def test_rootfs_package_archives_have_sha256(self) -> None:
        text = SOURCE.read_text(encoding="utf-8")
        pins = (
            (
                "orlix_grep_source",
                "https://ftp.gnu.org/gnu/grep/grep-3.12.tar.xz",
                "2649b27c0e90e632eadcd757be06c6e9a4f48d941de51e7c0f83ff76408a07b9",
                "grep-3.12",
            ),
            (
                "orlix_findutils_source",
                "https://ftp.gnu.org/gnu/findutils/findutils-4.10.0.tar.xz",
                "1387e0b67ff247d2abde998f90dfbf70c1491391a59ddfecb8ae698789f0a4f5",
                "findutils-4.10.0",
            ),
            (
                "orlix_e2fsprogs_source",
                "https://www.kernel.org/pub/linux/kernel/people/tytso/e2fsprogs/v1.47.1/e2fsprogs-1.47.1.tar.xz",
                "5a33dc047fd47284bca4bb10c13cfe7896377ae3d01cb81a05d406025d99e0d1",
                "e2fsprogs-1.47.1",
            ),
            (
                "orlix_jq_source",
                "https://github.com/jqlang/jq/releases/download/jq-1.7.1/jq-1.7.1.tar.gz",
                "478c9ca129fd2e3443fe27314b455e211e0d8c60bc8ff7df703873deeee580c2",
                "jq-1.7.1",
            ),
            (
                "orlix_curl_source",
                "https://curl.se/download/curl-8.20.0.tar.xz",
                "63fe2dc148ba0ceae89922ef838f7e5c946272c2e78b7c59fab4b79d3ce2b896",
                "curl-8.20.0",
            ),
            (
                "orlix_ncurses_source",
                "https://ftp.gnu.org/gnu/ncurses/ncurses-6.6.tar.gz",
                "355b4cbbed880b0381a04c46617b7656e362585d52e9cf84a67e2009b749ff11",
                "ncurses-6.6",
            ),
            (
                "orlix_zsh_source",
                "https://www.zsh.org/pub/old/zsh-5.9.tar.xz",
                "9b8d1ecedd5b5e81fbf1918e876752a7dd948e05c1a0dba10ab863842d45acd5",
                "zsh-5.9",
            ),
        )
        for name, url, sha256, prefix in pins:
            match = re.search(
                r'name = "%s",.*?sha256 = "([0-9a-f]{64})"' % name,
                text,
                re.S,
            )
            self.assertIsNotNone(match, name)
            self.assertEqual(match.group(1), sha256, name)
            self.assertIn(url, text)
            self.assertIn(prefix, text)

    def test_ghostty_packages_are_digest_pinned_without_zig_fetch(self) -> None:
        text = SOURCE.read_text(encoding="utf-8")
        self.assertNotIn("--fetch=all", text)
        self.assertNotIn("fetch-source", text)
        self.assertNotIn("xcodebuild", text)
        self.assertIn('packages = "//bazel/extensions:ghostty_zig_packages.json"', text)
        self.assertIn("ctx.download", text)
        self.assertIn("/usr/bin/tar", text)
        self.assertIn("zig-pkg/p/", text)
        payload = json.loads(PACKAGES.read_text(encoding="utf-8"))
        packages = payload["packages"]
        self.assertGreaterEqual(len(packages), 30)
        names = set()
        for pkg in packages:
            self.assertEqual(len(pkg["sha256"]), 64)
            self.assertTrue(pkg["url"].startswith("https://"))
            self.assertNotIn("latest", pkg["url"])
            self.assertNotIn(pkg["name"], names)
            names.add(pkg["name"])
        zlib = next(pkg for pkg in packages if "AAB0eQwD" in pkg["name"])
        self.assertIn("zlib-1220fed0", zlib["url"])
        self.assertNotEqual(zlib["url"].rsplit("/", 1)[-1], zlib["name"])
