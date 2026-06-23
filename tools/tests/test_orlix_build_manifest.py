#!/usr/bin/env python3

from __future__ import annotations

import contextlib
import io
import sys
import tempfile
import unittest
from argparse import Namespace
from importlib.machinery import SourceFileLoader
from importlib.util import module_from_spec, spec_from_loader
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
MODULE_PATH = REPO_ROOT / "tools" / "orlix-build-manifest"


def load_manifest_module():
    loader = SourceFileLoader("orlix_build_manifest", str(MODULE_PATH))
    spec = spec_from_loader(loader.name, loader)
    assert spec and spec.loader
    module = module_from_spec(spec)
    sys.modules[loader.name] = module
    spec.loader.exec_module(module)
    return module


class BuildManifestTests(unittest.TestCase):
    def setUp(self):
        self.module = load_manifest_module()
        self.module.environment_identity = lambda repo_root, profile: {
            "profile": profile,
            "platform": "test",
            "python": "test",
            "xcode_version": "test",
            "clang_version": "test",
            "git_head": "test",
        }

    def test_stage_manifest_is_stable_for_unchanged_inputs(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "input").mkdir()
            (root / "input" / "a.txt").write_text("alpha\n")
            stage = self.module.StageSpec("sample", ("input",))

            first = self.module.stage_manifest(root, "sample", stage, "release")
            second = self.module.stage_manifest(root, "sample", stage, "release")

            self.assertEqual(first["input_digest"], second["input_digest"])

    def test_stage_manifest_changes_when_input_changes(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "input.txt").write_text("alpha\n")
            stage = self.module.StageSpec("sample", ("input.txt",))

            first = self.module.stage_manifest(root, "sample", stage, "release")
            (root / "input.txt").write_text("beta\n")
            second = self.module.stage_manifest(root, "sample", stage, "release")

            self.assertNotEqual(first["input_digest"], second["input_digest"])

    def test_audit_hits_after_write_and_misses_after_change(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "input.txt").write_text("alpha\n")
            self.module.STAGES["sample"] = (
                self.module.StageSpec("stage", ("input.txt",)),
            )
            args = Namespace(
                component=["sample"],
                profile="release",
                repo_root=str(root),
                manifest_root="manifests",
                stage=None,
            )

            with contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(self.module.write(args), 0)
                self.assertEqual(self.module.audit(args), 0)

            (root / "input.txt").write_text("beta\n")
            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                self.assertEqual(self.module.audit(args), 1)

            self.assertIn("input-digest-changed", output.getvalue())

    def test_audit_misses_corrupt_manifest(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "input.txt").write_text("alpha\n")
            self.module.STAGES["sample"] = (
                self.module.StageSpec("stage", ("input.txt",)),
            )
            manifest = root / "manifests" / "release" / "sample" / "stage.json"
            manifest.parent.mkdir(parents=True)
            manifest.write_text("{not-json\n")
            args = Namespace(
                component=["sample"],
                profile="release",
                repo_root=str(root),
                manifest_root="manifests",
                stage=None,
            )

            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                self.assertEqual(self.module.audit(args), 1)

            self.assertIn("manifest-missing", output.getvalue())


    def test_manifest_miss_reason_detects_metadata_drift(self):
        current = {
            "manifest_version": 1,
            "depends_on": ["source-prep"],
            "environment": {"python": "3"},
            "inputs": [{"path": "input.txt", "digest": "abc"}],
            "optional_inputs": [],
            "input_digest": "abc",
        }
        recorded = dict(current)
        recorded["environment"] = {"python": "2"}

        self.assertEqual(
            self.module.manifest_miss_reason(recorded, current),
            "environment-changed",
        )

    def test_manifest_miss_reason_ignores_git_head_drift(self):
        current = {
            "manifest_version": 1,
            "depends_on": ["source-prep"],
            "environment": {"python": "3", "git_head": "new"},
            "inputs": [{"path": "input.txt", "digest": "abc"}],
            "optional_inputs": [],
            "input_digest": "abc",
        }
        recorded = dict(current)
        recorded["environment"] = {"python": "3", "git_head": "old"}

        self.assertIsNone(self.module.manifest_miss_reason(recorded, current))

    def test_audit_misses_when_recorded_environment_changes(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "input").mkdir()
            (root / "input" / "input.txt").write_text("alpha\n")
            manifest_root = root / "manifests"
            stage = self.module.StageSpec("sample", ("input",))
            self.module.STAGES = {"test": (stage,)}
            args = Namespace(
                command="write",
                repo_root=str(root),
                manifest_root=str(manifest_root),
                profile="release",
                component=("test",),
                stage=("sample",),
            )
            self.module.write(args)

            path = self.module.manifest_path(root, manifest_root, "release", "test", "sample")
            data = path.read_text()
            path.write_text(data.replace('"python"', '"stale-python"', 1))

            args.command = "audit"
            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                result = self.module.audit(args)

            self.assertEqual(result, 1)
            self.assertIn("environment-changed", output.getvalue())


if __name__ == "__main__":
    unittest.main()
