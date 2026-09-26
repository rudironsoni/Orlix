"""Rule text for the Mach-O kernel action. No Kbuild and no product build."""

from __future__ import annotations

import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
RULE = ROOT / "bazel/feasibility/kernel/kernel_macho.bzl"
BUILD = ROOT / "bazel/feasibility/kernel/BUILD.bazel"
UNREAD = (
    "make/tcti-proof-registry-provenance.mk",
    "OrlixOS/Sources/distribution/target-settings.xcconfig",
)


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
        self.assertIn('shell.get("CCACHE_DIR")', self.rule)
        self.assertIn('shell.get("CCACHE_DISABLE")', self.rule)
        self.assertNotIn("env[\"TMPDIR\"]", self.rule)

    def test_unread_extra_inputs_are_gone(self) -> None:
        self.assertNotIn("extra_inputs", self.rule)
        self.assertNotIn("extra_inputs", self.build)
        self.assertNotIn("extra_files", self.rule)
        for path in UNREAD:
            self.assertNotIn(path, self.build)
        self.assertNotIn("target-settings.xcconfig", self.rule)
        script_end = self.rule.index('""")', self.rule.index("ctx.actions.write(script"))
        self.assertNotIn("tcti-proof-registry-provenance.mk", self.rule[script_end:])

    def test_remote_cache_stays_disabled(self) -> None:
        archive = _action_tail(self.rule, "OrlixKernelMachOArchive")
        self.assertIn('"no-remote-cache": "1"', archive)
        self.assertIn('"no-remote-exec": "1"', archive)
        self.assertIn('"no-sandbox": "1"', archive)
        self.assertEqual(self.rule.count('"no-remote-cache": "1"'), 2)

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
