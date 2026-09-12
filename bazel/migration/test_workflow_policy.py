from __future__ import annotations

from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]


class WorkflowPolicyTests(unittest.TestCase):
    def test_one_canonical_pr_and_main_workflow_calls_make(self) -> None:
        workflow = ROOT / ".github/workflows/bazel-ci.yml"
        self.assertTrue(workflow.is_file())
        self.assertFalse((ROOT / ".github/workflows/bazel-pr.yml").exists())
        self.assertFalse((ROOT / ".github/workflows/bazel-main.yml").exists())
        self.assertFalse((ROOT / ".github/workflows/ios15-runtime.yml").exists())
        text = workflow.read_text(encoding="utf-8")
        self.assertIn("pull_request:", text)
        self.assertIn("branches: [main]", text)
        self.assertIn("name: Bazel matrix check", text)
        self.assertEqual(text.count("make __bazel-apple-ci"), 1)
        self.assertNotIn("bazelisk", text)
        self.assertNotIn("paths:", text)
        self.assertNotIn("paths-ignore:", text)

    def test_canonical_workflow_reuses_one_product_for_both_runtimes(self) -> None:
        workflow = (ROOT / ".github/workflows/bazel-ci.yml").read_text(encoding="utf-8")
        makefile = (ROOT / "make/bazel-migration.mk").read_text(encoding="utf-8")
        apple_ci = makefile.split("__bazel-apple-ci:", 1)[1].split(
            "__bazel-simulator-runtime-proof:", 1
        )[0]
        ios15 = makefile.split("__bazel-ios15-simulator-gate:", 1)[1].split(
            "__bazel-hostadapter:", 1
        )[0]
        self.assertEqual(apple_ci.count("__bazel-orlix-app"), 1)
        self.assertIn('ORLIX_BAZEL_APP_TARGETS="//Orlix:Orlix //Orlix:OrlixUITests"', apple_ci)
        self.assertIn("__bazel-current-simulator-gate", apple_ci)
        self.assertIn("__bazel-ios15-simulator-gate", apple_ci)
        self.assertIn('ORLIX_BAZEL_APP_PATH="$$app"', apple_ci)
        self.assertNotIn('"$(ORLIX_BAZEL)"', ios15)
        self.assertNotIn("__ios15-simulator-build", workflow)
        self.assertIn("if: always()", workflow)

    def test_canonical_workflow_has_stable_concurrency_and_download_only_pr_caches(self) -> None:
        text = (ROOT / ".github/workflows/bazel-ci.yml").read_text(encoding="utf-8")
        self.assertIn("bazel-ci-main", text)
        self.assertIn("bazel-ci-pr-{0}", text)
        self.assertIn("cancel-in-progress: true", text)
        self.assertIn("actions/cache/restore@0057852bfaa89a56745cba8c7296529d2fc39830", text)
        self.assertIn("actions/cache/save@0057852bfaa89a56745cba8c7296529d2fc39830", text)
        self.assertIn("if: github.event_name == 'push' && github.ref == 'refs/heads/main'", text)
        self.assertNotIn("Bazel/disk-cache", text)
        self.assertNotIn("Orlix/ccache", text)
        self.assertIn(
            "orlix-ios-runtime-15.5-arm64-71fd7d0159a4439ebef1abb2b1e0b26204b74af903dcc579a7b6e131065a1150",
            text,
        )

    def test_buildbuddy_cache_configuration(self) -> None:
        rc = (ROOT / ".bazelrc").read_text(encoding="utf-8")
        makefile = (ROOT / "make/bazel-migration.mk").read_text(encoding="utf-8")
        self.assertIn("--remote_cache=grpcs://remote.buildbuddy.io", rc)
        self.assertIn("--remote_cache_compression", rc)
        self.assertIn("--experimental_remote_cache_chunking", rc)
        self.assertIn("--remote_download_outputs=minimal", rc)
        self.assertIn("--bes_results_url=https://app.buildbuddy.io/invocation/", rc)
        self.assertIn("--bes_backend=grpcs://remote.buildbuddy.io", rc)
        self.assertIn("--remote_upload_local_results=false", rc)
        self.assertNotIn("--remote_executor", rc)
        self.assertIn("ORLIX_BAZEL_CACHE_EPOCH ?= v1", makefile)
        self.assertIn(
            "--remote_instance_name=orlix/apple/bazel-9.2.0/xcode-17F113/v1", rc
        )
        instance = next(line for line in rc.splitlines() if "--remote_instance_name" in line)
        for forbidden in ("github.sha", "github.ref", "pull_request", "15.5", "26.5"):
            self.assertNotIn(forbidden, instance)

    def test_buildbuddy_credentials_are_ephemeral_and_context_bound(self) -> None:
        workflow = (ROOT / ".github/workflows/bazel-ci.yml").read_text(encoding="utf-8")
        makefile = (ROOT / "make/bazel-migration.mk").read_text(encoding="utf-8")
        self.assertIn("secrets.BUILDBUDDY_API_KEY_WRITE", workflow)
        self.assertIn("secrets.BUILDBUDDY_API_KEY_READ", workflow)
        self.assertIn("head.repo.full_name == github.repository", workflow)
        self.assertIn("head.repo.full_name != github.repository", workflow)
        self.assertIn("ORLIX_BUILDBUDDY_CONTEXT", workflow)
        configure = makefile.split("__bazel-buildbuddy-configure:", 1)[1].split(
            "__bazel-buildbuddy-cleanup:", 1
        )[0]
        cleanup = makefile.split("__bazel-buildbuddy-cleanup:", 1)[1].split(
            "__bazel-apple-smoke:", 1
        )[0]
        self.assertIn("$${RUNNER_TEMP:-}", configure)
        self.assertIn("chmod 600", configure)
        self.assertIn("x-buildbuddy-api-key=$$BUILDBUDDY_API_KEY", configure)
        self.assertIn("unlink", cleanup)
        self.assertIn("if: always()", workflow.split("Remove BuildBuddy credentials", 1)[1])

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
        self.assertIn('cosign public-key --key "$key_path" > "$key_path.pub"', text)
        self.assertIn('export ORLIX_COSIGN_PUB="$key_path.pub"', text)
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
