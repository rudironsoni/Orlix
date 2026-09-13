from __future__ import annotations

import os
import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from Tools.AgentHarness import proof, state
from Tools.AgentHarness.hooks import common, pre_tool_use


class HooksStateProofTests(unittest.TestCase):
    def test_semantic_scope_and_generated_paths(self):
        envelope = {
            "owned_paths": ["src/**"],
            "read_only_paths": ["src/vendor/**"],
            "forbidden_paths": ["src/private/**"],
        }
        common.enforce_paths({"src/main.py"}, envelope)
        for path in ("other/main.py", "src/vendor/a.py", "src/private/a.py", ".codex/config.toml"):
            with self.assertRaises(common.HookBlocked):
                common.enforce_paths({path}, envelope)

    def test_pre_tool_hook_requires_current_envelope(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            with patch.dict(os.environ, {"ORLIX_BUILD_ROOT": str(root / "Build")}), patch.object(pre_tool_use, "repository_root", return_value=root):
                pre_tool_use.run({"tool_name": "read", "tool_input": {"path": "src/a.py"}})
                pre_tool_use.run({"tool_name": "exec_command", "tool_input": {"cmd": "make agent-task-envelope TASK=a"}})
                with self.assertRaises(common.HookBlocked):
                    pre_tool_use.run({"tool_name": "write", "tool_input": {"path": "src/a.py"}})

    def test_continuation_and_proof(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            subprocess.run(["git", "init", "-q", str(root)], check=True)
            subprocess.run(
                ["git", "-C", str(root), "-c", "user.name=Test", "-c", "user.email=test@example.com", "commit", "--allow-empty", "-m", "test"],
                check=True,
                capture_output=True,
            )
            envelope = {
                "task": {"identity": "task", "path": "task.md", "slug": "task", "status": "doing"},
                "required_proof": ["proof"],
                "build_intents": ["build"],
                "verification_intents": ["verify"],
            }
            with patch.dict(os.environ, {"ORLIX_BUILD_ROOT": str(root / "Build")}):
                state.write_json(state.agent_root(root) / "task-envelope.json", envelope)
                continuation = state.save_continuation(root, {"files_changed": ["b", "a"], "next_gate": "verify"})
                self.assertEqual(continuation["files_changed"], ["a", "b"])
                state.record_event(root, "pre-compact", {})
                self.assertEqual(state.restore_continuation(root)["next_gate"], "verify")
                current = proof.revision(root)
                record = {
                    "status": "passed",
                    "repository_revision": current,
                    "evidence_id": "e1",
                    "artifact_identity": "not-applicable",
                    "destination": "host",
                    "profile": "development",
                }
                evidence = {
                    "proof": [dict(record, requirement="proof")],
                    "build_intents": [dict(record, requirement="build")],
                    "verification_intents": [dict(record, requirement="verify")],
                    "known_failures": [],
                }
                self.assertEqual(proof.validate_completion(root, envelope, evidence), [])
                state.record_event(root, "post-tool-use", {"failure": True, "command_identity": {"sha256": "failed"}})
                self.assertIn("unresolved tool failure: failed", proof.validate_completion(root, envelope, evidence))
                evidence["resolved_failure_ids"] = ["failed"]
                self.assertEqual(proof.validate_completion(root, envelope, evidence), [])
                evidence["proof"] = []
                self.assertIn("missing current required_proof: proof", proof.validate_completion(root, envelope, evidence))

    def test_device_and_signing_gates_fail_closed(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            subprocess.run(["git", "init", "-q", str(root)], check=True)
            subprocess.run(
                ["git", "-C", str(root), "-c", "user.name=Test", "-c", "user.email=test@example.com", "commit", "--allow-empty", "-m", "test"],
                check=True,
                capture_output=True,
            )
            with patch.dict(os.environ, {"ORLIX_BUILD_ROOT": str(root / "Build")}):
                with self.assertRaises(common.HookBlocked):
                    common.gate(root, "physical_device_allowed")
                with self.assertRaises(common.HookBlocked):
                    common.gate(root, "signing_allowed")
                state.write_json(
                    state.agent_root(root) / "gates.json",
                    {"signing_allowed": True, "signing_allowed_revision": proof.revision(root)},
                )
                common.gate(root, "signing_allowed")


if __name__ == "__main__":
    unittest.main()
