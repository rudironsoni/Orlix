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
        self.assertEqual(text.count("run: make __bazel-apple-ci"), 1)
        self.assertEqual(text.count("make __bazel-apple-ci"), 2)
        self.assertNotIn("bazelisk", text)
        self.assertIn("paths:", text)
        self.assertNotIn("paths-ignore:", text)
        for path in BAZEL_RELEVANT_PATHS:
            self.assertIn(f'- "{path}"', text)
        for path in IOS15_RELEVANT_PATHS:
            self.assertIn(f'- "{path}"', text)

    def test_canonical_workflow_checks_out_and_names_evidence_by_head_sha(self) -> None:
        workflow = (ROOT / ".github/workflows/bazel-ci.yml").read_text(encoding="utf-8")
        checkout = workflow.split("Check out the exact commit", 1)[1].split(
            "Select Xcode", 1
        )[0]
        self.assertIn("ref: ${{ github.event.pull_request.head.sha || github.sha }}", checkout)
        self.assertIn(
            "name: bazel-apple-ci-${{ github.event.pull_request.head.sha || github.sha }}",
            workflow,
        )

    def test_xcode_version_comes_from_file_and_selection_is_verified(self) -> None:
        for name in (
            "bazel-ci.yml",
            "bazel-promote.yml",
            "bazel-benchmark.yml",
            "bazel-nightly.yml",
            "bazel-gc.yml",
            "testflight-beta.yml",
        ):
            text = (ROOT / ".github" / "workflows" / name).read_text(encoding="utf-8")
            self.assertIn("tr -d '[:space:]' < .xcode-version", text, name)
            self.assertIn("xcode-version: ${{ steps.xcode-version.outputs.version }}", text, name)
            self.assertIn(
                'python3 bazel/config/xcode_select.py --repo "$GITHUB_WORKSPACE" '
                '--build 17F113 --developer-dir "$ORLIX_PINNED_DEVELOPER_DIR"',
                text,
                name,
            )
            self.assertNotIn('xcode-version: "26.6"', text, name)
            self.assertNotIn('= "Xcode 26.6"', text, name)
            self.assertNotIn("/Applications/Xcode", text, name)

    def test_canonical_workflow_keeps_promoted_inputs_off_pull_requests(self) -> None:
        import json

        lock = json.loads((ROOT / "artifacts.lock.json").read_text(encoding="utf-8"))
        self.assertIn(lock.get("schema", 1), (1, 2))
        workflow = (ROOT / ".github/workflows/bazel-ci.yml").read_text(encoding="utf-8")
        select = workflow.split("Select the component mode", 1)[1].split(
            "Prepare promoted-mode verification inputs", 1
        )[0]
        self.assertIn("json.load(open('artifacts.lock.json')).get('schema', 1)", select)
        self.assertIn('GITHUB_EVENT_NAME" = "pull_request"', select)
        self.assertIn("bazel/migration/select_component_mode.py", select)
        self.assertIn("--changed-paths", select)
        self.assertIn("github.event.pull_request.changed_files", select)
        self.assertIn("github.event.before", select)
        self.assertIn("github.event.after", select)
        self.assertIn("git diff --name-only", select)
        self.assertNotIn("return PROMOTED_MODE if", (ROOT / "bazel/migration/component_selection.py").read_text())
        self.assertIn("selecting source mode conservatively", select)
        self.assertIn("Component mode selection", select)
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

    def test_toolchain_manifest_precedes_simulator_preparation(self) -> None:
        workflow = (ROOT / ".github/workflows/bazel-ci.yml").read_text(encoding="utf-8")
        manifest_at = workflow.index("Capture the toolchain manifest before simulator preparation")
        runtimes_at = workflow.index("Prepare both simulator runtimes")
        apple_ci_at = workflow.index("Run the Make-owned Apple CI operation")
        self.assertLess(manifest_at, runtimes_at)
        self.assertLess(runtimes_at, apple_ci_at)
        self.assertIn("make __bazel-toolchain-manifest", workflow)
        self.assertIn(
            'echo "ORLIX_BAZEL_RUN_ID=${{ github.run_id }}-${{ github.run_attempt }}" >> "$GITHUB_ENV"',
            workflow,
        )
        promote = (ROOT / ".github/workflows/bazel-promote.yml").read_text(encoding="utf-8")
        self.assertIn(
            'echo "ORLIX_BAZEL_RUN_ID=${{ github.run_id }}-${{ github.run_attempt }}" >> "$GITHUB_ENV"',
            promote,
        )
        self.assertIn("__bazel-promote-buildset", promote)

    def test_ios_builder_workflows_keep_cli_contract(self) -> None:
        build = (ROOT / ".github/workflows/ios-build.yml").read_text(encoding="utf-8")
        share = (ROOT / ".github/workflows/ios-share.yml").read_text(encoding="utf-8")
        for name in (
            "build_id", "snapshot_ref", "ios_path", "scheme", "use_signing",
            "configuration", "flutter_version", "jdk_version", "profile", "build_number",
        ):
            self.assertIn(f"{name}:", build)
        for name in ("build_id", "snapshot_ref", "ios_path", "scheme", "duration"):
            self.assertIn(f"{name}:", share)
        for text, name in ((build, "ios-build.yml"), (share, "ios-share.yml")):
            self.assertIn("SNAPSHOT_REF", text, name)
            self.assertIn("push:\n    tags:", text, name)
            self.assertIn("Delete trigger tag", text, name)
        self.assertIn('name: ipa', build)
        self.assertIn("build/*.ipa", build)
        self.assertIn("mobai-ci share", share)
        self.assertIn("--device", share)
        self.assertIn("ios-build/*", build)
        self.assertIn("ios-share/*", share)

    def test_ios_builder_orlix_routing(self) -> None:
        build = (ROOT / ".github/workflows/ios-build.yml").read_text(encoding="utf-8")
        share = (ROOT / ".github/workflows/ios-share.yml").read_text(encoding="utf-8")
        for text, name in ((build, "ios-build.yml"), (share, "ios-share.yml")):
            self.assertIn("ORLIX-ADAPTED", text, name)
            self.assertIn("builder init", text, name)
            self.assertIn('type=orlix', text, name)
            self.assertIn("make/bazel-migration.mk", text, name)
            self.assertIn("make __bazel-toolchain-manifest", text, name)
            self.assertIn("ORLIX_BAZEL_COMPONENT_MODE=$mode", text, name)
            self.assertIn("steps.xcode-version.outputs.version", text, name)
            self.assertIn("ORLIX_BAZEL_RUN_ID=${{ github.run_id }}-${{ github.run_attempt }}", text, name)
        self.assertIn("make __builder-package-ipa", build)
        self.assertIn("make __bazel-product-app", share)
        self.assertIn("signed Orlix builds are produced by the Orlix-owned release flow", build)
        self.assertIn("Apple Development codesigning identity", build)
        self.assertIn("team development provisioning profile", build)
        self.assertIn('elif [ "$PROJECT_TYPE" = "orlix" ]', build)
        block = build.split('elif [ "$PROJECT_TYPE" = "orlix" ]', 1)[1]
        end = min(
            (block.index(line) for line in ("\n          elif ", "\n          else", "\n          fi") if line in block),
            default=len(block),
        )
        self.assertNotIn("xcodebuild", block[:end])
        share_block = share.split('if [ "$PROJECT_TYPE" = "orlix" ]', 1)[1]
        share_end = min(
            (share_block.index(line) for line in ("\n          elif ", "\n          else") if line in share_block),
            default=len(share_block),
        )
        self.assertNotIn("xcodebuild", share_block[:share_end])

    def test_ios_builder_security_posture(self) -> None:
        import re

        for name in ("ios-build.yml", "ios-share.yml"):
            text = (ROOT / ".github/workflows" / name).read_text(encoding="utf-8")
            for used in re.findall(r"(?m)^\s+uses:\s*(\S+)", text):
                self.assertRegex(used, r"@[0-9a-f]{40}(?:\s*#|$)", f"{name}: {used}")
                self.assertNotRegex(used, r"@v\d+\s*(?:#|$)", f"{name}: {used}")
            self.assertNotIn("pull_request:", text, name)
            self.assertIn("persist-credentials: false", text, name)
            self.assertIn("contents: write", text, name)
            self.assertIn('fetch --depth=2 origin "+$SNAPSHOT_REF', text, name)
            self.assertIn("gh auth git-credential", text, name)
            self.assertIn('gh api -X DELETE "repos/${{ github.repository }}/git/refs/tags/${{ github.ref_name }}"', text, name)
            self.assertNotIn('push origin ":refs/tags/', text, name)

    def test_manual_promoted_proof_is_explicit_and_guarded(self) -> None:
        workflow = (ROOT / ".github/workflows/bazel-ci.yml").read_text(encoding="utf-8")
        self.assertIn("component_mode:", workflow)
        for option in ("auto", "source", "promoted"):
            self.assertIn(f"- {option}", workflow)
        self.assertIn("default: auto", workflow)
        # Promoted is only reachable through an explicit manual request, and the
        # normal Apple CI step is skipped in that case.
        self.assertIn("REQUESTED_COMPONENT_MODE", workflow)
        self.assertIn("manual promoted mode rejected", workflow)
        self.assertIn("lock schema is not 2", workflow)
        self.assertIn("artifact-identity-v2", workflow)
        self.assertIn("no immutable sha256 OCI digest", workflow)
        self.assertIn("manual promoted mode rejected", workflow)
        self.assertIn("if: github.event_name != 'workflow_dispatch' || inputs.component_mode != 'promoted'", workflow)
        # Cold then warm proof in one job with acquisition and action evidence.
        self.assertIn("Promoted-consumer proof (cold then warm)", workflow)
        self.assertIn("for pass in cold warm", workflow)
        self.assertIn("network_downloads", workflow)
        self.assertIn("local_store_hits", workflow)
        self.assertIn("promoted_execution.py", workflow)
        self.assertIn("--requested-mode promoted", workflow)
        self.assertIn("orlix-promoted-store", workflow)
        self.assertIn("Build/Bazel/output-base", workflow)

    def test_canonical_workflow_reuses_one_product_for_both_runtimes(self) -> None:
        workflow = (ROOT / ".github/workflows/bazel-ci.yml").read_text(encoding="utf-8")
        makefile = (ROOT / "make/bazel-migration.mk").read_text(encoding="utf-8")
        apple_ci = makefile.split("__bazel-apple-ci:", 1)[1].split(
            "__bazel-simulator-runtime-proof:", 1
        )[0]
        ios15 = makefile.split("__bazel-ios15-simulator-gate:", 1)[1].split(
            "__bazel-xctest-runtime-proof:", 1
        )[0]
        self.assertEqual(apple_ci.count("__bazel-orlix-app"), 1)
        self.assertIn('ORLIX_BAZEL_APP_TARGETS="//Orlix:Orlix //Orlix:OrlixUITests"', apple_ci)
        self.assertIn("__bazel-current-simulator-gate", apple_ci)
        self.assertIn("__bazel-ios15-simulator-gate", apple_ci)
        self.assertIn("__bazel-xctest-runtime-proof", apple_ci)
        self.assertIn("AppLaunchSmokeUITests/testLaunchCapturesScreenshot", makefile)
        self.assertIn("ORLIX_CANONICAL_APP_PATH", apple_ci)
        self.assertIn("xctest-app-identity.txt", apple_ci)
        self.assertIn("xctest_app_path", apple_ci)
        self.assertIn("xctest_current_runtime", apple_ci)
        self.assertIn('ORLIX_BAZEL_APP_PATH="$$app"', apple_ci)
        self.assertNotIn('"$(ORLIX_BAZEL)"', ios15)
        self.assertNotIn("__ios15-simulator-build", workflow)
        self.assertIn("if: always()", workflow)

    def test_canonical_workflow_has_stable_concurrency_and_download_only_pr_caches(self) -> None:
        text = (ROOT / ".github/workflows/bazel-ci.yml").read_text(encoding="utf-8")
        self.assertIn("bazel-ci-main", text)
        self.assertIn("bazel-ci-pr-{0}", text)
        self.assertIn("cancel-in-progress: true", text)
        self.assertIn("actions/cache/restore@55cc8345863c7cc4c66a329aec7e433d2d1c52a9", text)
        self.assertIn("actions/cache/save@55cc8345863c7cc4c66a329aec7e433d2d1c52a9", text)
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
            "--remote_instance_name=orlix/apple/bazel-9.2.0/xcode-$(ORLIX_XCODE_BUILD)/$(ORLIX_BAZEL_CACHE_EPOCH)",
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
        self.assertIn("github.event_name == 'workflow_dispatch'", guard)
        self.assertIn("github.repository == 'rudironsoni/Orlix'", guard)
        self.assertNotIn("refs/heads/", guard)
        self.assertNotIn("id-token: write", text)
        self.assertNotIn("attestations: write", text)
        self.assertIn("Cancel if derailed", text)
        self.assertIn("gh run cancel", text)
        self.assertIn("packages: write", text)
        self.assertIn("signed-buildset-${{ github.sha }}", text)
        self.assertIn("buildset-lock-proposal.json", text)
        self.assertIn("toolchain-manifest.json", text)
        self.assertIn("promotion-proof-index.json", text)
        self.assertIn("-sbom.json", text)
        self.assertIn("-in-toto.json", text)
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
