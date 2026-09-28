"""The app links OrlixKit instead of the private Apple-native targets."""

from __future__ import annotations

import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PRIVATE = (
    "//OrlixHostAdapter/Sources:OrlixHostAdapter",
    "//OrlixKernel/Sources:OrlixKernelBoot",
    "//bazel/feasibility/kernel:macho_link",
    "//bazel/product:OrlixBootloader",
    "//bazel/product:OrlixEngine",
)
KIT = (
    "//OrlixHostAdapter/Sources:OrlixHostAdapter",
    "//bazel/product:OrlixEngine",
)
ENGINE = (
    "//OrlixOS/Sources/Session:OrlixOS",
    "//bazel/feasibility/kernel:macho_link",
    "//bazel/product:OrlixBootloader",
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
        for label in KIT:
            self.assertIn(label, kit)
        self.assertNotIn("//OrlixKernel/Sources:OrlixKernelBoot", kit)
        self.assertNotIn("OrlixBootloader", kit)
        self.assertNotIn("macho_link", kit)
        for marker in GUEST:
            self.assertNotIn(marker, kit)
        self.assertIn("orlixkit_anchor.c", product)
        engine = _deps(_rule(product, 'objc_library(\n    name = "OrlixEngine",'))
        for label in ENGINE:
            self.assertEqual(engine.count(label), 1)
        self.assertNotIn("//OrlixHostAdapter/Sources:OrlixHostAdapter", engine)
        self.assertNotIn("//OrlixKernel/Sources:OrlixKernelBoot", engine)
        self.assertNotIn("OrlixKit", engine)
        self.assertNotIn("macho_archive", engine)
        self.assertNotIn("selected_macho", engine)
        for marker in GUEST:
            self.assertNotIn(marker, engine)
        anchor = (ROOT / "bazel/product/orlixengine_anchor.c").read_text(encoding="utf-8")
        self.assertIn("static const int orlix_engine_link_anchor", anchor)
        self.assertNotIn("mmap", anchor)
        self.assertNotIn(".elf", anchor)
        bootloader = _deps(_rule(product, 'objc_library(\n    name = "OrlixBootloader",'))
        self.assertIn("//OrlixHostAdapter/Sources:OrlixHostAdapter", bootloader)
        self.assertIn("//OrlixKernel/Sources:OrlixKernelBoot", bootloader)
        self.assertNotIn("macho_link", bootloader)
        self.assertNotIn("OrlixKit", bootloader)
        for marker in GUEST:
            self.assertNotIn(marker, bootloader)
        self.assertIn("orlixbootloader_anchor.c", product)

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

    def test_public_xcframework_packages_the_kit_edge(self) -> None:
        product = (ROOT / "bazel/product/BUILD.bazel").read_text(encoding="utf-8")
        framework = _rule(product, 'apple_static_xcframework(\n    name = "public_xcframework",')
        deps = _deps(framework)
        self.assertEqual(deps.count("//bazel/product:OrlixKit"), 1)
        for label in PRIVATE:
            self.assertNotIn(label, deps)
        for marker in GUEST:
            self.assertNotIn(marker, framework)
        self.assertIn('bundle_name = "OrlixKit"', framework)
        self.assertIn('"device": ["arm64"]', framework)
        self.assertIn('"simulator": ["arm64"]', framework)
        self.assertNotIn("x86_64", framework)
        self.assertIn('"ios": "15.0"', framework)
        header = (ROOT / "bazel/product/orlixkit_umbrella.h").read_text(encoding="utf-8")
        self.assertNotIn("mmap", header)
        self.assertNotIn(".elf", header)
        makefile = (ROOT / "make/bazel-migration.mk").read_text(encoding="utf-8")
        self.assertNotIn("OrlixKit", makefile)
        self.assertIn(
            "build $(ORLIX_BAZEL_APP_TARGETS) //bazel/product:public_xcframework ",
            makefile,
        )


if __name__ == "__main__":
    unittest.main()
