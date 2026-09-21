#!/usr/bin/env python3
"""Daily surfaces share one origin owner and selected_* producers."""

from __future__ import annotations

import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


class DailyRoutingTests(unittest.TestCase):
    def test_packages_consume_selected_uapi_and_sysroot(self) -> None:
        text = (ROOT / "bazel/feasibility/packages/BUILD.bazel").read_text(encoding="utf-8")
        self.assertIn('_SYSROOT = "//bazel/feasibility/mlibc:selected_sysroot"', text)
        self.assertIn('_UAPI = "//bazel/feasibility/kernel:selected_uapi"', text)
        self.assertNotIn("//bazel/feasibility/kernel:uapi", text)
        self.assertNotIn("//bazel/feasibility/mlibc:sysroot", text)

    def test_product_consumes_selected_macho(self) -> None:
        text = (ROOT / "bazel/product/BUILD.bazel").read_text(encoding="utf-8")
        self.assertIn('linux_archive = "//bazel/feasibility/kernel:selected_macho"', text)
        self.assertNotIn("//bazel/feasibility/kernel:macho", text)

    def test_rootfs_payload_consumes_selected_rootfs(self) -> None:
        text = (ROOT / "bazel/feasibility/rootfs/BUILD.bazel").read_text(encoding="utf-8")
        self.assertIn('rootfs = ":selected_rootfs"', text)

    def test_one_auto_preflight_owner(self) -> None:
        mk = (ROOT / "make/bazel-migration.mk").read_text(encoding="utf-8")
        self.assertEqual(mk.count("bazel/selection/auto_preflight.py"), 1)
        orlix_app = mk.split("__bazel-orlix-app:")[1].split("\n__")[0]
        product_app = mk.split("__bazel-product-app:")[1].split("\n__")[0]
        xcodeproj = mk.split("__bazel-feasibility-xcodeproj:")[1].split("\n__")[0]
        self.assertIn("__bazel-auto-preflight", orlix_app)
        self.assertIn("__bazel-auto-preflight", product_app)
        self.assertIn("__bazel-auto-preflight", xcodeproj)

    def test_xcodeproj_uses_component_mode_not_hardcoded_source(self) -> None:
        recipe = (ROOT / "make/bazel-migration.mk").read_text(encoding="utf-8")
        chunk = recipe.split("__bazel-feasibility-xcodeproj:")[1].split("\n__")[0]
        self.assertIn("--config=$(ORLIX_BAZEL_COMPONENT_MODE)", chunk)
        self.assertNotIn("--config=source", chunk)
        self.assertIn("origin-flags.txt", chunk)

    def test_classifier_is_not_copied_into_xcode_starlark(self) -> None:
        text = (ROOT / "xcode/BUILD.bazel").read_text(encoding="utf-8")
        self.assertNotIn("auto_preflight", text)
        self.assertNotIn("worktree_classifier", text)
        self.assertNotIn("origin_resolver", text)


if __name__ == "__main__":
    unittest.main()
