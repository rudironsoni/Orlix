"""Guest configure inputs stay on one package. Rootfs policy stays on assembly."""

from __future__ import annotations

import re
import subprocess
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
PACKAGES = ROOT / "bazel/feasibility/packages"
ROOTFS = ROOT / "bazel/feasibility/rootfs"
MARKERS = {
    "acl": "orlix_acl_source/",
    "attr": "orlix_attr_source/",
    "bash": "orlix_bash_source/",
    "coreutils": "orlix_coreutils_source/",
    "curl": "orlix_curl_source/",
    "e2fsprogs": "orlix_e2fsprogs_source/",
    "findutils": "orlix_findutils_source/",
    "grep": "orlix_grep_source/",
    "jq": "orlix_jq_source/",
    "libcap": "orlix_libcap_source/",
    "libselinux": "orlix_libselinux_source/",
    "libsepol": "orlix_libsepol_source/",
    "musl-fts": "orlix_musl_fts_source/",
    "ncurses": "orlix_ncurses_source/",
    "pcre2": "orlix_pcre2_source/",
    "zsh": "orlix_zsh_source/",
}


def configure_accepts(package: str, path: str) -> bool:
    if path.endswith("assembly_policy.txt"):
        return False
    for other, marker in MARKERS.items():
        if other != package and marker in path:
            return False
    return True


def _package_blocks(text: str) -> list[str]:
    parts = re.split(r"\n(?=orlix_(?:autotools|bash|coreutils)_package\()", text)
    return [part for part in parts if part.startswith("orlix_")]


