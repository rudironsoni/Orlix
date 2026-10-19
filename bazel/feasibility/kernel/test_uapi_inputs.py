"""UAPI input selection. A kernel .c edit must not be a headers_install input."""

from __future__ import annotations

from pathlib import Path
import subprocess
import unittest

from bazel.feasibility.kernel.test_linux_source_archive import LinuxSourceArchiveTests
from bazel.feasibility.kernel.uapi_inputs import EXACT, TREE_PREFIXES, headers_install_source

# make/bazel-migration.mk already runs this module. The import collects
# LinuxSourceArchiveTests here without editing that file.
assert issubclass(LinuxSourceArchiveTests, unittest.TestCase)

ROOT = Path(__file__).resolve().parents[3]
UAPI = ROOT / "bazel/feasibility/kernel/kernel_uapi.bzl"
BUILD = ROOT / "bazel/feasibility/kernel/BUILD.bazel"


class UapiInputTests(unittest.TestCase):
    def test_unrelated_kernel_sources_are_not_headers_install_inputs(self) -> None:
        self.assertFalse(headers_install_source("kernel/fork.c"))
        self.assertFalse(headers_install_source("mm/page_alloc.c"))
        self.assertFalse(headers_install_source("drivers/char/mem.c"))
        self.assertFalse(headers_install_source("usr/gen_init_cpio.c"))
        self.assertFalse(headers_install_source("arch/arm64/kernel/head.S"))
        self.assertFalse(headers_install_source("Documentation/admin-guide/README.rst"))
        self.assertFalse(headers_install_source("external/+native_sources+orlix_linux_source/kernel/fork.c"))

    def test_headers_scripts_makefiles_tools_and_syscall_config_are_inputs(self) -> None:
        required = (
            "include/uapi/linux/types.h",
            "arch/arm64/include/uapi/asm/unistd.h",
            "arch/arm64/include/uapi/asm/Kbuild",
            "arch/arm64/tools/syscall_64.tbl",
            "arch/arm64/tools/gen-sysreg.awk",
            "scripts/headers_install.sh",
            "scripts/Makefile.headersinst",
            "scripts/unifdef.c",
            "scripts/basic/fixdep.c",
            "Makefile",
            "Kbuild",
            "arch/arm64/Makefile",
            "arch/arm64/kernel/Makefile.syscalls",
            "external/+native_sources+orlix_linux_source/include/uapi/linux/types.h",
            "external/+native_sources+orlix_linux_source/scripts/unifdef.c",
        )
        for path in required:
            self.assertTrue(headers_install_source(path), path)

    def test_rule_selects_the_same_closure_and_variants_share_one_uapi(self) -> None:
        text = UAPI.read_text(encoding="utf-8")
        action = text[text.index('mnemonic = "OrlixLinuxHeadersInstall"'):]
        self.assertNotIn("linux_source[DefaultInfo].files", text)
        self.assertIn("_headers_install_sources", text)
        self.assertIn("linux-source.tar", action)
        self.assertIn("linux_source_archive.py", action)
        self.assertNotIn("linux_inputs", action)
        self.assertIn("patch_files", action)
        self.assertIn("ARCH=arm64", action)
        self.assertIn("headers_install", action)
        self.assertIn('"no-sandbox": "1"', action)
        self.assertNotIn("no-remote-cache", action)
        for prefix in TREE_PREFIXES:
            self.assertIn(f'"{prefix}"', text)
        for exact in EXACT:
            self.assertIn(f'"{exact}"', text)
        build = BUILD.read_text(encoding="utf-8")
        self.assertEqual(build.count("orlix_kernel_uapi("), 1)
        self.assertEqual(build.count('uapi = ":uapi"'), 1)
        self.assertIn("orlix_kernel_uapi_variant(", build)
        for destination in ('"iphoneos"', '"iphonesimulator"'):
            self.assertIn(destination, build)
        for profile in ('"development"', '"release"'):
            self.assertIn(profile, build)

    def test_developer_dir_or_xcrun_cannot_satisfy_hostcc(self) -> None:
        text = UAPI.read_text(encoding="utf-8")
        action = text[text.index('mnemonic = "OrlixLinuxHeadersInstall"'):text.index('mnemonic = "OrlixPromotedUapiProduct"')]
        self.assertIn('hostcc="$clang"', action)
        self.assertIn("*/tools/bin/clang|*/tools/llvm/bin/clang", action)
        self.assertIn('/usr/bin/wc -c "$toolchain_identity" "$clang"', action)
        self.assertIn("ctx.file.clang", action)
        self.assertIn('Label("@orlix_kernel_toolchain//:tools/bin/clang")', text)
        self.assertNotIn("XcodeDefault.xctoolchain", action)
        self.assertNotIn("xcrun", action)
        self.assertNotIn('hostcc="$DEVELOPER_DIR', action)
        self.assertNotIn("/usr/bin/clang", action)
        script = r"""
set -euo pipefail
clang="$1"
case "$clang" in
  */tools/bin/clang|*/tools/llvm/bin/clang) ;;
  *) printf '%s\n' "headers install must execute the staged clang" >&2; exit 1 ;;
esac
hostcc="$clang"
case "$hostcc" in
  */Toolchains/XcodeDefault.xctoolchain/*|*/xcrun) exit 1 ;;
esac
printf '%s\n' "$hostcc"
"""
        rejected = (
            "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang",
            "/usr/bin/xcrun",
            "/opt/homebrew/bin/clang",
        )
        for candidate in rejected:
            result = subprocess.run(
                ["bash", "-c", script, "bash", candidate],
                capture_output=True,
                text=True,
            )
            self.assertNotEqual(result.returncode, 0, candidate)
        for candidate in ("external/orlix_kernel_toolchain/tools/bin/clang", "external/orlix_kernel_toolchain/tools/llvm/bin/clang"):
            result = subprocess.run(
                ["bash", "-c", script, "bash", candidate],
                check=True,
                capture_output=True,
                text=True,
            )
            self.assertEqual(result.stdout.strip(), candidate)


if __name__ == "__main__":
    unittest.main()
