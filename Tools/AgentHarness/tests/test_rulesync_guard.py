from __future__ import annotations

import json
import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import Tools.AgentHarness.rulesync_reports as rulesync_reports
from Tools.AgentHarness.rulesync_reports import (
    generated_pr_guard,
    paths_from_name_status,
    pr_guard,
    reject,
    revision_inventory,
    validate_generated_write_paths,
    validate_inventory,
)


def record(path: str, feature: str = "skills") -> dict:
    return {"target": "fixture", "feature": feature, "generated_path": path, "generated_destination_root": "."}


def git(root: Path, *arguments: str, input_text: str | None = None) -> str:
    return subprocess.run(
        ["git", *arguments], cwd=root, check=True, text=True, input=input_text, capture_output=True
    ).stdout.strip()


def commit(root: Path, message: str) -> str:
    git(root, "add", "-A")
    git(root, "commit", "-qm", message)
    return git(root, "rev-parse", "HEAD")


class RuleSyncGuardTests(unittest.TestCase):
    def generated_repository(
        self,
        root: Path,
        *,
        source_kind: str = "main",
        later_path: str | None = None,
        wrong_parent: bool = False,
        generated_text: str = "generated\n",
    ) -> tuple[str, str, dict]:
        git(root, "init", "-q", "-b", "main")
        git(root, "config", "user.name", "Test")
        git(root, "config", "user.email", "test@example.com")
        (root / ".rulesync" / "rules").mkdir(parents=True)
        (root / ".rulesync" / "VERSION").write_text("16.26.1\n", encoding="utf-8")
        (root / ".rulesync" / "rules" / "overview.md").write_text("source\n", encoding="utf-8")
        (root / "rulesync.jsonc").write_text("{}\n", encoding="utf-8")
        source = commit(root, "source")
        if source_kind == "branch":
            git(root, "switch", "-qc", "source-branch")
            (root / "source.txt").write_text("branch only\n", encoding="utf-8")
            source = commit(root, "branch source")
        elif source_kind == "orphan":
            tree = git(root, "rev-parse", "HEAD^{tree}")
            source = git(root, "commit-tree", tree, input_text="orphan source\n")
        parent = source
        if wrong_parent:
            git(root, "switch", "-q", "main")
            (root / "unrelated.txt").write_text("later\n", encoding="utf-8")
            parent = commit(root, "wrong parent")
        branch = f"automation/rulesync-{source}"
        git(root, "switch", "-qc", branch, parent)
        (root / "AGENTS.md").write_text(generated_text, encoding="utf-8")
        git(root, "add", "-A")
        git(
            root,
            "commit",
            "-qm",
            "chore: generate agent files",
            "-m",
            f"source-revision: {source}",
            "-m",
            "rulesync-version: 16.26.1",
        )
        head = git(root, "rev-parse", "HEAD")
        git(root, "switch", "-q", "main")
        if later_path:
            path = root / later_path
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text("later\n", encoding="utf-8")
            commit(root, "later main")
        git(root, "remote", "add", "origin", ".")
        details = {
            "baseRefName": "main",
            "body": f"rulesync-generated: true\nsource-revision: {source}\nrulesync-version: 16.26.1\n",
            "headRefName": branch,
            "headRefOid": head,
            "isCrossRepository": False,
            "state": "OPEN",
        }
        return source, head, details

    def verify_generated(
        self,
        root: Path,
        source: str,
        head: str,
        details: dict,
        *,
        generated_text: str = "generated\n",
        validation_error: ValueError | None = None,
    ) -> None:
        original_run = rulesync_reports.run

        def fake_run(command, cwd, check=True):
            if command[:3] == ["gh", "pr", "view"]:
                return subprocess.CompletedProcess(command, 0, json.dumps(details), "")
            if command == ["rulesync", "generate"]:
                (cwd / "AGENTS.md").write_text(generated_text, encoding="utf-8")
                return subprocess.CompletedProcess(command, 0, "", "")
            return original_run(command, cwd, check)

        with patch("Tools.AgentHarness.rulesync_reports.run", side_effect=fake_run), patch(
            "Tools.AgentHarness.rulesync_reports.validate_write_set", side_effect=validation_error
        ):
            generated_pr_guard(root, "1", source, head)

    def test_revision_inventory_keeps_historical_pin(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)

            def extract(_repo, _revision, destination):
                (destination / ".rulesync").mkdir()
                (destination / ".rulesync" / "VERSION").write_text("15.0.0\n", encoding="utf-8")
                (destination / "rulesync.jsonc").write_text("{}\n", encoding="utf-8")

            def inspect(source):
                self.assertEqual((source / ".rulesync" / "VERSION").read_text(encoding="utf-8"), "15.0.0\n")
                return ([record("AGENTS.md", "rules")], {}, [])

            def version(command, _cwd, check=True):
                self.assertEqual(command, ["rulesync", "--version"])
                return subprocess.CompletedProcess(command, 0, "15.0.0\n", "")

            with patch("Tools.AgentHarness.rulesync_reports.extract_revision", side_effect=extract), patch(
                "Tools.AgentHarness.rulesync_reports.run", side_effect=version
            ), patch("Tools.AgentHarness.rulesync_reports.inventory", side_effect=inspect):
                self.assertEqual(revision_inventory(root, "base"), [record("AGENTS.md", "rules")])

    def test_different_revision_pins_fail_closed(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            git(root, "init", "-q", "-b", "main")
            git(root, "config", "user.name", "Test")
            git(root, "config", "user.email", "test@example.com")
            (root / ".rulesync").mkdir()
            (root / ".rulesync" / "VERSION").write_text("16.26.1\n", encoding="utf-8")
            (root / "rulesync.jsonc").write_text("{}\n", encoding="utf-8")
            base = commit(root, "base")
            (root / ".rulesync" / "VERSION").write_text("16.27.0\n", encoding="utf-8")
            head = commit(root, "head")
            with self.assertRaisesRegex(ValueError, "dedicated upgrade path"):
                pr_guard(root, base, head)

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
        self.assertNotIn("workflow_dispatch:", guard)
        self.assertIn('BASE="${{ github.event.pull_request.base.sha }}"', guard)
        self.assertIn('HEAD="${{ github.event.pull_request.head.sha }}"', guard)
        self.assertNotIn("agent-rules-generated-pr-check", guard)
        self.assertNotIn("statuses: write", guard)
        self.assertIn('rulesync generate --output-roots "$RUNNER_TEMP/generated"', validate)
        self.assertIn("Build/AgentHarness/rulesync/", validate)
        self.assertNotIn("workflow_dispatch:", generate)
        self.assertIn("rulesync generate", generate)
        self.assertIn("make agent-rules-validate-write-set", generate)
        self.assertIn("git diff --cached --quiet", generate)
        self.assertIn("automation/rulesync-${source_sha}", generate)
        self.assertIn("statuses: write", generate)
        self.assertNotIn("actions: write", generate)
        self.assertIn("agent-rules-generated-pr-check", generate)
        self.assertIn('statuses/${HEAD_SHA}', generate)
        self.assertIn('-f context="RuleSync generated output guard"', generate)
        verify = generate.index("make agent-rules-generated-pr-check")
        status = generate.index('statuses/${HEAD_SHA}')
        merge = generate.index('gh pr merge "$PR_NUMBER"')
        self.assertLess(verify, status)
        self.assertLess(status, merge)
        self.assertIn('gh pr merge "$PR_NUMBER"', generate)
        self.assertNotIn("git push origin HEAD:main", generate)
        self.assertNotIn("create-github-app-token", generate)

    def test_non_main_source_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source, head, details = self.generated_repository(root, source_kind="branch")
            with self.assertRaisesRegex(ValueError, "not an ancestor"):
                self.verify_generated(root, source, head, details)

    def test_source_not_ancestor_of_origin_main_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source, head, details = self.generated_repository(root, source_kind="orphan")
            with self.assertRaisesRegex(ValueError, "not an ancestor"):
                self.verify_generated(root, source, head, details)

    def test_later_rulesync_source_change_is_stale(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source, head, details = self.generated_repository(root, later_path=".rulesync/rules/overview.md")
            with self.assertRaisesRegex(ValueError, "source is stale"):
                self.verify_generated(root, source, head, details)

    def test_later_rulesync_config_change_is_stale(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source, head, details = self.generated_repository(root, later_path="rulesync.jsonc")
            with self.assertRaisesRegex(ValueError, "source is stale"):
                self.verify_generated(root, source, head, details)

    def test_unrelated_later_main_change_keeps_source_current(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source, head, details = self.generated_repository(root, later_path="docs/unrelated.md")
            self.verify_generated(root, source, head, details)

    def test_generated_commit_with_wrong_parent_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source, head, details = self.generated_repository(root, wrong_parent=True)
            with self.assertRaisesRegex(ValueError, "source revision as its only parent"):
                self.verify_generated(root, source, head, details)

    def test_generated_tree_mismatch_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source, head, details = self.generated_repository(root, generated_text="wrong\n")
            with self.assertRaisesRegex(ValueError, "does not exactly match"):
                self.verify_generated(root, source, head, details)

    def test_generated_write_set_escape_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source, head, details = self.generated_repository(root)
            with self.assertRaisesRegex(ValueError, "outside authorized destinations"):
                self.verify_generated(
                    root,
                    source,
                    head,
                    details,
                    validation_error=ValueError("RuleSync generation wrote outside authorized destinations"),
                )


if __name__ == "__main__":
    unittest.main()
