"""Linux rule-text proof for UAPI inputs and mlibc cache tags."""

from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[3]
UAPI = ROOT / "bazel/feasibility/kernel/kernel_uapi.bzl"
SYSROOT = ROOT / "bazel/feasibility/mlibc/mlibc_sysroot.bzl"


def action_text(text: str, mnemonic: str) -> str:
    marker = f'mnemonic = "{mnemonic}"'
    start = text.index(marker)
    next_marker = text.find('mnemonic = "', start + len(marker))
    if next_marker < 0:
        return text[start:]
    return text[start:next_marker]


class UapiMlibcRuleTextTests(unittest.TestCase):
    def test_headers_install_takes_patches_and_toolchain_identity(self) -> None:
        text = UAPI.read_text(encoding="utf-8")
        action = action_text(text, "OrlixLinuxHeadersInstall")
        self.assertIn("//OrlixKernel/Sources:linux_patches", text)
        self.assertIn("@orlix_kernel_toolchain//:identity.json", text)
        self.assertIn("patch_files", action)
        self.assertIn("ctx.file.toolchain_identity", action)
        self.assertIn("ctx.file.header_digest", action)
        self.assertIn("patch_list", action)
        self.assertNotIn("no-remote-cache", action)
        self.assertNotIn("0005-uapi-errno-orlix-comment.patch", text)
        self.assertIn('"no-remote-exec": "1"', action)

    def test_sysroot_ignores_uapi_sidecar_and_keeps_cache_tags(self) -> None:
        text = SYSROOT.read_text(encoding="utf-8")
        sysroot = action_text(text, "OrlixMLibCSysroot")
        compiler = action_text(text, "OrlixCompilerRuntime")
        self.assertNotIn("uapi.sha256", text)
        self.assertNotIn("uapi.sha256", sysroot)
        self.assertNotIn("uapi.uapi_digest", sysroot)
        self.assertIn("uapi.headers", sysroot)
        self.assertIn("meson_setup_plan", text)
        self.assertIn(".ninja_log", text)
        self.assertIn("report_compiler_edges", text)
        self.assertNotIn("configure_args=(--reconfigure)", text)
        self.assertNotIn("-newer", text)
        for action in (compiler, sysroot):
            self.assertIn('"no-remote-cache": "1"', action)
            self.assertIn('"no-remote-exec": "1"', action)


if __name__ == "__main__":
    unittest.main()
