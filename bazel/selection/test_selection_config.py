#!/usr/bin/env python3
"""Origin flags exist and unvalued global action env is gone."""

from __future__ import annotations

import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


class SelectionConfigTest(unittest.TestCase):
    def test_unvalued_global_action_env_is_removed(self) -> None:
        text = (ROOT / ".bazelrc").read_text(encoding="utf-8")
        self.assertNotIn("DEVELOPER_DIR", text)
        self.assertNotIn("ORLIX_PINNED_DEVELOPER_DIR", text)
        self.assertNotIn("ORLIX_BAZEL_AUTHORITY", text)
        self.assertNotIn("component_mode=auto", text)
        for line in text.splitlines():
            self.assertFalse(line.startswith("build --action_env="), line)
            self.assertFalse(line.startswith("build --host_action_env="), line)
        self.assertIn("build:promotion --action_env=CCACHE_DISABLE=1", text)
        self.assertIn("build:promotion --action_env=ORLIX_PACKAGE_INCREMENTAL=0", text)
        self.assertIn("build:promotion --action_env=ORLIX_MLIBC_INCREMENTAL=0", text)
        self.assertIn("build:promotion --action_env=ORLIX_KERNEL_INCREMENTAL=0", text)
        self.assertIn("build:promotion --action_env=ORLIX_COMPILER_LAUNCHER=", text)
        self.assertIn("build:source --//bazel/config:origin_kernel=source", text)
        self.assertIn("build:source --//bazel/config:use_promoted_lock=false", text)
        self.assertIn("build:promoted --//bazel/config:origin_mlibc=promoted", text)
        self.assertIn("build:promoted --//bazel/config:use_promoted_lock=true", text)

    def test_component_mode_stays_source_or_promoted(self) -> None:
        text = (ROOT / "bazel/config/BUILD.bazel").read_text(encoding="utf-8")
        start = text.index('name = "component_mode"')
        end = text.index('name = "origin_kernel"')
        block = text[start:end]
        self.assertNotIn("auto", block)
        self.assertIn('"source"', block)
        self.assertIn('"promoted"', block)
        for name in ("origin_kernel", "origin_uapi", "origin_mlibc", "origin_rootfs", "use_promoted_lock"):
            self.assertIn(f'name = "{name}"', text)


if __name__ == "__main__":
    unittest.main()
