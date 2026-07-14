#!/usr/bin/env python3
import json
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
HOOK_DIR = ROOT / ".codex" / "hooks"
ACTIVE_HOOKS = (
    "orlix_hook_common.py",
    "pre_tool_use_guard.py",
    "permission_request_guard.py",
    "post_tool_use_review.py",
    "stop_claim_check.py",
)
RETIRED_ADAPTERS = (
    "plan_context_guard.py",
    "pre_tool_use_policy",
    "post_tool_use_review",
    "session_start_context",
    "stop_guard",
)
FORBIDDEN_SUBSYSTEM_TERMS = (
    "orlix-tcti",
    "selected_task_policy",
    "unauthorized_physical_command",
    "unauthorized_release_command",
    "external-ssd",
    "external_ssd",
    "xcode-offload",
    "/volumes/1tb",
    "generated_tree",
    "generated upstream/build",
)


class HookScopeTests(unittest.TestCase):
    def test_project_hooks_disengage_outside_the_repository(self):
        source = json.loads((ROOT / ".rulesync" / "hooks.json").read_text())["hooks"]
        with tempfile.TemporaryDirectory() as foreign_root:
            for hooks in source.values():
                for hook in hooks:
                    result = subprocess.run(
                        hook["command"],
                        shell=True,
                        cwd=foreign_root,
                        input="{}",
                        text=True,
                        capture_output=True,
                        check=False,
                    )
                    self.assertEqual(result.returncode, 0, result.stderr)

    def test_generated_codex_hooks_match_rulesync_source(self):
        source = json.loads((ROOT / ".rulesync" / "hooks.json").read_text())["hooks"]
        generated = json.loads((ROOT / ".codex" / "hooks.json").read_text())["hooks"]
        event_names = {
            "preToolUse": "PreToolUse",
            "permissionRequest": "PermissionRequest",
            "postToolUse": "PostToolUse",
            "stop": "Stop",
        }
        expected = {}
        for source_name, generated_name in event_names.items():
            entries = []
            for hook in source[source_name]:
                entry = {key: value for key, value in hook.items() if key != "matcher"}
                wrapped = {"hooks": [entry]}
                if "matcher" in hook:
                    wrapped["matcher"] = hook["matcher"]
                entries.append(wrapped)
            expected[generated_name] = entries
        self.assertEqual(generated, expected)

    def test_lifecycle_hooks_are_subsystem_and_machine_neutral(self):
        paths = [ROOT / ".rulesync" / "hooks.json", ROOT / ".codex" / "hooks.json"]
        paths.extend(HOOK_DIR / name for name in ACTIVE_HOOKS)
        for path in paths:
            text = path.read_text().lower()
            for term in FORBIDDEN_SUBSYSTEM_TERMS:
                with self.subTest(path=path.relative_to(ROOT), term=term):
                    self.assertNotIn(term, text)

    def test_retired_subsystem_hook_adapters_are_absent(self):
        for name in RETIRED_ADAPTERS:
            with self.subTest(name=name):
                self.assertFalse((HOOK_DIR / name).exists())


if __name__ == "__main__":
    unittest.main()
