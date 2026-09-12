#!/usr/bin/env python3
"""Public Make targets must dry-run to the Bazel-owned shadows."""

from __future__ import annotations

import os
import subprocess
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

    def test_ios15_gate_keeps_pinned_isa_inputs(self) -> None:
        output = _dry_run(
            "ios15-simulator-gate",
            "ORLIX_IOS15_SIMULATOR_ID=00000000-0000-0000-0000-000000000000",
        )
        self.assertIn("__bazel-ios15-simulator-gate", output)
        makefile = (ROOT / "make" / "bazel-migration.mk").read_text(encoding="utf-8")
        self.assertNotIn("__tcti-isa-restore", makefile)
        self.assertNotIn("ORLIX_TCTI_ISA_PREPARED", makefile)
        self.assertNotIn("ORLIX_TCTI_ISA_ARTIFACTS", makefile)
        self.assertIn("deviceTypeIdentifier", makefile)
        self.assertIn("devicetypes", makefile)
        self.assertIn("--ios_simulator_device=", makefile)
        self.assertIn("build //Orlix:Orlix", makefile)
        self.assertIn("missing //Orlix:Orlix ipa after iOS 15 UI tests", makefile)

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
        self.assertIn('promote/$$$$side/output-base', mk)
        self.assertIn('promote/$$$$side/disk', mk)
        self.assertIn('"signed": false', mk)
        self.assertIn("unsigned promote mutated artifacts.lock.json", mk)
        self.assertIn("unsigned promote must not Cosign-sign", mk)

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
        self.assertIn("__bazel-rootfs:", mk)
        self.assertIn("//bazel/feasibility/packages:coreutils", mk)
        app = (ROOT / "Orlix" / "BUILD.bazel").read_text(encoding="utf-8")
        self.assertIn("//bazel/feasibility/rootfs:payload", app)
        self.assertIn("name = \"OrlixOSFramework\"", app)
        self.assertIn("//bazel/feasibility/kernel:macho_link", app)
        rootfs = (ROOT / "bazel/feasibility/rootfs/BUILD.bazel").read_text(encoding="utf-8")
        self.assertIn("//bazel/promotion:promoted_rootfs", rootfs)
        self.assertIn("component_promoted", rootfs)
        promoted = (ROOT / "bazel/promotion/BUILD.bazel").read_text(encoding="utf-8")
        self.assertIn("orlix_promoted_rootfs", promoted)
        self.assertIn("orlix_promoted_uapi", promoted)
        self.assertIn("orlix_promoted_sysroot", promoted)
        self.assertIn("imported/rootfs/initramfs.cpio.gz", promoted)
        self.assertIn("imported/uapi/uapi.sha256", promoted)
        self.assertIn("imported/mlibc/sysroot.sha256", promoted)
        self.assertIn("//bazel/promotion:promoted_apple_inputs", app)
        self.assertIn("orlix_promoted_apple_inputs", promoted)
        mlibc = (ROOT / "bazel/feasibility/mlibc/BUILD.bazel").read_text(encoding="utf-8")
        self.assertIn("//bazel/promotion:promoted_uapi", mlibc)
        inventory = (ROOT / "bazel/migration/legacy-target-map.json").read_text(
            encoding="utf-8"
        )
        self.assertIn('"name": "__bazel-substitute-promoted"', inventory)
        self.assertIn('"name": "__bazel-promote-$(1)"', inventory)


if __name__ == "__main__":
    unittest.main()
