from __future__ import annotations

import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from Tools.AgentHarness.rulesync_reports import (
    paths_from_name_status,
    reject,
    revision_inventory,
    validate_generated_write_paths,
    validate_inventory,
)


def record(path: str, feature: str = "skills") -> dict:
    return {"target": "fixture", "feature": feature, "generated_path": path, "generated_destination_root": "."}


class RuleSyncGuardTests(unittest.TestCase):
    def test_revision_inventory_uses_current_pin(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)

            def extract(_repo, _revision, destination):
                (destination / ".rulesync").mkdir()
                (destination / "rulesync.jsonc").write_text("{}\n", encoding="utf-8")

            def inspect(source):
                self.assertEqual((source / ".rulesync" / "VERSION").read_text(encoding="utf-8"), "16.26.1\n")
                return ([record("AGENTS.md", "rules")], {}, [])

            with patch("Tools.AgentHarness.rulesync_reports.extract_revision", side_effect=extract), patch(
                "Tools.AgentHarness.rulesync_reports.pinned_version", return_value="16.26.1"
            ), patch("Tools.AgentHarness.rulesync_reports.inventory", side_effect=inspect):
                self.assertEqual(revision_inventory(root, "base"), [record("AGENTS.md", "rules")])

    def test_protected_destinations_and_github_exception(self):
        records = [
            record(".codex/config.toml", "permissions"),
            record(".claude/settings.json", "hooks"),
            record(".cursor/hooks.json", "hooks"),
            record(".agents/skills/a/SKILL.md"),
            record(".github/skills/a/SKILL.md"),
            record(".github/hooks/hooks.json", "hooks"),
            record(".github/agents/a.agent.md", "subagents"),
            record(".github/permissions/policy.json", "permissions"),
            record(".github/mcp/config.json", "mcp"),
            record(".vscode/settings.json", "permissions"),
            record(".vscode/mcp.json", "mcp"),
        ]
        for path in (
            ".codex/new.md",
            ".claude/new.md",
            ".cursor/new.md",
            ".agents/new.md",
            ".github/skills/a/SKILL.md",
            ".github/hooks/hooks.json",
            ".github/agents/a.agent.md",
            ".github/permissions/policy.json",
            ".github/mcp/config.json",
            ".vscode/settings.json",
            ".vscode/mcp.json",
        ):
            with self.assertRaises(ValueError, msg=path):
                reject({path}, records)
        reject({".github/workflows/build.yml", ".github/actions/a/action.yml", ".github/ISSUE_TEMPLATE/bug.yml"}, records)
        reject({".github/copilot-instructions.md"}, records + [record(".github/copilot-instructions.md", "rules")])
        reject({".rulesync/skills/a/SKILL.md"}, records)

    def test_deletion_rename_copy_and_base_ownership(self):
        changed = paths_from_name_status(
            b"A\0added\0M\0modified\0D\0deleted\0T\0typed\0R100\0old\0new\0C100\0source\0copy\0"
        )
        self.assertEqual(changed, {"added", "modified", "deleted", "typed", "old", "new", "source", "copy"})
        base_records = [record(".github/skills/removed/SKILL.md")]
        with self.assertRaises(ValueError):
            reject({".github/skills/removed/SKILL.md"}, base_records)
        validate_generated_write_paths({".codex/config.toml"}, [record(".codex/config.toml", "permissions")])
        with self.assertRaises(ValueError):
            validate_generated_write_paths({"unexpected.txt"}, [record(".codex/config.toml", "permissions")])
        with self.assertRaisesRegex(ValueError, "outside authorized destinations"):
            validate_inventory([record("unexpected.txt")])
        with self.assertRaisesRegex(ValueError, "outside authorized destinations"):
            validate_inventory([record(".github/skills/a/SKILL.md", "rules")])

    def test_workflow_contracts(self):
        root = Path(__file__).resolve().parents[3]
        guard = (root / ".github/workflows/rulesync-generated-output-guard.yml").read_text(encoding="utf-8")
        validate = (root / ".github/workflows/rulesync-validate.yml").read_text(encoding="utf-8")
        generate = (root / ".github/workflows/rulesync-generate.yml").read_text(encoding="utf-8")
        self.assertIn("pull_request:", guard)
        self.assertIn('BASE="${{ github.event.pull_request.base.sha }}"', guard)
        self.assertIn('HEAD="${{ github.event.pull_request.head.sha }}"', guard)
        self.assertIn('rulesync generate --output-roots "$RUNNER_TEMP/generated"', validate)
        self.assertIn("Build/AgentHarness/rulesync/", validate)
        self.assertIn("rulesync generate", generate)
        self.assertIn("make agent-rules-validate-write-set", generate)
        self.assertIn("git diff --cached --quiet", generate)
        self.assertIn("git push origin HEAD:main", generate)


if __name__ == "__main__":
    unittest.main()
