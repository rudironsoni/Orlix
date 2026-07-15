#!/usr/bin/env python3

import importlib.util
import tempfile
import unittest
from pathlib import Path


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

    def tearDown(self):
        self.temp.cleanup()

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

    def test_knowledge_commit_requires_log_and_index_updates(self):
        state = {"knowledge_mutation_time": 10, "log_update_time": 9, "index_update_time": 11}
        self.assertFalse(COMMON.knowledge_updates_current(state))
        state["log_update_time"] = 12
        self.assertTrue(COMMON.knowledge_updates_current(state))

    def test_code_mutation_has_no_implementation_journal_requirement(self):
        state = {"knowledge_mutation_time": 0, "log_update_time": 0, "index_update_time": 0}
        self.assertTrue(COMMON.knowledge_updates_current(state))

    def test_semantic_warnings_remain_available(self):
        self.assertTrue(COMMON.vague_container_support("Docker support is complete"))
        self.assertFalse(COMMON.vague_container_support("OCI-derived Docker compatibility"))
        self.assertTrue(COMMON.workflow_without_authorization("spawn parallel agents"))
        self.assertFalse(COMMON.workflow_without_authorization("user explicitly requested parallel agents"))


if __name__ == "__main__":
    unittest.main()