class PackageRootfsInputTests(unittest.TestCase):
    def test_foreign_source_and_rootfs_policy_are_not_configure_inputs(self) -> None:
        grep_source = "external/+native_sources+orlix_grep_source/src/grep.c"
        bash_source = "external/+native_sources+orlix_bash_source/shell.c"
        policy = "bazel/feasibility/rootfs/assembly_policy.txt"
        self.assertTrue(configure_accepts("bash", bash_source))
        self.assertFalse(configure_accepts("bash", grep_source))
        self.assertFalse(configure_accepts("grep", bash_source))
        self.assertFalse(configure_accepts("coreutils", "external/+native_sources+orlix_attr_source/libattr/attr.c"))
        self.assertTrue(configure_accepts("coreutils", "external/+native_sources+orlix_coreutils_source/src/ls.c"))
        for package in MARKERS:
            self.assertFalse(configure_accepts(package, policy), package)
        rule = (PACKAGES / "action_inputs.bzl").read_text(encoding="utf-8")
        for marker in MARKERS.values():
            self.assertIn(f'"{marker}"', rule)
        self.assertIn('path.endswith("assembly_policy.txt")', rule)
        self.assertIn("foreign package source is not", rule)
        self.assertIn("rootfs policy is not", rule)
        for name in ("autotools.bzl", "bash.bzl", "coreutils.bzl"):
            text = (PACKAGES / name).read_text(encoding="utf-8")
            self.assertIn("guest_configure_inputs(ctx.attr.package_name,", text, name)
            self.assertNotIn("assembly_policy", text, name)

    def test_each_guest_package_has_its_own_source_repo(self) -> None:
        blocks = _package_blocks((PACKAGES / "BUILD.bazel").read_text(encoding="utf-8"))
        sources: dict[str, str] = {}
        for block in blocks:
            name = re.search(r'\n    name = "([^"]+)"', block)
            label = re.search(r'\n    sources = "([^"]+)"', block)
            self.assertIsNotNone(name, block[:80])
            self.assertIsNotNone(label, name.group(1) if name else block[:80])
            assert name is not None and label is not None
            sources[name.group(1)] = label.group(1)
        self.assertEqual(set(sources), set(MARKERS))
        self.assertEqual(len(set(sources.values())), len(sources))
        for package, label in sources.items():
            self.assertEqual(label, f"@orlix_{package.replace('-', '_')}_source//:source")

    def test_rootfs_assembly_reads_policy_and_package_outputs(self) -> None:
        rule = (ROOTFS / "rootfs.bzl").read_text(encoding="utf-8")
        assembly = rule.split("def _rootfs_impl", 1)[1].split("def _rootfs_payload_impl", 1)[0]
        self.assertIn("pkg_inputs = [pkg.install_tree for pkg in pkgs]", assembly)
        self.assertIn("direct = pkg_inputs +", assembly)
        self.assertIn("ctx.file.assembly_policy", assembly)
        self.assertNotIn("ctx.files.sources", assembly)
        self.assertNotIn("orlix_grep_source", assembly)
        self.assertNotIn("orlix_bash_source", assembly)
        self.assertNotIn("orlix_coreutils_source", assembly)
        self.assertIn('"no-sandbox": "1"', assembly)
        self.assertNotIn("/opt/homebrew/bin", rule)
        build = (ROOTFS / "BUILD.bazel").read_text(encoding="utf-8")
        packages = build.split("packages = [", 1)[1].split("]", 1)[0]
        self.assertNotIn("_source", packages)
        self.assertIn("exports_files([\"assembly_policy.txt\"])", build)
        policy = (ROOTFS / "assembly_policy.txt").read_text(encoding="utf-8").splitlines()
        for line in (
            "copy_bin usr/bin/grep bin/grep",
            "copy_bin usr/bin/bash bin/bash",
            "require usr/bin/getpolicyload",
            "require sbin/init",
            "symlink bin/sh bash",
            "metadata base_packages=bash coreutils grep findutils e2fsprogs jq curl zsh attr acl libcap libselinux",
            "image_link base /bin/sh bash",
            "image_file base /usr/bin/jq",
        ):
            self.assertIn(line, policy)
        for name in ("autotools.bzl", "bash.bzl", "coreutils.bzl", "local.bzl", "package.bzl", "build_state.py"):
            self.assertNotIn("assembly_policy", (PACKAGES / name).read_text(encoding="utf-8"), name)

    def test_cache_tags_and_tool_path_stay(self) -> None:
        autotools = (PACKAGES / "autotools.bzl").read_text(encoding="utf-8")
        self.assertNotIn('"no-remote-cache": "1"', autotools)
        self.assertNotIn("no-remote-cache", autotools)
        self.assertIn('"no-sandbox": "1"', autotools)
        self.assertIn('"no-remote-exec": "1"', autotools)
        self.assertIn('export PATH="$tool_bin:$work/toolchain:/usr/bin:/bin"', autotools)
        self.assertIn('clang="$tool_bin/clang"', autotools)
        self.assertIn('autom4te_perllibdir="$share/autoconf"', autotools)
        self.assertNotIn("xcrun", autotools)
        kernel = (ROOT / "bazel/feasibility/kernel/kernel_macho.bzl").read_text(encoding="utf-8")
        for mnemonic in ("OrlixTctiIsaRestore", "OrlixKernelMachOArchive"):
            start = kernel.index(f'mnemonic = "{mnemonic}"')
            block = kernel[start:kernel.index("\n", kernel.index("execution_requirements = ", start))]
            self.assertNotIn("no-remote-cache", block, mnemonic)
            self.assertIn('"no-remote-exec": "1"', block, mnemonic)
        self.assertIn('"no-sandbox": "1"', kernel)
        for name in ("coreutils.bzl", "bash.bzl"):
            text = (PACKAGES / name).read_text(encoding="utf-8")
            self.assertIn('"no-sandbox": "1"', text, name)
            self.assertNotIn("no-remote-cache", text, name)
            self.assertIn('export PATH="$tool_bin:/usr/bin:/bin"', text, name)
            self.assertIn('clang="$tool_bin/clang"', text, name)
            self.assertNotIn("/opt/homebrew/bin", text, name)
            self.assertNotIn("xcrun", text, name)
        rootfs = (ROOTFS / "rootfs.bzl").read_text(encoding="utf-8")
        self.assertIn('"PATH": "/usr/bin:/bin"', rootfs)

    def test_rootfs_command_is_valid_bash(self) -> None:
        rule = (ROOTFS / "rootfs.bzl").read_text(encoding="utf-8")
        start = rule.index('command = r"""') + len('command = r"""\n')
        end = rule.index('\n""",', start)
        script = rule[start:end]
        self.assertIn('policy="$exec_root/${14}"', script)
        self.assertNotIn('copy_bin "$base_tree/usr/bin/grep"', script)
        completed = subprocess.run(["/bin/bash", "-n"], input=script, text=True, capture_output=True)
        self.assertEqual(completed.returncode, 0, completed.stderr)


if __name__ == "__main__":
    unittest.main()
