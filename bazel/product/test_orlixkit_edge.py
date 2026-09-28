"""The app links OrlixKit instead of the private Apple-native targets."""

from __future__ import annotations

import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PRIVATE = (
    "//OrlixHostAdapter/Sources:OrlixHostAdapter",
    "//OrlixKernel/Sources:OrlixKernelBoot",
    "//bazel/feasibility/kernel:macho_link",
)
GUEST = (
    "OrlixMLibC",
    "OrlixCoreUtils",
    "rootfs",
    "libc.a",
)


def _rule(text: str, header: str) -> str:
    return text.split(header, 1)[1].split("\n)", 1)[0]


def _deps(block: str) -> str:
    return block.split("deps = [", 1)[1].split("]", 1)[0]


class OrlixKitEdgeTests(unittest.TestCase):
    def test_kit_packages_native_link_inputs_only(self) -> None:
        product = (ROOT / "bazel/product/BUILD.bazel").read_text(encoding="utf-8")
        kit = _deps(_rule(product, 'objc_library(\n    name = "OrlixKit",'))
        for label in PRIVATE:
            self.assertIn(label, kit)
        for marker in GUEST:
            self.assertNotIn(marker, kit)
        self.assertIn("orlixkit_anchor.c", product)

    def test_app_and_framework_link_the_kit_edge(self) -> None:
        app = (ROOT / "Orlix/BUILD.bazel").read_text(encoding="utf-8")
        framework = _deps(_rule(app, 'ios_framework(\n    name = "OrlixOSFramework",'))
        application = _deps(_rule(app, 'ios_application(\n    name = "Orlix",'))
        test_app = _deps(_rule(app, 'ios_application(\n    name = "OrlixTestApp",'))
        for block in (framework, application, test_app):
            self.assertIn("//bazel/product:OrlixKit", block)
            for label in PRIVATE:
                self.assertNotIn(label, block)
            self.assertNotIn("rootfs", block)
        resources = _rule(app, 'apple_resource_group(\n    name = "os_resources",')
        self.assertIn("//bazel/feasibility/rootfs:payload", resources)
        self.assertIn("//bazel/feasibility/kernel:macho_boot_resources", resources)

    def test_guest_markers_stay_on_the_os_module(self) -> None:
        session = (ROOT / "OrlixOS/Sources/Session/BUILD.bazel").read_text(encoding="utf-8")
        deps = _deps(_rule(session, 'swift_library(\n    name = "OrlixOS",'))
        self.assertIn("//OrlixMLibC/Sources/Provider:OrlixMLibCProvider", deps)
        self.assertIn("//OrlixCoreUtils/Sources/Provider:OrlixCoreUtilsProvider", deps)
        self.assertNotIn("OrlixKit", deps)
        app = (ROOT / "Orlix/BUILD.bazel").read_text(encoding="utf-8")
        for header in (
            'ios_framework(\n    name = "OrlixOSFramework",',
            'ios_application(\n    name = "Orlix",',
            'ios_application(\n    name = "OrlixTestApp",',
        ):
            deps = _deps(_rule(app, header))
            self.assertNotIn("OrlixMLibCProvider", deps)
            self.assertNotIn("OrlixCoreUtilsProvider", deps)

    def test_nm_edge_check_remains(self) -> None:
        makefile = (ROOT / "make/bazel-migration.mk").read_text(encoding="utf-8")
        self.assertIn("bazel/product/hostadapter_edge.py --check", makefile)
        self.assertNotIn('"undefined_kernel_symbols": []', makefile)
        composition = (ROOT / "bazel/product/composition.bzl").read_text(encoding="utf-8")
        self.assertIn('load("@rules_cc//cc/common:cc_info.bzl", "CcInfo")', composition)


if __name__ == "__main__":
    unittest.main()
