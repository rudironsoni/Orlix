"""Linux rule-text proof for UAPI inputs and mlibc cache tags."""

from pathlib import Path
import os
import subprocess
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
        self.assertIn('endswith(".patch")', text)
        self.assertIn('endswith(".diff")', text)
        self.assertIn("*.patch|*.diff)", action)
        self.assertIn('"no-remote-exec": "1"', action)
        root = ROOT / "OrlixKernel/Sources/ports/orlix/patches"
        selected = [
            path.relative_to(root).as_posix()
            for path in root.rglob("*")
            if path.is_file() and path.suffix in {".patch", ".diff"}
        ]
        self.assertIn("0001-kbuild-add-orlix-clang-target.patch", selected)
        self.assertIn("0004-sched-add-arch-cond-resched-hook.patch", selected)
        self.assertNotIn("exceptions/README.md", selected)
        self.assertNotIn("exceptions/0004-sched-add-arch-cond-resched-hook.patch.md", selected)
        self.assertTrue(all(name.endswith((".patch", ".diff")) for name in selected))
        skipped = subprocess.run(
            [
                "bash",
                "-c",
                'case "$1" in *.patch|*.diff) echo apply ;; *) echo skip ;; esac',
                "bash",
                "exceptions/0004-sched-add-arch-cond-resched-hook.patch.md",
            ],
            check=True,
            capture_output=True,
            text=True,
        )
        self.assertEqual(skipped.stdout.strip(), "skip")

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

    def test_unset_ccache_dir_compiles_with_clang(self) -> None:
        text = SYSROOT.read_text(encoding="utf-8")
        self.assertNotIn("CCACHE_DIR is required", text)
        self.assertEqual(text.count('if [ "$launcher" = /opt/homebrew/bin/ccache ] && [ -z "${CCACHE_DIR:-}" ]; then'), 2)
        script = r"""
launcher="${ORLIX_COMPILER_LAUNCHER-/opt/homebrew/bin/ccache}"
if [ "$launcher" = /opt/homebrew/bin/ccache ] && [ -z "${CCACHE_DIR:-}" ]; then
  launcher=""
fi
clang=clang
compiler=("$clang")
if [ -n "$launcher" ]; then compiler=("$launcher" "$clang"); fi
printf '%s\n' "${compiler[0]}"
"""
        base = {"PATH": os.environ["PATH"]}
        unset = subprocess.run(["bash", "-c", script], env=base, check=True, capture_output=True, text=True)
        self.assertEqual(unset.stdout.strip(), "clang")
        cached = subprocess.run(
            ["bash", "-c", script],
            env={**base, "CCACHE_DIR": "/tmp/orlix-ccache"},
            check=True,
            capture_output=True,
            text=True,
        )
        self.assertEqual(cached.stdout.strip(), "/opt/homebrew/bin/ccache")


if __name__ == "__main__":
    unittest.main()
