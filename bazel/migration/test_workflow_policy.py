from __future__ import annotations

from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]


class WorkflowPolicyTests(unittest.TestCase):
    def test_pr_workflow_calls_make(self) -> None:
        text = (ROOT / ".github/workflows/bazel-pr.yml").read_text(encoding="utf-8")
        self.assertIn("make __bazel-matrix-check", text)
        self.assertNotIn("bazelisk", text)

    def test_promote_workflow_is_dispatch_and_protected(self) -> None:
        text = (ROOT / ".github/workflows/bazel-promote.yml").read_text(encoding="utf-8")
        self.assertIn("workflow_dispatch", text)
        self.assertIn("environment: bazel-promotion", text)
        self.assertIn("make __bazel-promote-${{ inputs.component }}", text)

    def test_trust_policy_forbids_unsigned_main_lock_writes(self) -> None:
        import json

        policy = json.loads((ROOT / "bazel/promotion/trust-policy.json").read_text(encoding="utf-8"))
        self.assertIs(policy["unsigned_lock_writes_to_main"], False)
        self.assertIs(policy["mutable_latest_tag"], False)

    def test_main_workflow_calls_make(self) -> None:
        text = (ROOT / ".github/workflows/bazel-main.yml").read_text(encoding="utf-8")
        self.assertIn("make __bazel-matrix-check", text)
        self.assertIn("branches: [main]", text)
        self.assertNotIn("bazelisk", text)

    def test_nightly_workflow_calls_make(self) -> None:
        text = (ROOT / ".github/workflows/bazel-nightly.yml").read_text(encoding="utf-8")
        self.assertIn("make __bazel-matrix-check", text)
        self.assertIn("make __bazel-gc", text)

    def test_gc_workflow_calls_make(self) -> None:
        text = (ROOT / ".github/workflows/bazel-gc.yml").read_text(encoding="utf-8")
        self.assertIn("make __bazel-gc", text)

    def test_ios15_runtime_cancels_if_derailed(self) -> None:
        text = (ROOT / ".github/workflows/ios15-runtime.yml").read_text(encoding="utf-8")
        self.assertIn("Cancel if derailed", text)
        self.assertIn("gh run cancel", text)
        self.assertIn("timeout-minutes: 75", text)
        self.assertNotIn("timeout-minutes: 180", text)


if __name__ == "__main__":
    unittest.main()
