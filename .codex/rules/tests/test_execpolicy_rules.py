#!/usr/bin/env python3
import json
import subprocess
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
RULES = ROOT / ".codex" / "rules" / "orlix.rules"


def execpolicy_decision(command):
    result = subprocess.run(
        [
            "codex",
            "execpolicy",
            "check",
            "--pretty",
            "--rules",
            str(RULES),
            "--",
            *command,
        ],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if result.returncode != 0:
        raise AssertionError(result.stderr + result.stdout)
    start = result.stdout.find("{")
    if start == -1:
        raise AssertionError(f"missing json output: {result.stdout!r}")
    return json.loads(result.stdout[start:])


class ExecPolicyRulesTests(unittest.TestCase):
    def assert_decision(self, command, expected):
        payload = execpolicy_decision(command)
        self.assertEqual(payload.get("decision"), expected, payload)

    def test_git_push_policy(self):
        self.assert_decision(["git", "push", "origin", "main"], "allow")

    def test_destructive_remove_policy(self):
        self.assert_decision(["rm", "-rf", "Build"], "forbidden")

    def test_expensive_make_policy(self):
        command = [
            "timeout",
            "18000",
            "make",
            "-f",
            "OrlixOS/Makefile",
            "test",
            "PROFILE=release",
        ]
        self.assertEqual(execpolicy_decision(command), {"matchedRules": []})

    def test_beta_archive_policy(self):
        command = [
            "timeout",
            "18000",
            "make",
            "beta-archive",
            "ORLIX_DEVELOPMENT_TEAM=ZQ3L7M567L",
        ]
        self.assertEqual(execpolicy_decision(command), {"matchedRules": []})

    def test_simctl_policy(self):
        self.assert_decision(["xcrun", "simctl", "shutdown", "all"], "allow")

    def test_xcodebuild_policy(self):
        command = [
            "xcodebuild",
            "-project",
            "Orlix.xcodeproj",
            "-scheme",
            "Orlix",
            "archive",
        ]
        self.assert_decision(command, "allow")


if __name__ == "__main__":
    unittest.main()
