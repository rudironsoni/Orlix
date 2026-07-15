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
        (self.root / "docs" / "objects" / "initiative").mkdir(parents=True)
        (self.root / "AGENTS.md").write_text("# Rules\n")
        (self.root / "docs" / "index.md").write_text("# Index\n")

    def tearDown(self):
        self.temp.cleanup()

    def initiative(self, name: str, status: str) -> Path:
        path = self.root / "docs" / "objects" / "initiative" / f"{name}.md"
        path.write_text(f"---\ntype: initiative\ntags:\n  - test\nupdated: 2026-07-15\nstatus: {status}\n---\n\n# {name}\n")
        return path

    def test_active_initiatives_are_required_context(self):
        active = self.initiative("active", "active")
        self.initiative("deferred", "deferred")
        self.assertEqual(COMMON.active_plan_dirs(self.root), [active])
        self.assertEqual(
            COMMON.required_plan_context_paths(self.root),
            [self.root / "AGENTS.md", self.root / "docs" / "index.md", active],
        )

    def test_context_requires_every_active_initiative(self):
        active = self.initiative("active", "active")
        state = {"read_paths": [str((self.root / "AGENTS.md").resolve()), str((self.root / "docs" / "index.md").resolve())]}
        self.assertFalse(COMMON.plan_context_loaded(self.root, state))
        state["read_paths"].append(str(active.resolve()))
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
