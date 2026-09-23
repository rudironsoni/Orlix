from __future__ import annotations

import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

# Kernel and mlibc results are disk-cacheable. Their commands do not contain
# the worktree output path. Mutable Kbuild and Ninja state stays under that
# worktree's output base and is not an action input.
DISK_CACHEABLE = (
    (ROOT / "bazel/feasibility/kernel/kernel_macho.bzl", "OrlixKernelMachOArchive"),
    (ROOT / "bazel/feasibility/mlibc/mlibc_sysroot.bzl", "OrlixMLibCSysroot"),
)

BLOCKED = (
    (ROOT / "bazel/feasibility/kernel/kernel_macho.bzl", "OrlixTctiIsaRestore"),
    (ROOT / "bazel/feasibility/mlibc/mlibc_sysroot.bzl", "OrlixCompilerRuntime"),
    (ROOT / "bazel/feasibility/packages/coreutils.bzl", "OrlixGuestPackage"),
    (ROOT / "bazel/feasibility/packages/autotools.bzl", "OrlixGuestPackage"),
    (ROOT / "bazel/feasibility/packages/bash.bzl", "OrlixGuestPackage"),
    (ROOT / "bazel/feasibility/rootfs/rootfs.bzl", "OrlixRootfs"),
)


def _requirements(text: str, mnemonic: str) -> str:
    marker = 'mnemonic = "%s"' % mnemonic
    index = text.find(marker)
    if index < 0:
        raise AssertionError("missing mnemonic " + mnemonic)
    rest = text[index:]
    req = rest.find("execution_requirements")
    if req < 0:
        raise AssertionError("missing execution_requirements for " + mnemonic)
    return rest[req:req + 400]


class RemoteCachePolicyTests(unittest.TestCase):
    def test_kernel_and_mlibc_are_disk_cacheable(self) -> None:
        for path, mnemonic in DISK_CACHEABLE:
            requirements = _requirements(path.read_text(encoding="utf-8"), mnemonic)
            self.assertIn('"no-remote-exec": "1"', requirements, mnemonic)
            self.assertNotIn('"no-remote-cache": "1"', requirements, mnemonic)

    def test_other_foreign_producers_keep_no_remote_cache(self) -> None:
        for path, mnemonic in BLOCKED:
            requirements = _requirements(path.read_text(encoding="utf-8"), mnemonic)
            self.assertIn('"no-remote-cache": "1"', requirements, mnemonic)

    def test_uapi_headers_install_does_not_gain_a_blanket_block(self) -> None:
        text = (ROOT / "bazel/feasibility/kernel/kernel_uapi.bzl").read_text(encoding="utf-8")
        self.assertIn('mnemonic = "OrlixLinuxHeadersInstall"', text)
        self.assertNotIn('"no-remote-cache": "1"', text)


if __name__ == "__main__":
    unittest.main()
