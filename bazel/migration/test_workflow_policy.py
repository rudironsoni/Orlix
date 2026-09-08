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
        self.assertIn("make __bazel-publish-${{ inputs.component }}", text)
        self.assertIn("ORLIX_COSIGN_KEY is not set; leaving the unsigned dual-build unpublished", text)
        self.assertIn("Cancel if derailed", text)
        self.assertIn("gh run cancel", text)
        self.assertIn("packages: write", text)

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
        self.assertIn("timeout-minutes: 90", text)
        self.assertNotIn("timeout-minutes: 180", text)
        self.assertIn('ORLIX_PINNED_DEVELOPER_DIR=$(xcode-select -p)', text)
        self.assertIn("actions/cache@0057852bfaa89a56745cba8c7296529d2fc39830", text)
        self.assertIn("orlix-ios15-runtime-15.5-arm64-v1", text)
        self.assertIn("orlix-git-clones-", text)
        self.assertIn("~/Library/Caches/Orlix/git", text)
        self.assertIn("Build/OrlixKernel/upstream", text)
        self.assertIn("Build/OrlixMLibC/upstream", text)
        self.assertNotIn("vendor=all", text)
        self.assertIn("orlix-bazel-disk-", text)
        self.assertIn("orlix-ccache-", text)
        self.assertIn("make __tcti-isa-restore", text)
        self.assertIn("orlix-tcti-isa-", text)
        self.assertIn("OrlixKernel/Sources/ports/orlix/isa/prepared-tables.sha256", text)
        self.assertNotIn("AARCHMRS", text)
        self.assertNotIn("prepare type=tcti-isa", text)

    def test_vendor_ghostty_uses_git_cache(self) -> None:
        text = (ROOT / "Orlix/make/vendor.mk").read_text(encoding="utf-8")
        self.assertIn("ORLIX_GIT_CACHE", text)
        self.assertIn("ghostty.git", text)
        self.assertIn("cat-file -e", text)

    def test_promote_workflow_does_not_use_action_cache(self) -> None:
        text = (ROOT / ".github/workflows/bazel-promote.yml").read_text(encoding="utf-8")
        self.assertIn("Dual-build without action cache", text)
        self.assertNotIn("actions/cache@", text)


if __name__ == "__main__":
    unittest.main()
