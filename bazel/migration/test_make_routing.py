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
        [MAKE, "-C", str(ROOT), "-n", *args],
        check=False,
        capture_output=True,
        text=True,
        env=env,
    )
    output = completed.stdout + completed.stderr
    if completed.returncode != 0:
        raise AssertionError(f"{args!r} dry-run failed ({completed.returncode}): {output[-2000:]}")
    return output


class MakeRoutingTests(unittest.TestCase):
    def test_build_routes_to_orlix_app(self) -> None:
        output = _dry_run("build", "type=product")
        self.assertIn("__bazel-orlix-app", output)

    def test_test_routes_to_matrix_check(self) -> None:
        output = _dry_run("test")
        self.assertIn("__bazel-matrix-check", output)

    def test_headers_install_routes_to_uapi(self) -> None:
        output = _dry_run("headers_install")
        self.assertIn("__bazel-kernel-uapi", output)

    def test_xcodeproj_routes_to_feasibility_project(self) -> None:
        output = _dry_run("xcodeproj")
        self.assertIn("__bazel-feasibility-xcodeproj", output)

    def test_rebuild_routes_to_orlix_app(self) -> None:
        output = _dry_run("rebuild")
        self.assertIn("__bazel-orlix-app", output)

    def test_ios15_gate_restores_isa_tables(self) -> None:
        output = _dry_run(
            "ios15-simulator-gate",
            "ORLIX_IOS15_SIMULATOR_ID=00000000-0000-0000-0000-000000000000",
        )
        self.assertIn("__bazel-ios15-simulator-gate", output)
        self.assertIn("prepared-tables.tar.gz", output)
        self.assertIn("source_manifest.def", output)
        makefile = (ROOT / "make" / "bazel-migration.mk").read_text(encoding="utf-8")
        self.assertIn("deviceTypeIdentifier", makefile)
        self.assertIn("devicetypes", makefile)
        self.assertIn("--ios_simulator_device=", makefile)
        self.assertIn("build //Orlix:Orlix", makefile)
        self.assertIn("missing //Orlix:Orlix ipa after iOS 15 UI tests", makefile)

    def test_beta_archive_routes_to_orlix_archive(self) -> None:
        makefile = (ROOT / "Makefile").read_text(encoding="utf-8")
        self.assertIn("__bazel-orlix-archive", makefile)
        self.assertIn("ORLIX_BAZEL_AUTHORITY),1", makefile)

    def test_substitute_promoted_maps_reconstructed_oci(self) -> None:
        mk = (ROOT / "make" / "bazel-migration.mk").read_text(encoding="utf-8")
        self.assertIn("__bazel-substitute-promoted", mk)
        self.assertIn("promoted-components.json", mk)
        self.assertIn("bazel/promotion/substitute.py", mk)

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
        inventory = (ROOT / "bazel/migration/legacy-target-map.json").read_text(
            encoding="utf-8"
        )
        self.assertIn('"name": "__bazel-substitute-promoted"', inventory)
        self.assertIn('"name": "__bazel-promote-$(1)"', inventory)


if __name__ == "__main__":
    unittest.main()
