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

    def test_unset_ccache_dir_compiles_directly(self) -> None:
        start = self.rule.index('if [ -z "${CCACHE_DIR:-}" ]; then')
        snippet = self.rule[start:self.rule.index("fi\n", start) + 3]
        script = "set -euo pipefail\nunset CCACHE_DIR\nunset ORLIX_COMPILER_LAUNCHER\n" + snippet + '\ntest -z "${ORLIX_COMPILER_LAUNCHER}"\n'
        result = subprocess.run(["bash"], input=script, text=True, capture_output=True, check=False)
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

    def test_isa_restore_declares_gmake_and_awk(self) -> None:
        restore = _action_tail(self.rule, "OrlixTctiIsaRestore")
        self.assertNotIn("no-remote-cache", restore)
        self.assertIn('"no-remote-exec": "1"', restore)
        self.assertIn("ctx.file.gmake", self.rule)
        self.assertIn("ctx.file.awk", self.rule)
        self.assertNotIn("/opt/homebrew/bin/gmake", restore)
        self.assertIn('export PATH="$(/usr/bin/dirname "$awk_bin"):/usr/bin:/bin"', restore)

    def test_kernel_archive_declares_host_tools(self) -> None:
        archive = _action_tail(self.rule, "OrlixKernelMachOArchive")
        self.assertNotIn("no-remote-cache", archive)
        self.assertIn('"no-remote-exec": "1"', archive)
        self.assertIn('"no-sandbox": "1"', archive)
        self.assertNotIn("no-remote-cache", self.rule)
        self.assertIn("require_declared", self.rule)
        self.assertIn("ctx.file.guest_clang", self.rule)
        self.assertIn("ctx.file.provenance_makefile", self.rule)
        self.assertIn("make/tcti-proof-registry-provenance.mk", self.rule)
        self.assertNotIn("os.path.realpath", self.rule)

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
