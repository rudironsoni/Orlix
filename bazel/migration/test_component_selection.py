from __future__ import annotations

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from component_selection import PROMOTED_MODE, SOURCE_MODE, select_component_mode


class ComponentSelectionTests(unittest.TestCase):
    def test_app_only_pr_selects_promoted(self) -> None:
        paths = ["Orlix/Sources/Terminal/Pane.swift", "OrlixHostAdapter/Sources/Host.swift", "docs/index.md"]
        self.assertEqual(PROMOTED_MODE, select_component_mode(paths, 2))

    def test_component_pr_selects_source(self) -> None:
        self.assertEqual(
            SOURCE_MODE,
            select_component_mode(["OrlixKernel/Sources/ports/orlix/kernel_macho.bzl"], 2),
        )

    def test_app_only_main_push_selects_promoted(self) -> None:
        paths = ["Orlix/Sources/App.swift", "OrlixOS/Sources/Session/Session.swift"]
        self.assertEqual(PROMOTED_MODE, select_component_mode(paths, 2))

    def test_kernel_main_push_selects_source(self) -> None:
        self.assertEqual(
            SOURCE_MODE,
            select_component_mode(["OrlixKernel/Sources/ports/orlix/kernel_macho.bzl"], 2),
        )

    def test_mlibc_main_push_selects_source(self) -> None:
        self.assertEqual(
            SOURCE_MODE,
            select_component_mode(["OrlixMLibC/Sources/string/memcpy.c"], 2),
        )

    def test_rootfs_package_main_push_selects_source(self) -> None:
        for path in (
            "OrlixCoreUtils/Sources/true.c",
            "OrlixOS/Sources/init/rootinit.c",
            "bazel/feasibility/rootfs/rootfs.bzl",
            "bazel/feasibility/packages/coreutils.bzl",
        ):
            self.assertEqual(SOURCE_MODE, select_component_mode([path], 2), path)

    def test_promotion_bazel_toolchain_main_push_selects_source(self) -> None:
        for path in (
            "bazel/promotion/compare.py",
            "bazel/feasibility/packages/coreutils.bzl",
            "bazel/config/toolchain-pin.json",
            "bazel/extensions/native_sources.bzl",
            "bazel/migration/proof-map.json",
            "make/bazel-migration.mk",
            ".github/workflows/bazel-ci.yml",
            "artifacts.lock.json",
            "MODULE.bazel",
            "upstreams.lock.json",
            ".bazelrc",
            "Makefile",
            "third_party/patches/x.patch",
            "xcode/project.xcodeproj/project.pbxproj",
            "project.yml",
            "Brewfile",
            "OrlixOS/Sources/make/exec.mk",
            "OrlixOS/Sources/patches/libselinux.patch",
            "OrlixOS/Sources/distribution/target-settings.xcconfig",
        ):
            self.assertEqual(SOURCE_MODE, select_component_mode([path], 2), path)

    def test_unknown_or_empty_classification_selects_source(self) -> None:
        self.assertEqual(SOURCE_MODE, select_component_mode(["new_dir/file.c"], 2))
        self.assertEqual(SOURCE_MODE, select_component_mode([], 2))
        self.assertEqual(
            SOURCE_MODE,
            select_component_mode(["Orlix/Sources/App.swift", "OrlixMLibC/Sources/x.c"], 2),
        )

    def test_agent_harness_changes_select_source_mode(self) -> None:
        self.assertEqual(
            SOURCE_MODE,
            select_component_mode(["Tools/AgentHarness/tests/policy.py"], 2),
        )

    def test_orlixos_app_only_surfaces_select_promoted(self) -> None:
        paths = ["OrlixOS/Sources/include/orlix.h", "OrlixOS/Sources/Session/Session.swift"]
        self.assertEqual(PROMOTED_MODE, select_component_mode(paths, 2))

    def test_deletions_and_renames_follow_the_same_rules(self) -> None:
        self.assertEqual(SOURCE_MODE, select_component_mode(["OrlixKernel/Makefile"], 2))
        self.assertEqual(PROMOTED_MODE, select_component_mode(["Orlix/Sources/Old.swift"], 2))

    def test_selection_is_deterministic(self) -> None:
        paths = ["Orlix/Sources/App.swift", "docs/index.md"]
        self.assertEqual(
            select_component_mode(paths, 2),
            select_component_mode(list(reversed(paths)), 2),
        )

    def test_old_lock_schema_always_selects_source(self) -> None:
        self.assertEqual(SOURCE_MODE, select_component_mode(["Orlix/Sources/App.swift"], 1))


if __name__ == "__main__":
    unittest.main()
