"""The Linux repo configures one archive, and one changed byte moves the revision."""

from __future__ import annotations

from pathlib import Path
import os
import shutil
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(ROOT / "OrlixKernel/Sources/ports/orlix/kbuild"))

from bazel.feasibility.kernel.linux_source_archive import (  # noqa: E402
    configured_source_files,
    extract,
    pack,
    package_build,
)
from source_state import prepare  # noqa: E402


def _populate(root: Path, fork: bytes) -> None:
    root.mkdir(parents=True, exist_ok=True)
    (root / "Makefile").write_text("VERSION = 6\n", encoding="utf-8")
    (root / "Kbuild").write_text("obj-y := kernel/\n", encoding="utf-8")
    (root / ".clang-format").write_text("BasedOnStyle: LLVM\n", encoding="utf-8")
    kernel = root / "kernel"
    kernel.mkdir()
    (kernel / "fork.c").write_bytes(fork)
    (kernel / "empty").mkdir()
    generic = root / "include/uapi/asm-generic"
    generic.mkdir(parents=True)
    (generic / "types.h").write_text("typedef int __kernel_long_t;\n", encoding="utf-8")
    arm = root / "arch/arm64/include/uapi/asm"
    arm.mkdir(parents=True)
    (arm / "unistd.h").write_text("#define __NR_read 63\n", encoding="utf-8")
    link = root / "include/asm"
    link.parent.mkdir(parents=True, exist_ok=True)
    link.symlink_to("uapi/asm-generic")
    script = root / "scripts/headers_install.sh"
    script.parent.mkdir(parents=True)
    script.write_text("#!/bin/sh\n", encoding="utf-8")
    script.chmod(0o755)


def _revision(linux: Path, work: Path) -> str:
    work.mkdir(parents=True)
    config = work / "release_defconfig"
    config.write_text("CONFIG_PAGE_SIZE_4KB=y\n", encoding="utf-8")
    isa = work / "isa"
    isa.mkdir()
    (isa / "marker.txt").write_text("isa\n", encoding="utf-8")
    return prepare(linux, work / "port", [], [], config, isa, "release", work)["sha256"]


class LinuxSourceArchiveTests(unittest.TestCase):
    def test_analysis_reads_the_archive_without_indexing_an_empty_list(self) -> None:
        text = (ROOT / "bazel/feasibility/analysis/providers.bzl").read_text(encoding="utf-8")
        self.assertIn('item.basename != "linux-source.tar"', text)
        self.assertIn("item.short_path", text)
        self.assertNotIn('linux_package[0].endswith("/linux-source.tar")', text)
        self.assertEqual(text.count("if len(linux_package) == 1:"), 2)
        self.assertEqual(text.count("linux_package[0]"), 3)

    def test_source_package_configures_one_archive(self) -> None:
        build = package_build(["Makefile", "usr/gen_init_cpio.c"])
        self.assertNotIn("glob(", build)
        files = configured_source_files(build)
        self.assertEqual(files, ["linux-source.tar"])

    def test_extract_into_existing_directory_keeps_the_archive_bytes(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "linux"
            _populate(source, b"int fork_state;\n")
            archive = root / "linux-source.tar"
            pack(source, archive)
            destination = root / "orlix-linux-uapi" / "linux"
            destination.mkdir(parents=True)
            extract(archive, destination)
            self.assertEqual((destination / "Makefile").read_text(encoding="utf-8"), "VERSION = 6\n")
            self.assertEqual((destination / "kernel/fork.c").read_bytes(), b"int fork_state;\n")

    def test_one_byte_source_change_changes_revision_and_archive(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            original = root / "linux"
            _populate(original, b"int fork_state;\n")
            changed = root / "changed"
            shutil.copytree(original, changed, symlinks=True)
            fork = changed / "kernel/fork.c"
            fork.write_bytes(b"int fork_state;\nX")
            original_revision = _revision(original, root / "work-original")
            changed_revision = _revision(changed, root / "work-changed")
            self.assertNotEqual(original_revision, changed_revision)
            original_archive = root / "original.tar"
            changed_archive = root / "changed.tar"
            pack(original, original_archive)
            pack(changed, changed_archive)
            self.assertNotEqual(original_archive.read_bytes(), changed_archive.read_bytes())
            again = root / "again.tar"
            pack(original, again)
            self.assertEqual(original_archive.read_bytes(), again.read_bytes())
            unpacked = root / "unpacked"
            extract(original_archive, unpacked)
            self.assertEqual(original_revision, _revision(unpacked, root / "work-unpacked"))
            self.assertEqual((unpacked / "kernel/fork.c").read_bytes(), b"int fork_state;\n")
            self.assertEqual(os.readlink(unpacked / "include/asm"), "uapi/asm-generic")
            self.assertTrue((unpacked / "scripts/headers_install.sh").stat().st_mode & 0o111)
            self.assertTrue((unpacked / "kernel/empty").is_dir())


if __name__ == "__main__":
    unittest.main()
