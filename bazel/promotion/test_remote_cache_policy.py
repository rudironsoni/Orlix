"""Contract spec for foreign-action remote cache tags.

no-remote-cache blocks the disk cache and BuildBuddy. It does not block the
output-base action cache. no-remote-exec stays on these foreign actions.
This reads current rule text. It is not a timed build and not a product build.
"""

from __future__ import annotations

import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
POLICY = {
    "no_remote_cache_blocks": ("buildbuddy", "disk_cache"),
    "no_remote_cache_does_not_block": ("output_base_action_cache",),
    "no_remote_exec": True,
}
REQUIRED_NO_REMOTE_CACHE = (
    ("bazel/feasibility/kernel/kernel_macho.bzl", "OrlixKernelMachOArchive"),
    ("bazel/feasibility/mlibc/mlibc_sysroot.bzl", "OrlixMLibCSysroot"),
    ("bazel/feasibility/mlibc/mlibc_sysroot.bzl", "OrlixCompilerRuntime"),
    ("bazel/feasibility/packages/coreutils.bzl", "OrlixGuestPackage"),
    ("bazel/feasibility/packages/bash.bzl", "OrlixGuestPackage"),
    ("bazel/feasibility/packages/autotools.bzl", "OrlixGuestPackage"),
    ("bazel/feasibility/rootfs/rootfs.bzl", "OrlixRootfs"),
    ("bazel/feasibility/kernel/kernel_macho.bzl", "OrlixTctiIsaRestore"),
)
FORBIDDEN_NO_REMOTE_CACHE = (
    ("bazel/feasibility/kernel/kernel_uapi.bzl", "OrlixLinuxHeadersInstall"),
    ("bazel/feasibility/rootfs/rootfs.bzl", "OrlixRootfsPayload"),
    ("bazel/feasibility/packages/package.bzl", "OrlixGuestPackage"),
    ("bazel/feasibility/packages/local.bzl", "OrlixGuestPackage"),
)
MNEMONIC = re.compile(r'mnemonic\s*=\s*"([^"]+)"')
REQUIREMENTS = re.compile(r"execution_requirements\s*=\s*\{([^}]*)\}", re.DOTALL)
FLAG = re.compile(r'"([^"]+)"\s*:\s*"([^"]+)"')


def _requirements(path: str) -> dict[str, list[dict[str, str]]]:
    text = (ROOT / path).read_text(encoding="utf-8")
    found: dict[str, list[dict[str, str]]] = {}
    matches = list(MNEMONIC.finditer(text))
    if not matches:
        raise AssertionError(f"no mnemonics in {path}")
    for index, match in enumerate(matches):
        end = matches[index + 1].start() if index + 1 < len(matches) else len(text)
        window = text[match.end() : end]
        body = REQUIREMENTS.search(window)
        if body is None:
            raise AssertionError(f"{path} has no execution_requirements for {match.group(1)}")
        found.setdefault(match.group(1), []).append(dict(FLAG.findall(body.group(1))))
    return found


class RemoteCachePolicyTests(unittest.TestCase):
    def test_policy_blocks_shared_caches_and_keeps_the_output_base(self) -> None:
        self.assertEqual(POLICY["no_remote_cache_blocks"], ("buildbuddy", "disk_cache"))
        self.assertEqual(POLICY["no_remote_cache_does_not_block"], ("output_base_action_cache",))
        self.assertIs(POLICY["no_remote_exec"], True)

    def test_required_producers_keep_no_remote_cache(self) -> None:
        seen = {mnemonic for _path, mnemonic in REQUIRED_NO_REMOTE_CACHE}
        self.assertEqual(
            seen,
            {
                "OrlixCompilerRuntime",
                "OrlixGuestPackage",
                "OrlixKernelMachOArchive",
                "OrlixMLibCSysroot",
                "OrlixRootfs",
                "OrlixTctiIsaRestore",
            },
        )
        for path, mnemonic in REQUIRED_NO_REMOTE_CACHE:
            matches = _requirements(path)[mnemonic]
            self.assertTrue(matches, mnemonic)
            for requirements in matches:
                self.assertEqual(requirements.get("no-remote-cache"), "1", f"{path} {mnemonic}")
                self.assertEqual(requirements.get("no-remote-exec"), "1", f"{path} {mnemonic}")

    def test_kernel_archive_keeps_no_sandbox(self) -> None:
        requirements = _requirements("bazel/feasibility/kernel/kernel_macho.bzl")["OrlixKernelMachOArchive"]
        self.assertEqual(len(requirements), 1)
        self.assertEqual(requirements[0].get("no-sandbox"), "1")
        self.assertEqual(requirements[0].get("no-remote-cache"), "1")

    def test_headers_payload_and_local_c_do_not_use_the_tag_as_cover(self) -> None:
        for path, mnemonic in FORBIDDEN_NO_REMOTE_CACHE:
            matches = _requirements(path)[mnemonic]
            self.assertTrue(matches, mnemonic)
            for requirements in matches:
                self.assertNotIn("no-remote-cache", requirements, f"{path} {mnemonic}")
                self.assertEqual(requirements.get("no-remote-exec"), "1", f"{path} {mnemonic}")


if __name__ == "__main__":
    unittest.main()
