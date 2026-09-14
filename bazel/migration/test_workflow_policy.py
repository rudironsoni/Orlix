from __future__ import annotations

from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]

BAZEL_RELEVANT_PATHS = (
    ".github/workflows/bazel-*.yml",
    ".github/workflows/ios15-runtime.yml",
    ".bazelignore",
    ".bazelrc",
    ".bazelversion",
    "MODULE.bazel",
    "MODULE.bazel.lock",
    "Brewfile",
    "Makefile",
    "make/bazel-migration.mk",
    "artifacts.lock.json",
    "bazel/**",
    "**/BUILD.bazel",
    "**/*.bzl",
    "Orlix/make/vendor.mk",
    "OrlixKernel/Sources/ports/orlix/**",
    "OrlixMLibC/**",
    "third_party/**",
)

IOS15_RELEVANT_PATHS = (
    ".github/workflows/ios15-runtime.yml",
    "Brewfile",
    "Makefile",
    "Orlix/**",
    "OrlixHostAdapter/**",
    "OrlixKernel/**",
    "OrlixMLibC/**",
    "OrlixOS/**",
    "project.yml",
)


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
        self.assertIn("paths:", text)
        self.assertNotIn("paths-ignore:", text)
        for path in BAZEL_RELEVANT_PATHS:
            self.assertIn(f'- "{path}"', text)
        for path in IOS15_RELEVANT_PATHS:
            self.assertIn(f'- "{path}"', text)

    def test_canonical_workflow_keeps_promoted_inputs_off_pull_requests(self) -> None:
        import json

        lock = json.loads((ROOT / "artifacts.lock.json").read_text(encoding="utf-8"))
        self.assertEqual(lock.get("schema", 1), 1)
        workflow = (ROOT / ".github/workflows/bazel-ci.yml").read_text(encoding="utf-8")
        select = workflow.split("Select the component mode", 1)[1].split(
            "Prepare promoted-mode verification inputs", 1
        )[0]
        self.assertIn('json.load(open("artifacts.lock.json")).get("schema", 1)', select)
        self.assertIn('GITHUB_EVENT_NAME" = "pull_request"', select)
        self.assertIn('$schema" != "2"', select)
        self.assertIn("ORLIX_BAZEL_COMPONENT_MODE: ${{ steps.component-mode.outputs.mode }}", workflow)
        self.assertIn("packages: read", workflow)
        self.assertIn("steps.component-mode.outputs.mode == 'promoted'", workflow)
        self.assertIn("oras login ghcr.io", workflow)
        promoted_block = workflow.split("Prepare promoted-mode verification inputs", 1)[1].split(
            "Run the Make-owned Apple CI operation", 1
        )[0]
        self.assertIn("ORLIX_COSIGN_PUB_VALUE", promoted_block)
        self.assertIn('printf \'%s\\n\' "$ORLIX_COSIGN_PUB_VALUE" > "$key_path"', promoted_block)
        self.assertIn("ORLIX_COSIGN_PUB=$key_path", promoted_block)
        self.assertNotIn("attest-build-provenance", workflow)

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
        self.assertNotIn("--remote_instance_name", rc)
        self.assertIn("ORLIX_BAZEL_CACHE_EPOCH ?= v1", makefile)
        configure = makefile.split("__bazel-buildbuddy-configure:", 1)[1].split(
            "__bazel-buildbuddy-cleanup:", 1
        )[0]
        self.assertIn(
            "--remote_instance_name=orlix/apple/bazel-9.2.0/xcode-17F113/$(ORLIX_BAZEL_CACHE_EPOCH)",
            configure,
        )

    def test_buildbuddy_credentials_are_ephemeral_and_context_bound(self) -> None:
        workflow = (ROOT / ".github/workflows/bazel-ci.yml").read_text(encoding="utf-8")
        makefile = (ROOT / "make/bazel-migration.mk").read_text(encoding="utf-8")
        self.assertIn("secrets.ORLIX_CI_BUILDBUDDY_WRITE_API_KEY", workflow)
        self.assertIn("secrets.ORLIX_CI_BUILDBUDDY_READ_API_KEY", workflow)
        self.assertNotIn("secrets.BUILDBUDDY_API_KEY_WRITE", workflow)
        self.assertNotIn("secrets.BUILDBUDDY_API_KEY_READ", workflow)
        self.assertIn("head.repo.full_name == github.repository", workflow)
        self.assertIn("&& 'pr' || 'fork'", workflow)
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
        self.assertIn("make __bazel-promote-buildset", text)
        self.assertIn('make "__bazel-publish-${component}"', text)
        self.assertIn("make __bazel-lock-proposal", text)
        missing_key = text.split('if [ -z "${ORLIX_COSIGN_KEY}" ]; then', 1)[1].split("fi", 1)[0]
        self.assertIn("exit 1", missing_key)
        self.assertNotIn("exit 0", missing_key)
        self.assertIn("github.event_name == 'workflow_dispatch'", text)
        self.assertIn("github.repository == 'rudironsoni/Orlix'", text)
        guard = text.split("name: Dual-build promote", 1)[1].split("runs-on:", 1)[0]
        for allowed_ref in ("refs/heads/main", "refs/heads/fix/build-optimizations"):
            self.assertIn(allowed_ref, guard)
        self.assertNotIn("id-token: write", text)
        self.assertNotIn("attestations: write", text)
        self.assertIn("Cancel if derailed", text)
        self.assertIn("gh run cancel", text)
        self.assertIn("packages: write", text)
        self.assertIn("signed-buildset-${{ github.sha }}", text)
        self.assertIn("buildset-lock-proposal.json", text)
        self.assertIn('--registry-config "$ORLIX_ORAS_REGISTRY_CONFIG"', text)
        for component in (
            "kernel-release-iphoneos",
            "kernel-release-iphonesimulator",
            "kernel-development-iphoneos",
            "kernel-development-iphonesimulator",
        ):
            self.assertIn(f"Build/Bazel/promote/{component}/{component}-signed.json", text)

    def test_trust_policy_forbids_unsigned_main_lock_writes(self) -> None:
        import json

        policy = json.loads((ROOT / "bazel/promotion/trust-policy.json").read_text(encoding="utf-8"))
        self.assertIs(policy["unsigned_lock_writes_to_main"], False)
        self.assertIs(policy["mutable_latest_tag"], False)
        self.assertEqual(policy["allowed_workflows"], [".github/workflows/bazel-promote.yml"])
        self.assertEqual(policy["required_environment"], "bazel-promotion")
        self.assertIn("refs/heads/fix/build-optimizations", policy["allowed_refs"])

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
        makefile = (ROOT / "make/bazel-migration.mk").read_text(encoding="utf-8")
        self.assertIn("Dual-build without action cache", text)
        self.assertIn("actions/cache/restore@", text)
        self.assertIn("actions/cache/save@", text)
        self.assertIn("Bazel/repository-cache", text)
        buildset = makefile.split("__bazel-promote-buildset:", 1)[1].split(
            "define ORLIX_BAZEL_PUBLISH", 1
        )[0]
        self.assertIn("--nouse_action_cache", buildset)
        self.assertIn("--disk_cache= --repository_cache=", buildset)
        self.assertIn("--remote_cache= --remote_executor=", buildset)
        self.assertIn("--config=promotion", buildset)
        self.assertIn('export ORLIX_COSIGN_KEY="file://${key_path}"', text)
        self.assertIn('cosign public-key --key "$key_path" > "$key_path.pub"', text)
        self.assertIn('export ORLIX_COSIGN_PUB="$key_path.pub"', text)
        self.assertIn("ORLIX_COSIGN_KEY_PASSWORD", text)
        self.assertIn("oras login ghcr.io", text)
        self.assertIn("COSIGN_PASSWORD", text)

    def test_buildset_promotion_has_one_workflow_authority(self) -> None:
        self.assertFalse((ROOT / ".github/workflows/bazel-lock-proposal.yml").exists())
        text = (ROOT / ".github/workflows/bazel-promote.yml").read_text(encoding="utf-8")
        self.assertIn("make __bazel-lock-proposal", text)
        self.assertNotIn("git commit", text)
        self.assertNotIn("git push", text)
        self.assertNotIn("--apply-lock", text)


if __name__ == "__main__":
    unittest.main()
