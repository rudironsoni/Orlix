#!/usr/bin/env python3
"""Rule text for separate OpenSSL and libssh2 archive pins."""

from __future__ import annotations

import subprocess
import unittest
from pathlib import Path

RULE = Path(__file__).with_name("native_bundle.bzl")


def command_texts(rule_text: str) -> list[str]:
    marker = 'command = r"""'
    commands = []
    start = 0
    while True:
        found = rule_text.find(marker, start)
        if found < 0:
            return commands
        body = found + len(marker)
        end = rule_text.index('"""', body)
        commands.append(rule_text[body:end])
        start = end + 3


class SshArchivePinTest(unittest.TestCase):
    def test_commands_pin_tools_dates_and_work_dirs(self) -> None:
        rule_text = RULE.read_text(encoding="utf-8")
        commands = command_texts(rule_text)
        self.assertEqual(len(commands), 2)
        openssl = next(command for command in commands if "build_libs SOURCE_DATE_EPOCH=1" in command)
        libssh2 = next(command for command in commands if "CRYPTO_BACKEND=OpenSSL" in command)
        self.assertNotIn("mktemp", rule_text)
        self.assertNotIn("TMPDIR", rule_text)
        self.assertNotIn("/opt/homebrew", rule_text)
        self.assertNotIn("OrlixSshAppleArchives", rule_text)
        self.assertIn('work="$exec_root/orlix-openssl-$destination-$architecture"', openssl)
        self.assertIn("export SOURCE_DATE_EPOCH=1", openssl)
        self.assertIn("build_libs SOURCE_DATE_EPOCH=1", openssl)
        self.assertIn("install_sw SOURCE_DATE_EPOCH=1", openssl)
        self.assertNotIn("libssh2", openssl)
        self.assertNotIn("cmake", openssl)
        self.assertIn('work="$exec_root/orlix-libssh2-$destination-$architecture"', libssh2)
        self.assertIn("export SOURCE_DATE_EPOCH=1", libssh2)
        self.assertNotIn("./Configure", libssh2)
        self.assertNotIn("build_libs", libssh2)
        self.assertIn('"SOURCE_DATE_EPOCH": "1"', rule_text)
        self.assertIn('"PATH": "/usr/bin:/bin"', rule_text)
        self.assertEqual(rule_text.count("use_default_shell_env = False"), 2)
        self.assertEqual(rule_text.count('"no-remote-exec": "1"'), 2)
        self.assertNotIn("no-remote-cache", rule_text)
        for command in commands:
            completed = subprocess.run(["/bin/bash", "-n"], input=command, text=True, capture_output=True)
            self.assertEqual(completed.returncode, 0, completed.stderr)


if __name__ == "__main__":
    unittest.main()
