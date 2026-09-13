from __future__ import annotations

import subprocess
import tempfile
import unittest
from pathlib import Path

from Tools.AgentHarness.context import Ontology
from Tools.AgentHarness.task_envelope import compile_envelope, validate_envelope


PAGE = """---
type: {kind}
tags:
  - test
updated: 2026-09-13
status: {status}
{links}
---
# Test
"""


class ContextEnvelopeTests(unittest.TestCase):
    def fixture(self, root: Path) -> None:
        subprocess.run(["git", "init", "-q", str(root)], check=True)
        subprocess.run(
            ["git", "-C", str(root), "-c", "user.name=Test", "-c", "user.email=test@example.com", "commit", "--allow-empty", "-m", "test"],
            check=True,
            capture_output=True,
        )
        for path, content in {
            ".rulesync/rules/overview.md": "rules\n",
            "docs/index.md": "index\n",
            "docs/ontology.md": "ontology\n",
            "docs/objects/epic/doing/selected.md": PAGE.format(kind="epic", status="doing", links='targets:\n  - "[Component](../../software-component/component.md)"'),
            "docs/objects/story/doing/selected.md": PAGE.format(kind="story", status="doing", links='story_of:\n  - "[Epic](../../epic/doing/selected.md)"'),
            "docs/objects/task/doing/selected.md": PAGE.format(kind="task", status="doing", links='external_id: "github:rudironsoni/Orlix#232"\ntask_of:\n  - "[Story](../../story/doing/selected.md)"\napplies:\n  - "[Concept](../../../concepts/concept.md)"\nrelates_to:\n  - "[ADR](../../architecture-decision/accepted.md)"\nowned_paths:\n  - "src/**"\nread_only_paths:\n  - "vendor/**"\nforbidden_paths:\n  - "generated/**"\nrequired_skills:\n  - "orlix-test"\nrequired_role: "orlix-implementer"\nrequired_proof:\n  - "proof"\nbuild_intents:\n  - "build"\nverification_intents:\n  - "verify"'),
            "docs/objects/task/doing/unrelated.md": PAGE.format(kind="task", status="doing", links='task_of:\n  - "[Story](../../story/doing/selected.md)"'),
            "docs/objects/software-component/component.md": PAGE.format(kind="software-component", status="active", links=""),
            "docs/objects/architecture-decision/accepted.md": PAGE.format(kind="architecture-decision", status="accepted", links=""),
            "docs/concepts/concept.md": PAGE.format(kind="concept", status="active", links=""),
        }.items():
            target = root / path
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text(content, encoding="utf-8")

    def test_selected_typed_closure_and_envelope(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.fixture(root)
            closure = Ontology(root).resolve("github:rudironsoni/Orlix#232")
            self.assertIn("docs/objects/architecture-decision/accepted.md", closure["context_paths"])
            self.assertIn("docs/objects/software-component/component.md", closure["context_paths"])
            self.assertNotIn("docs/objects/task/doing/unrelated.md", closure["context_paths"])
            envelope = compile_envelope(root, "selected")
            self.assertEqual(envelope["owned_paths"], ["src/**"])
            self.assertNotIn("allowed_mcp_servers", envelope)

    def test_validation_rejects_native_client_configuration(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.fixture(root)
            envelope = compile_envelope(root, "selected")
            envelope["sandbox_mode"] = "read-only"
            with self.assertRaisesRegex(ValueError, "native client configuration"):
                validate_envelope(root, envelope)


if __name__ == "__main__":
    unittest.main()
