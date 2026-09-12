from __future__ import annotations

from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]


class WorkflowPolicyTests(unittest.TestCase):
    def test_pr_workflow_calls_make(self) -> None:
        text = (ROOT / ".github/workflows/bazel-pr.yml").read_text(encoding="utf-8")
        self.assertIn("make __bazel-matrix-check", text)
        self.assertNotIn("bazelisk", text)
        self.assertNotIn("paths:", text)
        self.assertNotIn("paths-ignore:", text)

    def test_promote_workflow_is_dispatch_and_protected(self) -> None:
        text = (ROOT / ".github/workflows/bazel-promote.yml").read_text(encoding="utf-8")
        self.assertIn("workflow_dispatch", text)
        self.assertIn("environment: bazel-promotion", text)
        self.assertIn("make __bazel-promote-${{ inputs.component }}", text)
        self.assertIn("make __bazel-publish-${{ inputs.component }}", text)
        missing_key = text.split('if [ -z "${ORLIX_COSIGN_KEY}" ]; then', 1)[1].split("fi", 1)[0]
        self.assertIn("exit 1", missing_key)
        self.assertNotIn("exit 0", missing_key)
        self.assertIn("github.ref == 'refs/heads/main'", text)
        self.assertIn("Cancel if derailed", text)
        self.assertIn("gh run cancel", text)
        self.assertIn("packages: write", text)
        self.assertIn("signed-${{ inputs.component }}-${{ github.sha }}", text)
        self.assertIn("${{ inputs.component }}-signed.json", text)
        self.assertIn('--registry-config "$ORLIX_ORAS_REGISTRY_CONFIG"', text)
        for component in (
            "kernel-release-iphoneos",
            "kernel-release-iphonesimulator",
            "kernel-development-iphoneos",
            "kernel-development-iphonesimulator",
        ):
            self.assertIn(f"          - {component}", text)

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
        self.assertNotIn("paths:", text)
        self.assertNotIn("paths-ignore:", text)

    def test_nightly_workflow_calls_make(self) -> None:
        text = (ROOT / ".github/workflows/bazel-nightly.yml").read_text(encoding="utf-8")
        self.assertIn("make __bazel-matrix-check", text)
        self.assertIn("make __bazel-gc", text)
        self.assertIn("make __bazel-reconstruct-source", text)

    def test_gc_workflow_calls_make(self) -> None:
        text = (ROOT / ".github/workflows/bazel-gc.yml").read_text(encoding="utf-8")
        self.assertIn("make __bazel-gc", text)

    def test_benchmark_keeps_measurements_and_comparison_in_make(self) -> None:
        text = (ROOT / ".github/workflows/bazel-benchmark.yml").read_text(encoding="utf-8")
        self.assertIn("make __bazel-cache-equivalence", text)
        self.assertIn("set -euo pipefail", text)
        self.assertIn("github.ref == 'refs/heads/main'", text)
        self.assertIn("contents: read", text)
        self.assertNotIn("packages: write", text)
        self.assertNotIn("continue-on-error", text)
        self.assertNotIn("actions/cache@", text)
        for artifact in ("build-events.json", "profile.json.gz", "comparison.json", "execution.json"):
            self.assertIn(artifact, text)
        self.assertIn("if: always()", text)
        self.assertIn("retention-days: 14", text)

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
        self.assertNotIn("make __tcti-isa-restore", text)
        self.assertNotIn("orlix-tcti-isa-", text)
        self.assertNotIn("Build/OrlixKernel/orlix-tcti-isa", text)
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
        self.assertIn('export ORLIX_COSIGN_KEY="file://${key_path}"', text)
        self.assertIn("ORLIX_COSIGN_KEY_PASSWORD", text)
        self.assertIn("oras login ghcr.io", text)
        self.assertIn("COSIGN_PASSWORD", text)

    def test_lock_proposal_workflow_does_not_write_main(self) -> None:
        text = (ROOT / ".github/workflows/bazel-lock-proposal.yml").read_text(encoding="utf-8")
        self.assertIn("workflow_dispatch", text)
        self.assertIn("environment: bazel-promotion", text)
        self.assertIn("make __bazel-lock-proposal", text)
        self.assertIn(".headSha == $sha", text)
        self.assertIn('.conclusion == "success"', text)
        components = {
            "uapi": "uapi",
            "mlibc": "mlibc",
            "rootfs": "rootfs",
            "kernel-release-iphoneos": "kernel_release_iphoneos",
            "kernel-release-iphonesimulator": "kernel_release_iphonesimulator",
            "kernel-development-iphoneos": "kernel_development_iphoneos",
            "kernel-development-iphonesimulator": "kernel_development_iphonesimulator",
        }
        for component, input_name in components.items():
            self.assertIn(f"      {input_name}_run_id:\n", text)
            self.assertIn(f"run-id: ${{{{ inputs.{input_name}_run_id }}}}", text)
            self.assertIn(f"signed-{component}-${{{{ github.sha }}}}", text)
        self.assertIn("Cancel if derailed", text)
        self.assertNotIn("git commit", text)
        self.assertNotIn("git push", text)
        self.assertNotIn("--apply-lock", text)


if __name__ == "__main__":
    unittest.main()
