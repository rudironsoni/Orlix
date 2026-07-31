#!/usr/bin/env python3

import importlib.util
import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch


ROOT = Path(__file__).resolve().parents[3]
COMMON_PATH = ROOT / ".codex" / "hooks" / "orlix_hook_common.py"
SPEC = importlib.util.spec_from_file_location("orlix_hook_common", COMMON_PATH)
assert SPEC and SPEC.loader
COMMON = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(COMMON)


class OntologyLifecycleGuardTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        (self.root / "docs").mkdir(parents=True)
        (self.root / "AGENTS.md").write_text("# Rules\n")
        (self.root / "docs" / "index.md").write_text("# Index\n")
        self.initialize_repository(self.root)

    def tearDown(self):
        self.temp.cleanup()

    def git(self, *args: str):
        return subprocess.run(
            ["git", *args],
            cwd=self.root,
            check=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )

    def initialize_repository(self, root: Path, include_agents: bool = True):
        subprocess.run(["git", "init", "-q"], cwd=root, check=True)
        subprocess.run(["git", "config", "user.name", "Hook Test"], cwd=root, check=True)
        subprocess.run(["git", "config", "user.email", "hook-test@example.invalid"], cwd=root, check=True)
        paths = ["docs/index.md"]
        if include_agents:
            paths.append("AGENTS.md")
        subprocess.run(["git", "add", *paths], cwd=root, check=True)
        subprocess.run(["git", "commit", "-qm", "test baseline"], cwd=root, check=True)

    def work_page(self, kind: str, status: str, name: str) -> Path:
        path = self.root / "docs" / "objects" / kind / status / f"{name}.md"
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(
            f"---\ntype: {kind}\ntags:\n  - test\nupdated: 2026-07-15\nstatus: {status}\n---\n\n# {name}\n"
        )
        return path

    def test_doing_work_hierarchy_is_required_context(self):
        epic = self.work_page("epic", "doing", "epic")
        story = self.work_page("story", "doing", "story")
        task = self.work_page("task", "doing", "task")
        self.work_page("task", "todo", "future-task")
        self.work_page("task", "done", "finished-task")
        self.assertEqual(COMMON.doing_work_pages(self.root), [epic, story, task])
        self.assertEqual(
            COMMON.required_plan_context_paths(self.root),
            [self.root / "AGENTS.md", self.root / "docs" / "index.md", epic, story, task],
        )

    def test_context_requires_every_doing_work_page(self):
        task = self.work_page("task", "doing", "task")
        state = {
            "read_paths": [
                str((self.root / "AGENTS.md").resolve()),
                str((self.root / "docs" / "index.md").resolve()),
            ]
        }
        self.assertFalse(COMMON.plan_context_loaded(self.root, state))
        state["read_paths"].append(str(task.resolve()))
        self.assertTrue(COMMON.plan_context_loaded(self.root, state))

    def test_stale_knowledge_timestamps_do_not_block_a_clean_tree(self):
        state = {"knowledge_mutation_time": 10, "log_update_time": 9, "index_update_time": 11}
        self.assertTrue(COMMON.knowledge_updates_current(self.root, state))

    def test_dirty_knowledge_requires_current_log_and_index_updates(self):
        knowledge = self.work_page("task", "doing", "task")
        state = {"knowledge_mutation_time": 10, "log_update_time": 9, "index_update_time": 11}
        self.assertTrue(COMMON.knowledge_paths_dirty(self.root))
        self.assertFalse(COMMON.knowledge_updates_current(self.root, state))
        state["log_update_time"] = 12
        self.assertTrue(COMMON.knowledge_updates_current(self.root, state))
        self.git("add", str(knowledge.relative_to(self.root)))
        self.assertTrue(COMMON.knowledge_paths_dirty(self.root))
        knowledge.write_text(knowledge.read_text() + "Tracked edit.\n")
        self.assertTrue(COMMON.knowledge_paths_dirty(self.root))

    def test_every_guarded_knowledge_path_blocks_for_each_dirty_mode(self):
        stale_state = {"knowledge_mutation_time": 10, "log_update_time": 9, "index_update_time": 11}
        for knowledge_path in COMMON.KNOWLEDGE_PATHS:
            for mode in ("staged", "tracked_unstaged", "untracked"):
                with self.subTest(knowledge_path=knowledge_path, mode=mode), tempfile.TemporaryDirectory() as temp:
                    root = Path(temp)
                    (root / "docs").mkdir(parents=True)
                    (root / "AGENTS.md").write_text("# Rules\n")
                    (root / "docs" / "index.md").write_text("# Index\n")
                    self.initialize_repository(
                        root,
                        include_agents=knowledge_path != "AGENTS.md" or mode != "untracked",
                    )
                    path = root / knowledge_path
                    if path.suffix:
                        path.parent.mkdir(parents=True, exist_ok=True)
                    else:
                        path.mkdir(parents=True, exist_ok=True)
                        path = path / "knowledge.md"
                    path.write_text("Initial knowledge.\n")
                    if mode != "untracked":
                        subprocess.run(["git", "add", str(path.relative_to(root))], cwd=root, check=True)
                        subprocess.run(["git", "commit", "-qm", "track knowledge"], cwd=root, check=True)
                        path.write_text("Changed knowledge.\n")
                        if mode == "staged":
                            subprocess.run(["git", "add", str(path.relative_to(root))], cwd=root, check=True)
                    self.assertTrue(COMMON.knowledge_paths_dirty(root))
                    self.assertFalse(COMMON.knowledge_updates_current(root, stale_state))

    def test_knowledge_status_failure_blocks_even_with_current_timestamps(self):
        state = {"knowledge_mutation_time": 10, "log_update_time": 10, "index_update_time": 10}
        with patch.object(COMMON.subprocess, "run", return_value=subprocess.CompletedProcess([], 128)):
            self.assertIsNone(COMMON.knowledge_paths_dirty(self.root))
            self.assertFalse(COMMON.knowledge_updates_current(self.root, state))

    def test_code_mutation_has_no_implementation_journal_requirement(self):
        state = {"knowledge_mutation_time": 0, "log_update_time": 0, "index_update_time": 0}
        self.assertTrue(COMMON.knowledge_updates_current(self.root, state))

    def test_semantic_warnings_remain_available(self):
        self.assertTrue(COMMON.vague_container_support("Docker support is complete"))
        self.assertFalse(COMMON.vague_container_support("OCI-derived Docker compatibility"))
        self.assertTrue(COMMON.workflow_without_authorization("spawn parallel agents"))
        self.assertFalse(COMMON.workflow_without_authorization("user explicitly requested parallel agents"))


if __name__ == "__main__":
    unittest.main()
