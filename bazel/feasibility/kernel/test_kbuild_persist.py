from __future__ import annotations

import os
import stat
import subprocess
import tempfile
import unittest
from pathlib import Path

import kbuild_persist as persist
from bazel.content_digest import tree_digest


class KbuildArchiveTests(unittest.TestCase):
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


class ContentDigestTests(unittest.TestCase):
    def test_digest_changes_for_paths_modes_contents_and_symlink_targets(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "tree"
            root.mkdir()
            source = root / "header.h"
            source.write_text("#define VALUE 1\n", encoding="utf-8")
            link = root / "current"
            link.symlink_to("header.h")

            original = tree_digest(root)
            source.write_text("#define VALUE 2\n", encoding="utf-8")
            self.assertNotEqual(original, tree_digest(root))

            source.chmod(stat.S_IRUSR | stat.S_IWUSR)
            mode_changed = tree_digest(root)
            source.chmod(stat.S_IRUSR | stat.S_IWUSR | stat.S_IRGRP)
            mode_updated = tree_digest(root)
            self.assertNotEqual(mode_changed, mode_updated)

            link.unlink()
            link.symlink_to("missing.h")
            symlink_changed = tree_digest(root)
            self.assertNotEqual(mode_updated, symlink_changed)

            source.rename(root / "renamed.h")
            self.assertNotEqual(symlink_changed, tree_digest(root))


class HeadersInstallTests(unittest.TestCase):
    def test_removed_headers_cannot_survive_installed_or_staging_state(self) -> None:
        repository = Path(__file__).resolve().parents[3]
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / "linux"
            build = root / "build"
            output = build / "OrlixMLibC/kernel-headers/release"
            kbuild = build / "OrlixKernel/build/release-uapi-arm64"
            staging = kbuild / "usr/include"
            for headers in (source / "include/uapi", output / "include", staging):
                (headers / "linux").mkdir(parents=True)
                (headers / "linux/removed.h").write_text("obsolete\n")
            (source / "include/uapi/linux/removed.h").unlink()
            (source / "include/uapi/linux/current.h").write_text("#define CURRENT 1\n")
            (source / ".orlix-port-profile").write_text("old profile\n")
            (output / ".orlix-headers-ready").write_text("old output\n")
            os.utime(source / ".orlix-port-profile", (1000, 1000))
            os.utime(output / ".orlix-headers-ready", (2000, 2000))
            state = kbuild / "upstream-state"
            state.write_bytes(b"retain upstream incremental state\n")
            (source / "Makefile").write_text(
                ".PHONY: headers_install\nheaders_install:\n"
                '\t@test ! -e "$(INSTALL_HDR_PATH)/include/linux/removed.h"\n'
                '\t@test ! -e "$(O)/usr/include/linux/removed.h"\n'
                '\t@mkdir -p "$(O)/usr/include/linux" "$(INSTALL_HDR_PATH)/include"\n'
                '\t@cp include/uapi/linux/current.h "$(O)/usr/include/linux/current.h"\n'
                '\t@cp -R "$(O)/usr/include/." "$(INSTALL_HDR_PATH)/include/"\n'
            )
            result = subprocess.run(
                ["gmake", "-f", "OrlixKernel/Sources/ports/orlix/kbuild/kernel-rules.mk",
                 "-o", "__prepare-port", "__headers-install", "PROFILE=release",
                 f"ORLIX_BUILD_ROOT={build}", f"ORLIX_KERNEL_PORT_DIR={source}"],
                cwd=repository, capture_output=True, text=True, timeout=30,
            )
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            for headers in (output / "include", staging):
                self.assertFalse((headers / "linux/removed.h").exists())
                self.assertEqual(tree_digest(headers), tree_digest(source / "include/uapi"))
            self.assertEqual(state.read_bytes(), b"retain upstream incremental state\n")


if __name__ == "__main__":
    unittest.main()
