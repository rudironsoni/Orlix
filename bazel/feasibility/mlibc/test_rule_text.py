"""Linux rule-text proof for UAPI inputs and mlibc cache tags."""

from pathlib import Path
import subprocess
import tempfile
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
            self.assertNotIn("no-remote-cache", action)
            self.assertIn('"no-remote-exec": "1"', action)
            self.assertIn("use_default_shell_env = False", action)
        self.assertNotIn("TMPDIR", text)
        self.assertNotIn("/opt/homebrew/bin/ccache", text)
        self.assertNotIn("command -v meson", text)
        self.assertNotIn("command -v ninja", text)
        self.assertNotIn("command -v llvm-ar", text)
        self.assertNotIn("command -v llvm-strip", text)
        self.assertNotIn("command -v ld.lld", text)
        self.assertNotIn("orlix-mlibc-state", text)
        self.assertNotIn('"DEVELOPER_DIR"', text)
        self.assertNotIn("ORLIX_PINNED_DEVELOPER_DIR", text)
        self.assertIn("-u DEVELOPER_DIR", text)
        self.assertIn("-u SDKROOT", text)
        self.assertNotIn("xcodebuild", text)
        self.assertNotIn("xcrun", text)
        self.assertNotIn("/usr/bin/nm", text)
        self.assertIn("ORLIX_MLIBC_TOOL_BIN", text)
        self.assertIn("ORLIX_MLIBC_SDK", text)
        self.assertIn('guest_clang="$tool_root/llvm/bin/clang"', text)
        self.assertIn("'$guest_clang', '--target=aarch64-linux-gnu'", text)
        self.assertIn("-fuse-ld=$lld", text)
        self.assertIn("ctx.file.ld_lld", text)
        self.assertIn("ctx.file.guest_clang", text)
        self.assertNotIn("\"c = ['$clang', '--target=aarch64-linux-gnu']\"", text)
        self.assertIn("'-isysroot', '$sdkroot'", text)
        self.assertIn("llvm-nm", text)
        self.assertIn("-ffile-prefix-map=$tool_root=/orlix/tools", text)
        self.assertIn("-ffile-prefix-map=$sdkroot=/orlix/macos-sdk", text)
        self.assertIn("ctx.files.macos_sdk", text)
        self.assertIn("-nostdlibinc", compiler)
        self.assertIn("mlibc-work", text)
        self.assertIn('"$ar" rcs "$runtime_out"', compiler)
        self.assertNotIn('"$work"/*.o', text)

    def test_compiler_runtime_archives_relative_member_names(self) -> None:
        script = r"""
set -euo pipefail
work="$1"
mkdir -p "$work"
printf 'obj\n' > "$work/addtf3.o"
ar_out="$2"
(
  cd "$work"
  members=()
  for object in *.o; do members+=("$object"); done
  printf '%s\n' "${members[@]}"
)
"""
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            work = root / "compiler-rt-objects"
            listed = subprocess.run(
                ["bash", "-c", script, "bash", str(work), str(root / "libcompiler_rt.a")],
                check=True,
                capture_output=True,
                text=True,
            )
        self.assertEqual(listed.stdout.splitlines(), ["addtf3.o"])

    def test_empty_meson_plan_does_not_expand_an_empty_array(self) -> None:
        text = SYSROOT.read_text(encoding="utf-8")
        self.assertIn('if [ "${#configure_args[@]}" -gt 0 ]; then', text)
        self.assertNotIn('"$meson_bin" setup "${configure_args[@]}"', text)
        script = r"""
set -euo pipefail
plan="$1"
configure_args=()
if [ -n "$plan" ]; then
  read -r -a configure_args <<< "$plan"
fi
meson_setup=(meson setup)
if [ "${#configure_args[@]}" -gt 0 ]; then
  meson_setup+=("${configure_args[@]}")
fi
meson_setup+=(builddir sourcedir --wrap-mode=nodownload)
printf '%s\n' "${meson_setup[@]}"
"""
        empty = subprocess.run(
            ["bash", "-c", script, "bash", ""],
            check=True,
            capture_output=True,
            text=True,
        )
        self.assertEqual(empty.stdout.splitlines(), ["meson", "setup", "builddir", "sourcedir", "--wrap-mode=nodownload"])
        flagged = subprocess.run(
            ["bash", "-c", script, "bash", "--reconfigure --clearcache"],
            check=True,
            capture_output=True,
            text=True,
        )
        self.assertEqual(
            flagged.stdout.splitlines()[:4],
            ["meson", "setup", "--reconfigure", "--clearcache"],
        )


if __name__ == "__main__":
    unittest.main()
