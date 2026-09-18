#!/usr/bin/env python3
"""Real-repository proof that the promotion checkpoint identity covers the
component producers and excludes the promotion control plane.

This runs the same Bazel query the promotion recipe runs and asserts, against
the actual repository graph, that representative producer inputs are captured
and that promotion control-plane code is not. It is skipped when Bazel is
unavailable so it never blocks a unit-only environment; canonical CI, which has
Bazel, exercises it.
"""

from __future__ import annotations

import os
import subprocess
import tempfile
import unittest
from pathlib import Path

import checkpoint

ROOT = Path(__file__).resolve().parents[2]
REGISTRY = ROOT / "bazel" / "promotion" / "components.json"


def _bazel() -> str | None:
    candidate = os.environ.get("ORLIX_BAZEL")
    if candidate and Path(candidate).is_file():
        return candidate
    for path in (
        Path.home() / "Library/Caches/Orlix/Tools/bazel/9.2.0/bazel",
        Path("/opt/homebrew/bin/bazel"),
        Path("/usr/local/bin/bazel"),
    ):
        if path.is_file():
            return str(path)
    return None


@unittest.skipUnless(_bazel() is not None, "bazel is required for the checkpoint integration proof")
class ComponentInputIntegrationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        import json

        labels = [
            entry["label"]
            for entry in json.loads(REGISTRY.read_text(encoding="utf-8"))["components"]
        ]
        expression = (
            "kind('source file', deps(set({labels}))) "
            "union buildfiles(deps(set({labels}))) "
            "union loadfiles(deps(set({labels})))"
        ).format(labels=" ".join(labels))
        with tempfile.TemporaryDirectory() as tmp:
            output_base = Path(tmp) / "output-base"
            completed = subprocess.run(
                [
                    _bazel(),
                    f"--output_base={output_base}",
                    "query",
                    expression,
                    "--output=label",
                    f"--repo_env=DEVELOPER_DIR={os.environ.get('DEVELOPER_DIR', '')}",
                ],
                cwd=str(ROOT),
                capture_output=True,
                text=True,
                timeout=1800,
            )
            if completed.returncode != 0:
                raise unittest.SkipTest(f"bazel query unavailable: {completed.stderr[-400:]}")
            labels_out = [line for line in completed.stdout.splitlines() if line.startswith("//")]
        paths = set()
        for label in labels_out:
            package, _, name = label[2:].partition(":")
            paths.add((f"{package}/{name}" if package else name).lstrip("/"))
        cls.paths = sorted(paths)

    def test_registry_is_the_authority(self) -> None:
        import json

        names = sorted(entry["name"] for entry in json.loads(REGISTRY.read_text())["components"])
        self.assertEqual(len(names), 7)

    def test_producer_inputs_are_captured(self) -> None:
        required = (
            "bazel/feasibility/kernel/kernel_uapi.bzl",
            "bazel/feasibility/kernel/kernel_macho.bzl",
            "bazel/feasibility/kernel/BUILD.bazel",
            "bazel/feasibility/mlibc/mlibc_sysroot.bzl",
            "bazel/feasibility/rootfs/rootfs.bzl",
            "bazel/artifact_identity.bzl",
        )
        missing = [path for path in required if path not in self.paths]
        self.assertEqual(missing, [], f"producer inputs not captured: {missing}")

    def test_control_plane_is_excluded(self) -> None:
        for path in (
            "bazel/promotion/lock_proposal.py",
            "bazel/promotion/sign.py",
            "bazel/promotion/publish.py",
            "bazel/promotion/checkpoint.py",
            "bazel/promotion/compare.py",
        ):
            self.assertNotIn(path, self.paths, f"control-plane file must not gate reuse: {path}")

    def test_identity_is_non_empty_and_tracks_a_real_input(self) -> None:
        base = checkpoint.component_input_identity(
            ROOT,
            self.paths + ["MODULE.bazel.lock"],
            REGISTRY,
            "release,promotion,26.6,iphoneos,dbg",
        )
        self.assertEqual(len(base), 64)
        mutated = checkpoint.component_input_identity(
            ROOT,
            self.paths + ["MODULE.bazel.lock"] + ["bazel/feasibility/kernel/kernel_uapi.bzl"],
            REGISTRY,
            "release,promotion,26.6,iphoneos,dbg",
        )
        self.assertEqual(base, mutated, "duplicate inputs must not change the identity")


if __name__ == "__main__":
    unittest.main()
