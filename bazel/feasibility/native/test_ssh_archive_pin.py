#!/usr/bin/env python3
"""Rule text for the OrlixSshAppleArchives temp path and OpenSSL date pin."""

from __future__ import annotations

import unittest
from pathlib import Path

RULE = Path(__file__).with_name("native_bundle.bzl")


def command_text(rule_text: str) -> str:
    marker = 'command = r"""'
    start = rule_text.index(marker) + len(marker)
    end = rule_text.index('"""', start)
    return rule_text[start:end]


class SshArchivePinTest(unittest.TestCase):
    def test_command_has_no_mktemp_path_and_no_tmpdir_copy(self) -> None:
        rule_text = RULE.read_text(encoding="utf-8")
        command = command_text(rule_text)
        self.assertNotIn("mktemp", command)
        self.assertNotIn("TMPDIR", command)
        self.assertNotIn("TMPDIR", rule_text)
        self.assertIn('work="$exec_root/orlix-ssh-work"', command)
        self.assertIn("export SOURCE_DATE_EPOCH=1", command)
        self.assertIn("build_libs SOURCE_DATE_EPOCH=1", command)
        self.assertIn("install_sw SOURCE_DATE_EPOCH=1", command)
        self.assertIn('"SOURCE_DATE_EPOCH": "1"', rule_text)
        self.assertIn("use_default_shell_env = False", rule_text)
        self.assertIn('"no-remote-exec": "1"', rule_text)
        self.assertNotIn("no-remote-cache", rule_text)


if __name__ == "__main__":
    unittest.main()
