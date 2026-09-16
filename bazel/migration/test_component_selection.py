from __future__ import annotations

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from component_selection import PROMOTED_MODE, SOURCE_MODE, select_component_mode


class ComponentSelectionTests(unittest.TestCase):
    def test_non_pull_request_events_follow_the_lock_schema(self) -> None:
        self.assertEqual(PROMOTED_MODE, select_component_mode([], 2, event_name="push"))
        self.assertEqual(SOURCE_MODE, select_component_mode([], 1, event_name="push"))
        self.assertEqual(SOURCE_MODE, select_component_mode(["Orlix/Sources/App.swift"], 1, event_name="push"))

    def test_pull_request_with_app_only_changes_selects_promoted(self) -> None:
        paths = ["Orlix/Sources/Terminal/Pane.swift", "OrlixHostAdapter/Sources/Host.swift", "docs/index.md"]
        self.assertEqual(PROMOTED_MODE, select_component_mode(paths, 2, event_name="pull_request"))

    def test_agent_harness_changes_select_source_mode(self) -> None:
        self.assertEqual(
            SOURCE_MODE,
            select_component_mode(["Tools/AgentHarness/tests/policy.py"], 2, event_name="pull_request"),
        )

    def test_component_sources_select_source_mode(self) -> None:
        for path in (
            "OrlixKernel/Sources/ports/orlix/overlay/kernel/sched/core.c",
            "OrlixMLibC/Sources/options/ansi/generic/stdio.c",
            "OrlixCoreUtils/Sources/true.c",
            "OrlixOS/Sources/init/rootinit.c",
            "OrlixOS/Sources/make/exec.mk",
            "OrlixOS/Sources/patches/libselinux.patch",
            "OrlixOS/Sources/distribution/target-settings.xcconfig",
            "bazel/feasibility/packages/coreutils.bzl",
            "bazel/promotion/compare.py",
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
        ):
            self.assertEqual(
                SOURCE_MODE,
                select_component_mode([path], 2, event_name="pull_request"),
                path,
            )

    def test_orlixos_app_only_surfaces_select_promoted(self) -> None:
        paths = ["OrlixOS/Sources/include/orlix.h", "OrlixOS/Sources/Session/Session.swift"]
        self.assertEqual(PROMOTED_MODE, select_component_mode(paths, 2, event_name="pull_request"))

    def test_unknown_or_ambiguous_paths_select_source_mode(self) -> None:
        self.assertEqual(SOURCE_MODE, select_component_mode(["new_dir/file.c"], 2, event_name="pull_request"))
        self.assertEqual(SOURCE_MODE, select_component_mode([], 2, event_name="pull_request"))
        self.assertEqual(
            SOURCE_MODE,
            select_component_mode(["Orlix/Sources/App.swift", "OrlixMLibC/Sources/x.c"], 2, event_name="pull_request"),
        )

    def test_deletions_and_renames_follow_the_same_rules(self) -> None:
        self.assertEqual(SOURCE_MODE, select_component_mode(["OrlixKernel/Makefile"], 2, event_name="pull_request"))
        self.assertEqual(PROMOTED_MODE, select_component_mode(["Orlix/Sources/Old.swift"], 2, event_name="pull_request"))

    def test_selection_is_deterministic(self) -> None:
        paths = ["Orlix/Sources/App.swift", "docs/index.md"]
        self.assertEqual(
            select_component_mode(paths, 2, event_name="pull_request"),
            select_component_mode(list(reversed(paths)), 2, event_name="pull_request"),
        )


if __name__ == "__main__":
    unittest.main()
