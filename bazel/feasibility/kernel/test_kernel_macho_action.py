"""Rule text for the Mach-O kernel action. No Kbuild and no product build."""

from __future__ import annotations

import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
RULE = ROOT / "bazel/feasibility/kernel/kernel_macho.bzl"
BUILD = ROOT / "bazel/feasibility/kernel/BUILD.bazel"


def _action_tail(text: str, mnemonic: str) -> str:
    start = text.index(f'mnemonic = "{mnemonic}"')
    requirements = text.index("execution_requirements = ", start)
    return text[start:text.index("\n", requirements)]


class KernelMachOActionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.rule = RULE.read_text(encoding="utf-8")
        cls.build = BUILD.read_text(encoding="utf-8")

    def test_shell_env_is_pinned(self) -> None:
        archive = _action_tail(self.rule, "OrlixKernelMachOArchive")
        self.assertIn("use_default_shell_env = False", archive)
        self.assertNotIn("use_default_shell_env = True", self.rule)
        self.assertNotIn("TMPDIR", self.rule)
        self.assertNotIn('shell.get("CCACHE_DIR")', self.rule)
        self.assertNotIn('env["CCACHE_DIR"]', self.rule)
        self.assertIn('shell.get("CCACHE_DISABLE")', self.rule)
        self.assertNotIn("env[\"TMPDIR\"]", self.rule)
        self.assertNotIn("CCACHE_DIR is required", self.rule)

    def test_unset_ccache_dir_uses_default_cache(self) -> None:
        start = self.rule.index('if [ -n "${CCACHE_DISABLE:-}" ]; then')
        snippet = self.rule[start:self.rule.index("fi\n", start) + 3]
        empty_home = "set -euo pipefail\nunset CCACHE_DIR\nunset CCACHE_DISABLE\nunset ORLIX_COMPILER_LAUNCHER\nexport HOME=/var/empty\nccache_staged=/staged/ccache\n" + snippet + 'test -z "${ORLIX_COMPILER_LAUNCHER}"\ntest -z "${CCACHE_DIR:-}"\n'
        result = subprocess.run(["bash"], input=empty_home, text=True, capture_output=True, check=False)
        self.assertEqual(result.returncode, 0, result.stderr)
        writable = "set -euo pipefail\nunset CCACHE_DIR\nunset CCACHE_DISABLE\nunset ORLIX_COMPILER_LAUNCHER\nexport HOME=\"$(/usr/bin/mktemp -d)\"\nccache_staged=/staged/ccache\n" + snippet + 'test "$ORLIX_COMPILER_LAUNCHER" = "$ccache_staged"\ntest "$CCACHE_DIR" = "$HOME/Library/Caches/Orlix/ccache"\ntest -d "$CCACHE_DIR"\n'
        result = subprocess.run(["bash"], input=writable, text=True, capture_output=True, check=False)
        self.assertEqual(result.returncode, 0, result.stderr)
        disabled = "set -euo pipefail\nunset CCACHE_DIR\nexport CCACHE_DISABLE=1\nexport HOME=/var/empty\nunset ORLIX_COMPILER_LAUNCHER\nccache_staged=/staged/ccache\n" + snippet + 'test -z "${ORLIX_COMPILER_LAUNCHER}"\n'
        result = subprocess.run(["bash"], input=disabled, text=True, capture_output=True, check=False)
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_unread_extra_inputs_are_gone(self) -> None:
        self.assertNotIn("extra_inputs", self.rule)
        self.assertNotIn("extra_inputs", self.build)
        self.assertNotIn("extra_files", self.rule)
        self.assertNotIn("OrlixOS/Sources/distribution/target-settings.xcconfig", self.build)
        self.assertNotIn("target-settings.xcconfig", self.rule)
        self.assertNotIn("make/tcti-proof-registry-provenance.mk", self.build)
        script = self.rule[self.rule.index("ctx.actions.write(script"):self.rule.index('""")', self.rule.index("ctx.actions.write(script"))]
        self.assertNotIn("os.path.realpath", script)
        self.assertIn("make/tcti-proof-registry-provenance.mk", script)

    def test_isa_restore_executes_staged_tools(self) -> None:
        restore = _action_tail(self.rule, "OrlixTctiIsaRestore")
        self.assertNotIn("no-remote-cache", restore)
        self.assertIn('"no-remote-exec": "1"', restore)
        self.assertNotIn("/usr/bin", restore)
        self.assertNotIn("/opt/homebrew", restore)
        self.assertNotIn("xcrun", restore)
        self.assertNotIn("mktemp", restore)
        self.assertIn('export PATH="$tool_bin"', restore)
        self.assertIn('export DYLD_LIBRARY_PATH="$tool_lib"', restore)
        self.assertIn('export PYTHONHOME="$python_home"', restore)
        self.assertIn('"$python_bin"', restore)
        self.assertIn("source_state.resume", restore)
        self.assertIn("ctx.files.tool_libs", restore)
        self.assertIn("ctx.files.python_home", restore)
        self.assertIn('"$printf_bin"', restore)
        self.assertIn("tools/bin/meson-python", self.rule)
        self.assertIn("tools/bin/printf", self.rule)

    def test_kernel_archive_executes_staged_tools(self) -> None:
        archive = _action_tail(self.rule, "OrlixKernelMachOArchive")
        self.assertNotIn("no-remote-cache", archive)
        self.assertIn('"no-remote-exec": "1"', archive)
        self.assertIn('"no-sandbox": "1"', archive)
        self.assertNotIn("/opt/homebrew", self.rule)
        self.assertNotIn("xcrun", self.rule)
        self.assertNotIn("xcodebuild", self.rule)
        self.assertNotIn("command -v gmake", self.rule)
        self.assertIn('gmake="$gmake_staged"', self.rule)
        self.assertIn('clang="$guest_clang_staged"', self.rule)
        self.assertIn('"$host_clang"', self.rule)
        self.assertIn('DYLD_LIBRARY_PATH="$tool_lib:$guest_lib"', self.rule)
        self.assertIn("ctx.files.macos_sdk", self.rule)
        self.assertIn("ctx.files.bison_share", self.rule)
        self.assertIn("SDKSettings.plist", self.rule)
        self.assertIn('"$bison_share/m4sugar/m4sugar.m4"', self.rule)
        self.assertIn("ctx.files.guest_clang_support", self.rule)
        self.assertIn("make/tcti-proof-registry-provenance.mk", self.rule)
        self.assertIn('"$tool_lib"', self.rule)
        self.assertIn('"$guest_lib"', self.rule)
        self.assertIn('"$provenance"', self.rule)
        self.assertIn("source_state.resume", self.rule)

    def test_orlixcc_log_is_failure_only(self) -> None:
        failure, success = self.rule.split('if [ "$kbuild_status" -ne 0 ]; then', 1)[1].split("\nfi\n", 1)
        self.assertIn('"$kbuild_err"', failure)
        self.assertNotIn("grep -v", failure)
        self.assertIn("ORLIXCC", success)
        self.assertLess(success.index("grep -v"), success.index("source_state.record"))

    def test_source_edit_does_not_wipe_kbuild_state(self) -> None:
        self.assertIn('ORLIX_KERNEL_INCREMENTAL="${ORLIX_KERNEL_INCREMENTAL:-1}"', self.rule)
        self.assertIn("source_state.resume", self.rule)
        self.assertIn("source_state.sync", self.rule)
        self.assertNotIn('rm -rf "$work"', self.rule)
        self.assertNotIn("0005-uapi-errno-orlix-comment.patch", self.rule)
        self.assertNotIn("0005-uapi-errno-orlix-comment.patch", self.build)

    def test_action_script_parses(self) -> None:
        start = self.rule.index('ctx.actions.write(script, r"""')
        start = self.rule.index("\n", start) + 1
        script = self.rule[start:self.rule.index('""")', start)]
        result = subprocess.run(["bash", "-n"], input=script, text=True, capture_output=True, check=False)
        self.assertEqual(result.returncode, 0, result.stderr)


if __name__ == "__main__":
    unittest.main()
