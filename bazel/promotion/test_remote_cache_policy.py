from __future__ import annotations

import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

# Expensive foreign producers that fail Gate A (undeclared host tools / PATH /
# default shell env). 0.4 keeps no-remote-cache on these mnemonics.
BLOCKED = (
    (ROOT / "bazel/feasibility/kernel/kernel_macho.bzl", "OrlixKernelMachOArchive"),
    (ROOT / "bazel/feasibility/kernel/kernel_macho.bzl", "OrlixTctiIsaRestore"),
    (ROOT / "bazel/feasibility/mlibc/mlibc_sysroot.bzl", "OrlixMLibCSysroot"),
    (ROOT / "bazel/feasibility/mlibc/mlibc_sysroot.bzl", "OrlixCompilerRuntime"),
    (ROOT / "bazel/feasibility/packages/coreutils.bzl", "OrlixGuestPackage"),
    (ROOT / "bazel/feasibility/packages/autotools.bzl", "OrlixGuestPackage"),
    (ROOT / "bazel/feasibility/packages/bash.bzl", "OrlixGuestPackage"),
    (ROOT / "bazel/feasibility/rootfs/rootfs.bzl", "OrlixRootfs"),
)


class RemoteCachePolicyTests(unittest.TestCase):
    def test_expensive_foreign_producers_keep_no_remote_cache(self) -> None:
        for path, mnemonic in BLOCKED:
            text = path.read_text(encoding="utf-8")
            self.assertIn(f'mnemonic = "{mnemonic}"', text, path.name)
            self.assertIn('"no-remote-cache": "1"', text, f"{path.name} {mnemonic}")

    def test_uapi_headers_install_does_not_gain_a_blanket_block(self) -> None:
        text = (ROOT / "bazel/feasibility/kernel/kernel_uapi.bzl").read_text(encoding="utf-8")
        self.assertIn('mnemonic = "OrlixLinuxHeadersInstall"', text)
        self.assertNotIn('"no-remote-cache": "1"', text)


if __name__ == "__main__":
    unittest.main()
