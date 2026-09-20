#!/usr/bin/env python3
"""Public Make targets must dry-run to the Bazel-owned shadows."""

from __future__ import annotations

import os
import re
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
MAKE = "gmake"
PATH = os.environ.get("PATH", "/usr/bin:/bin")


def _dry_run(*args: str) -> str:
    env = os.environ.copy()
    env["PATH"] = PATH
    completed = subprocess.run(
        [MAKE, "-C", str(ROOT), "-n", "MAKE=echo", "ORLIX_BAZEL_AUTHORITY=1", *args],
        check=False,
        capture_output=True,
        timeout=30,
        text=True,
        env=env,
    )
    output = completed.stdout + completed.stderr
    if completed.returncode != 0:
        raise AssertionError(f"{args!r} dry-run failed ({completed.returncode}): {output[-2000:]}")
    return output


class MakeRoutingTests(unittest.TestCase):
    def test_buildbuddy_policy_modes(self) -> None:
        expected = {
            ("normal", "main"): "write",
            ("normal", "pr"): "read",
            ("normal", "fork"): "off",
            ("conserve", "main"): "write",
            ("conserve", "pr"): "off",
            ("conserve", "fork"): "off",
            ("off", "main"): "off",
        }
        for (mode, context), access in expected.items():
            with self.subTest(mode=mode, context=context):
                with tempfile.TemporaryDirectory() as temporary:
                    output = Path(temporary) / "output"
                    env = {**os.environ, "GITHUB_OUTPUT": str(output)}
                    result = subprocess.run(
                        [MAKE, "-C", str(ROOT), "__bazel-buildbuddy-policy",
                         f"ORLIX_BUILDBUDDY_CACHE_MODE={mode}",
                         f"ORLIX_BUILDBUDDY_CONTEXT={context}"],
                        capture_output=True, text=True, timeout=30, env=env,
                    )
                    self.assertEqual(result.returncode, 0, result.stderr)
                    self.assertIn(f"access={access}", output.read_text())

    def test_buildbuddy_policy_rejects_invalid_mode(self) -> None:
        result = subprocess.run(
            [MAKE, "-C", str(ROOT), "__bazel-buildbuddy-policy",
             "ORLIX_BUILDBUDDY_CACHE_MODE=invalid",
             "ORLIX_BUILDBUDDY_CONTEXT=main"],
            capture_output=True, text=True, timeout=30,
        )
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("invalid ORLIX_BUILDBUDDY_CACHE_MODE", result.stderr)

    def test_ios15_source_gate_separates_build_from_runtime(self) -> None:
        aggregate = _dry_run("ios15-simulator-gate", "ORLIX_BAZEL_AUTHORITY=0")
        self.assertLess(aggregate.index("__ios15-simulator-build"), aggregate.index("__ios15-simulator-test"))
        build = _dry_run("__ios15-simulator-build", "ORLIX_BAZEL_AUTHORITY=0")
        runtime = _dry_run("__ios15-simulator-test", "ORLIX_BAZEL_AUTHORITY=0")
        self.assertIn("build-for-testing", build)
        self.assertNotIn("test-without-building", build)
        self.assertIn("test-without-building", runtime)
        self.assertNotIn("build-for-testing", runtime)
        self.assertIn("validate_simulator_app", runtime)
        self.assertIn("-test-timeouts-enabled YES", runtime)
        self.assertIn("-maximum-test-execution-time-allowance 180", runtime)

    def test_kernel_build_respects_profile_and_destination(self) -> None:
        output = _dry_run(
            "__bazel-kernel-boot", "PROFILE=development",
            "ORLIX_BAZEL_DESTINATION=iphoneos", "ORLIX_BAZEL_COMPILATION_MODE=opt",
        )
        command = next(line for line in output.splitlines() if " build //bazel/feasibility/kernel:macho " in line)
        self.assertIn("--config=development", command)
        self.assertIn("--compilation_mode=opt", command)
        self.assertIn("--ios_multi_cpus=arm64", command)
        self.assertIn("--platforms=@build_bazel_apple_support//platforms:ios_arm64", command)

    def test_prepared_kernel_does_not_acquire_isa_again(self) -> None:
        for prepared in ("0", "1"):
            result = subprocess.run(
                [MAKE, "-qp", "-f", "OrlixKernel/Sources/ports/orlix/kbuild/kernel-rules.mk",
                 "__prepare-kbuild", f"ORLIX_KERNEL_PORT_PREPARED={prepared}"],
                cwd=ROOT, capture_output=True, text=True, timeout=30,
            )
            self.assertIn(result.returncode, (0, 1), result.stderr)
            dependencies = next(line for line in result.stdout.splitlines() if line.startswith("__prepare-kbuild:"))
            self.assertEqual("__orlix-tcti-isa-prepare" in dependencies, prepared == "0")

    def test_default_keeps_pre_cutover_authority(self) -> None:
        self.assertIn("ORLIX_BAZEL_AUTHORITY ?= 0", (ROOT / "Makefile").read_text())
        output = _dry_run("xcodeproj", "ORLIX_BAZEL_AUTHORITY=0")
        self.assertNotIn("__bazel-feasibility-xcodeproj", output)
        self.assertIn("OrlixKernel/Makefile", output)

    def test_build_routes_to_orlix_app(self) -> None:
        output = _dry_run("build", "type=product")
        self.assertIn("__bazel-orlix-app", output)

    def test_source_app_does_not_reconstruct_promoted_components(self) -> None:
        output = _dry_run(
            "__bazel-orlix-app", "ORLIX_BAZEL_COMPONENT_MODE=source",
            "PROFILE=development", "ORLIX_BAZEL_DESTINATION=iphoneos",
            "ORLIX_BAZEL_COMPILATION_MODE=opt",
        )
        command = next(line for line in output.splitlines() if " build //Orlix:Orlix " in line)
        self.assertIn("--config=source", command)
        self.assertIn("--config=development", command)
        self.assertIn("--compilation_mode=opt", command)
        self.assertIn("--ios_multi_cpus=arm64", command)
        self.assertNotIn("bazel/promotion/reconstruct.py", output)
        self.assertIn(" cquery //Orlix:Orlix ", output)

    def test_test_routes_to_matrix_check(self) -> None:
        output = _dry_run("test")
        self.assertIn("__bazel-matrix-check", output)

    def test_app_tests_use_bazel_project_and_existing_suites(self) -> None:
        output = _dry_run("app-tests")
        self.assertIn("__bazel-test-app", output)
        self.assertIn("__bazel-test-app-architecture", output)
        self.assertNotIn("-project Orlix.xcodeproj", output)
        output = _dry_run("__bazel-test-app")
        self.assertIn("--action_env=ORLIX_PINNED_DEVELOPER_DIR=", output)
        self.assertNotIn("--action_env=DEVELOPER_DIR=", output)
        self.assertNotIn("--host_action_env=DEVELOPER_DIR=", output)

    def test_headers_install_routes_to_uapi(self) -> None:
        output = _dry_run("headers_install")
        self.assertIn("__bazel-kernel-uapi", output)

    def test_xcodeproj_routes_to_feasibility_project(self) -> None:
        output = _dry_run("xcodeproj")
        self.assertIn("__bazel-feasibility-xcodeproj", output)

    def test_rebuild_routes_to_orlix_app(self) -> None:
        output = _dry_run("rebuild")
        self.assertIn("__bazel-orlix-app", output)

    def test_apple_ci_builds_one_app_for_both_runtime_gates(self) -> None:
        makefile = (ROOT / "make" / "bazel-migration.mk").read_text(encoding="utf-8")
        apple_ci = makefile.split("__bazel-apple-ci:", 1)[1].split(
            "__bazel-simulator-runtime-proof:", 1
        )[0]
        self.assertEqual(apple_ci.count("__bazel-orlix-app"), 1)
        self.assertIn('ORLIX_BAZEL_APP_TARGETS="//Orlix:Orlix //Orlix:OrlixUITests"', apple_ci)
        self.assertIn("__bazel-current-simulator-gate", apple_ci)
        self.assertIn("__bazel-ios15-simulator-gate", apple_ci)
        self.assertEqual(apple_ci.count('ORLIX_BAZEL_APP_PATH="$$app"'), 2)
        self.assertNotIn("__tcti-isa-restore", makefile)
        self.assertNotIn("ORLIX_TCTI_ISA_PREPARED", makefile)
        self.assertNotIn("ORLIX_TCTI_ISA_ARTIFACTS", makefile)
        runtime = makefile.split("__bazel-simulator-runtime-proof:", 1)[1].split(
            "__bazel-current-simulator-gate:", 1
        )[0]
        self.assertIn("simctl install", runtime)
        self.assertIn("simctl launch", runtime)
        self.assertIn('kill -0 "$$launch_pid"', runtime)
        self.assertIn("DiagnosticReports", runtime)
        self.assertNotIn('"$(ORLIX_BAZEL)"', runtime)

    def test_xctest_proof_uses_helper_and_canonical_app(self) -> None:
        makefile = (ROOT / "make" / "bazel-migration.mk").read_text(encoding="utf-8")
        proof = makefile.split("__bazel-xctest-runtime-proof:", 1)[1].split(
            "__bazel-hostadapter:", 1
        )[0]
        self.assertIn('test -d "$(ORLIX_CANONICAL_APP_PATH)"', proof)
        self.assertIn('xctestrun.py" --resolve-test-bundle', proof)
        self.assertIn("cquery-outputs.txt", proof)
        self.assertIn('xctestrun.py" --developer-dir "$(ORLIX_PINNED_DEVELOPER_DIR)"', proof)
        self.assertIn('--only "AppLaunchSmokeUITests/testLaunchCapturesScreenshot"', proof)
        self.assertIn("test.xctestrun", proof)
        self.assertIn("OrlixUITests-Runner.app", proof)
        self.assertIn("IsXCTRunnerHostedTestBundle", proof)
        self.assertIn("OnlyTestIdentifiers", proof)
        self.assertIn("OrlixUITests-Runner.app/Info.plist", proof)
        self.assertIn("com.apple.test.OrlixUITests-Runner", proof)
        self.assertIn("test-without-building", proof)
        self.assertIn("-derivedDataPath", proof)
        self.assertIn("xctest-app-identity.txt", proof)
        self.assertIn("xctest-passed.log", proof)
        self.assertNotIn("-only-testing:", proof)
        self.assertNotIn("ZipFile", proof)
        self.assertNotIn("rglob", proof)

    def test_toolchain_manifest_lifecycle(self) -> None:
        mk = (ROOT / "make" / "bazel-migration.mk").read_text(encoding="utf-8")
        self.assertIn("__bazel-toolchain-manifest: __bazel-version-check", mk)
        self.assertIn(
            "__bazel-feasibility-bootstrap: __bazel-version-check __bazel-migration-inventory-check __bazel-toolchain-manifest",
            mk,
        )
        self.assertIn("__bazel-promote-buildset: __bazel-version-check __bazel-toolchain-manifest", mk)
        self.assertIn("__bazel-promote-$(1): __bazel-version-check __bazel-toolchain-manifest", mk)
        manifest = mk.split("__bazel-toolchain-manifest:", 1)[1].split(
            "__bazel-server-restart:", 1
        )[0]
        self.assertIn('xcode_select.py" --repo "$(CURDIR)" --build "$(ORLIX_XCODE_BUILD)"', manifest)
        self.assertIn("ensure_current_manifest", manifest)
        self.assertIn("validate_manifest", manifest)
        self.assertIn("ORLIX_BAZEL_RUN_ID", manifest)
        bootstrap = mk.split("__bazel-feasibility-bootstrap:", 1)[1].split(
            "__bazel-module-lock-update:", 1
        )[0]
        self.assertIn("validate_manifest", bootstrap)
        self.assertNotIn("capture_manifest(", bootstrap)
        self.assertNotIn("toolchain_pin.capture_manifest(", mk)

    def test_python3_c_payloads_compile_without_shell_operators(self) -> None:
        # Payloads must avoid single quotes inside (double quotes only), so the
        # first closing quote always ends the Python source.
        sources = [ROOT / "Makefile"] + sorted((ROOT / "make").glob("*.mk"))
        self.assertTrue(sources)
        checked = 0
        for source in sources:
            logical = []
            pending = ""
            for physical in source.read_text(encoding="utf-8").splitlines():
                if physical.endswith("\\"):
                    pending += physical[:-1] + "\n"
                else:
                    logical.append(pending + physical)
                    pending = ""
            for line in logical:
                if "python3 -c" not in line:
                    continue
                match = re.search(r"python3 -c '(.*?)'", line)
                self.assertIsNotNone(match, f"unparseable python3 -c in {source.name}: {line[:120]}")
                payload = match.group(1)
                code = payload.replace("$$", "__DOLLAR__")
                code = re.sub(r"\$\([\w_]+\)", "__MAKEVAR__", code)
                code = re.sub(r"\$\{[\w_]+(?::-[^}]*)?\}", "__MAKEVAR__", code)
                try:
                    compile(code, f"{source.name}:python3 -c", "exec")
                except SyntaxError as error:
                    self.fail(f"unexecutable python3 -c payload in {source.name}: {error}: {payload[:120]}")
                stripped = re.sub(r'"[^"]*"', '""', code)
                for operator in ("&&", "||", ">>", "<<"):
                    self.assertNotIn(operator, stripped, f"shell operator in {source.name}: {payload[:120]}")
                checked += 1
        self.assertGreater(checked, 20)

    def test_silent_prefix_only_starts_or_continues_recipes(self) -> None:
        # Proven with gmake 4.x: a leading @ is honored on a recipe's first
        # physical line and on lines ending with a backslash continuation,
        # but reaches the shell literally on a final line, failing as
        # "@cmd: command not found". Catch that shape statically. A TAB line
        # whose predecessor lacks a trailing backslash starts its own recipe
        # (separate shell) and may carry @.
        sources = [ROOT / "Makefile"] + sorted((ROOT / "make").glob("*.mk"))
        for source in sources:
            physical = source.read_text(encoding="utf-8").splitlines()
            for index, line in enumerate(physical):
                if not line.startswith("\t@"):
                    continue
                predecessor_continues = index > 0 and physical[index - 1].rstrip().endswith("\\")
                self.assertTrue(
                    not predecessor_continues or line.rstrip().endswith("\\"),
                    f"literal @ reaches the shell in {source.name}:{index + 1}: {line.strip()[:80]}",
                )

    def test_xctest_proof_writer_resolves_labels_beside_the_proof_file(self) -> None:
        makefile = (ROOT / "make" / "bazel-migration.mk").read_text(encoding="utf-8")
        apple_ci = makefile.split("__bazel-apple-ci:", 1)[1].split(
            "__bazel-simulator-runtime-proof:", 1
        )[0]
        lines = [line for line in apple_ci.splitlines() if "xctest evidence mismatch" in line]
        self.assertEqual(len(lines), 1)
        recipe = lines[0].strip().lstrip("@").rstrip("\\").rstrip().rstrip(";")
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            app = "/the/canonical/app"
            for label in ("current", "ios15"):
                (root / label).mkdir()
                (root / label / "xctest-passed.log").touch()
                (root / label / "xctest-app-identity.txt").write_text(app + "\n", encoding="utf-8")
            command = (
                recipe.replace("$$evidence_dir", str(root))
                .replace("$(ORLIX_CURRENT_SIMULATOR_VERSION)", "26.5")
                .replace("$$app", app)
            )
            completed = subprocess.run(
                ["bash", "-c", command], capture_output=True, text=True, timeout=60
            )
            self.assertEqual(completed.returncode, 0, completed.stderr[-2000:])
            proof = (root / "runtime-proof.json").read_text(encoding="utf-8")
            self.assertIn('"same_application_path": "/the/canonical/app"', proof)
            self.assertIn('"canonical_simulator_product_compile_count": 1', proof)

    def test_builder_product_targets(self) -> None:
        mk = (ROOT / "make" / "bazel-migration.mk").read_text(encoding="utf-8")
        self.assertIn("__bazel-product-app: __bazel-feasibility-bootstrap", mk)
        self.assertIn("__bazel-substitute-promoted", mk.split("__bazel-product-app:", 1)[1].split("__builder-component-mode:", 1)[0])
        product = mk.split("__bazel-product-app:", 1)[1].split("__builder-component-mode:", 1)[0]
        self.assertIn("build //Orlix:Orlix", product)
        self.assertIn("ORLIX_PRODUCT_APP=", product)
        self.assertIn("CFBundleSupportedPlatforms", product)
        self.assertIn("locked-buildset.json", product)
        self.assertNotIn("ORLIX_DEVELOPMENT_TEAM", product)
        mode = mk.split("__builder-component-mode:", 1)[1].split("ORLIX_BUILDER_BUILD_ID", 1)[0]
        self.assertIn("select_component_mode.py", mode)
        self.assertIn("HEAD~1 HEAD", mode)
        self.assertIn("ORLIX_BAZEL_COMPONENT_MODE=", mode)
        package = mk.split("__builder-package-ipa:", 1)[1].split("__bazel-orlix-archive:", 1)[0]
        self.assertIn("ORLIX_BUILDER_BUILD_ID", package)
        self.assertIn("iphoneos", package)
        self.assertIn("CFBundleVersion", package)
        self.assertIn(".ipa", package)
        self.assertIn("codesign --remove-signature", package)
        self.assertIn("staged app is still signed", package)
        self.assertNotIn("xcodebuild", package)
        self.assertNotIn("xcodebuild", product)

    def test_beta_archive_routes_to_orlix_archive(self) -> None:
        makefile = (ROOT / "Makefile").read_text(encoding="utf-8")
        self.assertIn("__bazel-orlix-archive", makefile)
        self.assertIn("ORLIX_BAZEL_AUTHORITY),1", makefile)
        recipe = (ROOT / "make/bazel-migration.mk").read_text().split("__bazel-orlix-archive:")[1].split("\n__bazel-ios15-simulator-gate:")[0]
        self.assertIn("build //Orlix:Orlix.xcarchive --apple_generate_dsym", recipe)
        self.assertIn("codesign --verify --deep --strict", recipe)
        self.assertNotIn("ORLIX_BETA_IPA_PATH", recipe)

    def test_substitute_promoted_maps_reconstructed_oci(self) -> None:
        mk = (ROOT / "make" / "bazel-migration.mk").read_text(encoding="utf-8")
        self.assertIn("__bazel-substitute-promoted", mk)
        self.assertIn("promoted-components.json", mk)
        self.assertIn("bazel/promotion/substitute.py", mk)
        self.assertIn("--stage", mk)
        self.assertIn("bazel/promotion/imported", mk)
        self.assertIn("origin_resolver.py", mk)
        self.assertIn("__bazel-substitute-promoted", mk.split("__bazel-orlix-app:")[1].split("__bazel-orlix-archive:")[0])

    def test_reconstruct_checks_local_store_before_network_tools(self) -> None:
        mk = (ROOT / "make" / "bazel-migration.mk").read_text(encoding="utf-8")
        recipe = mk.split("__bazel-reconstruct:", 1)[1].split(
            "__bazel-substitute-promoted:", 1
        )[0]
        self.assertNotIn("command -v oras", recipe)
        self.assertNotIn("command -v cosign", recipe)
        self.assertIn("ORLIX_COSIGN_PUB is required to reconstruct", recipe)
        self.assertIn("ORLIX_PROMOTED_ARTIFACT_STORE", recipe)
        reconstruct = (ROOT / "bazel" / "promotion" / "reconstruct.py").read_text(
            encoding="utf-8"
        )
        self.assertIn("cosign is required to reconstruct", reconstruct)
        self.assertIn("oras is required to reconstruct", reconstruct)

    def test_unsigned_promote_uses_two_clean_output_bases(self) -> None:
        mk = (ROOT / "make" / "bazel-migration.mk").read_text(encoding="utf-8")
        self.assertIn('for side in a b; do', mk)
        self.assertIn("--nouse_action_cache", mk)
        self.assertIn("--config=release --config=promotion", mk)
        self.assertIn('promote/$$$$side/output-base', mk)
        self.assertIn('promote/$$$$side/disk', mk)
        self.assertIn('"signed": false', mk)
        self.assertIn("unsigned promote mutated artifacts.lock.json", mk)
        self.assertIn("unsigned promote must not Cosign-sign", mk)

    def test_xcode_identity_comes_from_version_file_and_selection(self) -> None:
        mk = (ROOT / "make" / "bazel-migration.mk").read_text(encoding="utf-8")
        self.assertIn("ORLIX_XCODE_VERSION_FILE", mk)
        self.assertIn(".xcode-version", mk)
        self.assertNotIn("ORLIX_XCODE_VERSION ?= 26.6", mk)
        self.assertNotIn("/Applications/Xcode", mk)
        self.assertIn("ORLIX_PINNED_DEVELOPER_DIR ?= $(shell xcode-select -p 2>/dev/null)", mk)
        self.assertIn("__xcode-select:", mk)
        self.assertIn("xcode_select.py\" --repo \"$(CURDIR)\" --build \"$(ORLIX_XCODE_BUILD)\" --select", mk)
        bootstrap = mk.split("__bazel-feasibility-bootstrap:", 1)[1].split(
            "__bazel-module-lock-update:", 1
        )[0]
        self.assertIn("xcode_select.py\" --repo \"$(CURDIR)\" --build \"$(ORLIX_XCODE_BUILD)\"", bootstrap)

    def test_buildset_promotion_builds_all_components_twice(self) -> None:
        import json

        output = _dry_run("__bazel-promote-buildset")
        self.assertEqual(output.count(" build \"${labels[@]}\""), 1)
        self.assertIn("for side in a b", output)
        self.assertIn("--nouse_action_cache", output)
        self.assertIn("--disk_cache= --repository_cache=", output)
        self.assertIn("--remote_cache= --remote_executor=", output)
        self.assertIn("components.py\" --labels", output)
        registry = json.loads(
            (ROOT / "bazel" / "promotion" / "components.json").read_text(encoding="utf-8")
        )
        self.assertEqual(
            [entry["label"] for entry in registry["components"]],
            [
                "//bazel/feasibility/kernel:uapi",
                "//bazel/feasibility/mlibc:sysroot",
                "//bazel/feasibility/rootfs:rootfs",
                "//bazel/feasibility/kernel:kernel-release-iphoneos",
                "//bazel/feasibility/kernel:kernel-release-iphonesimulator",
                "//bazel/feasibility/kernel:kernel-development-iphoneos",
                "//bazel/feasibility/kernel:kernel-development-iphonesimulator",
            ],
        )

    def test_buildset_promotion_resumes_by_component_input_identity(self) -> None:
        mk = (ROOT / "make" / "bazel-migration.mk").read_text(encoding="utf-8")
        buildset = mk.split("__bazel-promote-buildset:", 1)[1].split(
            "define ORLIX_BAZEL_PUBLISH", 1
        )[0]
        # Identity comes from the component producing inputs enumerated by
        # Bazel, not from the commit SHA.
        self.assertIn("--compute-identity", buildset)
        self.assertIn("kind('source file', deps(set(", buildset)
        self.assertIn("components.py\" --labels", buildset)
        self.assertIn("--component-input-identity", buildset)
        self.assertIn("--proof-source-sha", buildset)
        self.assertNotIn("--check --promote-root \"$$promote\" --source-sha", buildset)
        # The expensive build lives behind the resume guard, after the check.
        # Use the A/B build marker (--nouse_action_cache); the checkpoint query
        # also runs with --batch, so --batch is not a unique build marker.
        self.assertLess(buildset.index("--compute-identity"), buildset.index("--nouse_action_cache"))
        self.assertLess(buildset.index("--nouse_action_cache"), buildset.index("--proof-source-sha"))
        self.assertIn("promotion checkpoint hit", buildset)
        self.assertIn("component-input-identity.txt", buildset)
        workflow = (ROOT / ".github" / "workflows" / "bazel-promote.yml").read_text(encoding="utf-8")
        self.assertIn("Restore verified promotion checkpoint", workflow)
        self.assertIn("Save verified promotion checkpoint", workflow)
        self.assertIn("path: Build/Bazel/promote", workflow)
        self.assertIn("orlix-promote-v2-", workflow)
        # The cache key is the computed identity, never the commit SHA.
        self.assertIn("orlix-promote-v2-${{ steps.promote-identity.outputs.identity }}", workflow)
        self.assertNotIn("orlix-promote-${{ github.sha }}", workflow)
        self.assertLess(
            workflow.index("Save verified promotion checkpoint"),
            workflow.index("Sign and publish the buildset to GHCR"),
        )

    def test_buildset_lock_requires_cosign_blob_verification(self) -> None:
        makefile = (ROOT / "make" / "bazel-migration.mk").read_text(encoding="utf-8")
        proposal = _dry_run("__bazel-lock-proposal")
        activation = _dry_run("__bazel-lock-from-signed")
        activation_recipe = makefile.split("__bazel-lock-from-signed:", 1)[1].split(
            "__bazel-reconstruct:", 1
        )[0]
        self.assertIn("cosign sign-blob", proposal)
        self.assertIn("cosign verify-blob", proposal)
        self.assertIn("buildset-lock-proposal.sigstore.json", proposal)
        self.assertIn("promotion-proof-index.json", proposal)
        self.assertIn("toolchain-manifest.json", proposal)
        self.assertIn("--toolchain-manifest", proposal)
        self.assertIn("--proof-index", proposal)
        self.assertIn("--promote-root", proposal)
        self.assertIn("--source-sha", proposal)
        self.assertIn("--proposal", activation)
        self.assertIn("--bundle", activation)
        self.assertIn("--apply-lock", activation)
        self.assertIn("--evidence-dir", activation_recipe)
        self.assertNotIn("__bazel-lock-proposal", activation_recipe)
        self.assertNotIn("ORLIX_COSIGN_KEY", activation_recipe)
        self.assertNotIn("--signed", activation_recipe)

    def test_kernel_promotion_routes_use_exact_v2_component_boundaries(self) -> None:
        mk = (ROOT / "make" / "bazel-migration.mk").read_text(encoding="utf-8")
        components = (
            ("kernel-release-iphoneos", "release", "arm64"),
            ("kernel-release-iphonesimulator", "release", "sim_arm64"),
            ("kernel-development-iphoneos", "development", "arm64"),
            ("kernel-development-iphonesimulator", "development", "sim_arm64"),
        )
        for component, profile, cpu in components:
            promote = _dry_run(f"__bazel-promote-{component}")
            self.assertIn(
                f"build //bazel/feasibility/kernel:{component}", promote
            )
            self.assertIn("--config=promotion", promote)
            self.assertIn(f"--config={profile}", promote)
            self.assertIn("--nouse_action_cache", promote)
            self.assertIn("--remote_cache= --remote_executor=", promote)
            self.assertIn(f"--ios_multi_cpus={cpu}", promote)
            self.assertIn(
                f"--platforms=@build_bazel_apple_support//platforms:ios_{cpu}",
                promote,
            )
            self.assertIn("--artifact-identity-format artifact-identity-v2", promote)
            self.assertIn(f"--component {component}", promote)
            self.assertIn(f"{component}/product", promote)
            publish = _dry_run(f"__bazel-publish-{component}")
            self.assertIn(
                "digest_file=\"$promote/a/digest.sha256\"", publish
            )
            self.assertIn(
                "artifact=\"$promote/a/staged\"", publish
            )

    def test_promotion_consumes_hermetic_feasibility_targets(self) -> None:
        mk = (ROOT / "make" / "bazel-migration.mk").read_text(encoding="utf-8")
        self.assertIn(
            "ORLIX_BAZEL_PROMOTE,uapi,//bazel/feasibility/kernel:uapi", mk
        )
        self.assertIn(
            "ORLIX_BAZEL_PROMOTE,mlibc,//bazel/feasibility/mlibc:sysroot", mk
        )
        self.assertIn(
            "ORLIX_BAZEL_PROMOTE,rootfs,//bazel/feasibility/rootfs:rootfs", mk
        )
        self.assertNotIn("legacy-marker-sha256", mk)
        for component in (
            "kernel-release-iphoneos",
            "kernel-release-iphonesimulator",
            "kernel-development-iphoneos",
            "kernel-development-iphonesimulator",
        ):
            self.assertIn(f"__bazel-promote-{component}", mk)
            self.assertIn(f"__bazel-publish-{component}", mk)
            self.assertIn(f"kernel-{component.split('-', 1)[1]}", mk)
        self.assertIn("artifact-identity-v2.json", mk)
        self.assertIn("artifact-identity-v2.sha256", mk)
        self.assertIn('schema") == 2', mk)
        self.assertIn("__bazel-rootfs:", mk)
        self.assertIn("//bazel/feasibility/packages:coreutils", mk)
        app = (ROOT / "Orlix" / "BUILD.bazel").read_text(encoding="utf-8")
        self.assertIn("//bazel/feasibility/rootfs:payload", app)
        self.assertIn("name = \"OrlixOSFramework\"", app)
        self.assertIn("//bazel/feasibility/kernel:macho_link", app)
        rootfs = (ROOT / "bazel/feasibility/rootfs/BUILD.bazel").read_text(encoding="utf-8")
        self.assertIn("//bazel/promotion:promoted_rootfs", rootfs)
        self.assertIn("origin_rootfs_promoted", rootfs)
        promoted = (ROOT / "bazel/promotion/BUILD.bazel").read_text(encoding="utf-8")
        promoted_rule = (ROOT / "bazel/promotion/promoted.bzl").read_text(encoding="utf-8")
        self.assertIn("orlix_promoted_rootfs", promoted)
        self.assertIn("orlix_promoted_uapi", promoted)
        self.assertIn("orlix_promoted_sysroot", promoted)
        self.assertIn("imported/rootfs/product/initramfs.cpio.gz", promoted)
        self.assertIn("imported/uapi/product/uapi.sha256", promoted)
        self.assertIn("imported/mlibc/product/sysroot.sha256", promoted)
        # The proof index binds buildset/<side>/execution.json and
        # build-events.json. After the A/B build the recipe must reclaim only
        # the huge output bases and keep those evidence files, and the whole
        # buildset directory must be removed only by the pre-build cleanup.
        self.assertIn('rm -rf "$$promote/buildset/$$side/output-base"', mk)
        self.assertEqual(
            mk.count("shutil.rmtree(sys.argv[1], ignore_errors=True)' \"$$promote/buildset\""),
            1,
        )
        self.assertIn("//bazel/promotion:promoted_apple_inputs", app)
        self.assertIn("orlix_promoted_apple_inputs", promoted)
        for position in (11, 12, 13):
            self.assertIn(f'"$exec_root/${{{position}}}"', promoted_rule)
            self.assertNotIn(f'"$exec_root/${position}"', promoted_rule)
        mlibc = (ROOT / "bazel/feasibility/mlibc/BUILD.bazel").read_text(encoding="utf-8")
        self.assertIn("//bazel/promotion:promoted_sysroot", mlibc)
        self.assertIn("origin_mlibc_promoted", mlibc)
        kernel = (ROOT / "bazel/feasibility/kernel/BUILD.bazel").read_text(encoding="utf-8")
        self.assertIn("//bazel/promotion:promoted_uapi", kernel)
        self.assertIn("origin_uapi_promoted", kernel)
        inventory = (ROOT / "bazel/migration/legacy-target-map.json").read_text(
            encoding="utf-8"
        )
        self.assertIn('"name": "__bazel-substitute-promoted"', inventory)
        self.assertIn('"name": "__bazel-promote-$(1)"', inventory)


if __name__ == "__main__":
    unittest.main()
