#!/usr/bin/env python3

from __future__ import annotations

import contextlib
import io
import json
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

    def test_stage_manifest_input_digest_ignores_metadata_drift(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "input").mkdir()
            (root / "input" / "input.txt").write_text("alpha\n")
            (root / "tools").mkdir()
            (root / self.module.MANIFEST_TOOL_PATH).write_text("tool-v1\n")
            stage = self.module.StageSpec("sample", ("input",))

            self.module.environment_identity = lambda repo_root, profile: {
                "profile": profile,
                "platform": "test",
                "python": "test",
                "xcode_version": "test",
                "clang_version": "test",
                "git_head": "old",
            }
            first = self.module.stage_manifest(root, "sample", stage, "release")

            (root / self.module.MANIFEST_TOOL_PATH).write_text("tool-v2\n")
            self.module.environment_identity = lambda repo_root, profile: {
                "profile": profile,
                "platform": "test",
                "python": "test",
                "xcode_version": "test",
                "clang_version": "test",
                "git_head": "new",
            }
            second = self.module.stage_manifest(root, "sample", stage, "release")

            self.assertEqual(first["input_digest"], second["input_digest"])
            self.assertIsNone(self.module.manifest_miss_reason(first, second))

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

    def test_manifest_miss_reason_ignores_tool_provenance_drift(self):
        current = {
            "manifest_version": 1,
            "depends_on": ["source-prep"],
            "environment": {"python": "3", "git_head": "new"},
            "tool": {"path": "tools/orlix-build-manifest", "digest": "new"},
            "inputs": [{"path": "input.txt", "digest": "abc"}],
            "optional_inputs": [],
            "input_digest": "abc",
        }
        recorded = dict(current)
        recorded["tool"] = {"path": "tools/orlix-build-manifest", "digest": "old"}

        self.assertEqual(
            self.module.manifest_miss_reason(recorded, current),
            None,
        )

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

    def test_audit_hits_when_only_manifest_tool_provenance_changes(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "input").mkdir()
            (root / "input" / "input.txt").write_text("alpha\n")
            (root / "tools").mkdir()
            (root / self.module.MANIFEST_TOOL_PATH).write_text("tool-v1\n")
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

            (root / self.module.MANIFEST_TOOL_PATH).write_text("tool-v2\n")

            args.command = "audit"
            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                result = self.module.audit(args)

            self.assertEqual(result, 0)
            self.assertIn("hit: test:sample", output.getvalue())

    def test_audit_misses_when_dependency_manifest_missing(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "input").mkdir()
            (root / "input" / "input.txt").write_text("alpha\n")
            (root / "tools").mkdir()
            (root / self.module.MANIFEST_TOOL_PATH).write_text("tool\n")
            manifest_root = root / "manifests"
            stage_a = self.module.StageSpec("a", ("input",))
            stage_b = self.module.StageSpec("b", ("input",), depends_on=("a",))
            self.module.STAGES = {"test": (stage_a, stage_b)}
            path = self.module.manifest_path(root, manifest_root, "release", "test", "b")
            path.parent.mkdir(parents=True)
            path.write_text(
                json.dumps(self.module.stage_manifest(root, "test", stage_b, "release"))
            )
            args = Namespace(
                command="audit",
                repo_root=str(root),
                manifest_root=str(manifest_root),
                profile="release",
                component=("test",),
                stage=("b",),
            )

            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                result = self.module.audit(args)

            self.assertEqual(result, 1)
            self.assertIn("dependency-missing a", output.getvalue())

    def test_audit_misses_when_dependency_manifest_stale(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "input").mkdir()
            (root / "input" / "input.txt").write_text("alpha\n")
            (root / "tools").mkdir()
            (root / self.module.MANIFEST_TOOL_PATH).write_text("tool\n")
            manifest_root = root / "manifests"
            stage_a = self.module.StageSpec("a", ("input",))
            stage_b = self.module.StageSpec("b", ("input",), depends_on=("a",))
            self.module.STAGES = {"test": (stage_a, stage_b)}
            for stage in (stage_a, stage_b):
                path = self.module.manifest_path(root, manifest_root, "release", "test", stage.name)
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(
                    json.dumps(self.module.stage_manifest(root, "test", stage, "release"))
                )
            (root / "input" / "input.txt").write_text("beta\n")
            stage_b_path = self.module.manifest_path(root, manifest_root, "release", "test", "b")
            stage_b_path.write_text(
                json.dumps(self.module.stage_manifest(root, "test", stage_b, "release"))
            )
            args = Namespace(
                command="audit",
                repo_root=str(root),
                manifest_root=str(manifest_root),
                profile="release",
                component=("test",),
                stage=("b",),
            )

            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                result = self.module.audit(args)

            self.assertEqual(result, 1)
            self.assertIn("dependency-stale a input-digest-changed", output.getvalue())

    def test_audit_misses_when_transitive_dependency_manifest_missing(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "input").mkdir()
            (root / "input" / "input.txt").write_text("alpha\n")
            (root / "tools").mkdir()
            (root / self.module.MANIFEST_TOOL_PATH).write_text("tool\n")
            manifest_root = root / "manifests"
            stage_a = self.module.StageSpec("a", ("input",))
            stage_b = self.module.StageSpec("b", ("input",), depends_on=("a",))
            stage_c = self.module.StageSpec("c", ("input",), depends_on=("b",))
            self.module.STAGES = {"test": (stage_a, stage_b, stage_c)}
            for stage in (stage_b, stage_c):
                path = self.module.manifest_path(root, manifest_root, "release", "test", stage.name)
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(
                    json.dumps(self.module.stage_manifest(root, "test", stage, "release"))
                )
            args = Namespace(
                command="audit",
                repo_root=str(root),
                manifest_root=str(manifest_root),
                profile="release",
                component=("test",),
                stage=("c",),
            )

            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                result = self.module.audit(args)

            self.assertEqual(result, 1)
            self.assertIn("dependency-of b dependency-missing a", output.getvalue())

    def test_audit_misses_when_dependency_graph_cycles(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "input").mkdir()
            (root / "input" / "input.txt").write_text("alpha\n")
            (root / "tools").mkdir()
            (root / self.module.MANIFEST_TOOL_PATH).write_text("tool\n")
            manifest_root = root / "manifests"
            stage_a = self.module.StageSpec("a", ("input",), depends_on=("b",))
            stage_b = self.module.StageSpec("b", ("input",), depends_on=("a",))
            self.module.STAGES = {"test": (stage_a, stage_b)}
            for stage in (stage_a, stage_b):
                path = self.module.manifest_path(root, manifest_root, "release", "test", stage.name)
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(
                    json.dumps(self.module.stage_manifest(root, "test", stage, "release"))
                )
            args = Namespace(
                command="audit",
                repo_root=str(root),
                manifest_root=str(manifest_root),
                profile="release",
                component=("test",),
                stage=("a",),
            )

            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                result = self.module.audit(args)

            self.assertEqual(result, 1)
            self.assertIn("dependency-cycle a", output.getvalue())


    def test_audit_treats_directory_manifest_as_miss(self):
        for component in ("linux", "mlibc", "coreutils"):
            with self.subTest(component=component), tempfile.TemporaryDirectory() as tmp:
                manifest_root = Path(tmp) / "manifests"
                manifest_path = manifest_root / "release" / component / "source-prep.json"
                manifest_path.mkdir(parents=True)

                class Args:
                    pass

                Args.component = [component]
                Args.manifest_root = manifest_root
                Args.repo_root = REPO_ROOT
                Args.profile = "release"
                Args.stage = ["source-prep"]

                stdout = io.StringIO()
                with contextlib.redirect_stdout(stdout):
                    result = self.module.audit(Args)

                self.assertEqual(result, 1)
                self.assertIn(f"{component}:source-prep manifest-missing", stdout.getvalue())

    def test_audit_treats_non_object_manifest_json_as_miss(self):
        for component, manifest_text in (("linux", '"scalar"'), ("mlibc", "[]"), ("coreutils", "null")):
            with self.subTest(component=component), tempfile.TemporaryDirectory() as tmp:
                manifest_root = Path(tmp) / "manifests"
                manifest_path = manifest_root / "release" / component / "source-prep.json"
                manifest_path.parent.mkdir(parents=True)
                manifest_path.write_text(manifest_text + "\n")

                class Args:
                    pass

                Args.component = [component]
                Args.manifest_root = manifest_root
                Args.repo_root = REPO_ROOT
                Args.profile = "release"
                Args.stage = ["source-prep"]

                stdout = io.StringIO()
                with contextlib.redirect_stdout(stdout):
                    result = self.module.audit(Args)

                self.assertEqual(result, 1)
                self.assertIn(f"{component}:source-prep manifest-missing", stdout.getvalue())



class BuildManifestWriteGraphTests(unittest.TestCase):
    def setUp(self) -> None:
        loader = SourceFileLoader("orlix_build_manifest_write_graph", str(MODULE_PATH))
        spec = spec_from_loader("orlix_build_manifest_write_graph", loader)
        assert spec is not None
        module = module_from_spec(spec)
        sys.modules["orlix_build_manifest_write_graph"] = module
        loader.exec_module(module)
        self.module = module

    def test_write_rejects_unknown_dependency_graph(self) -> None:
        self.module.STAGES["synthetic"] = (
            self.module.StageSpec("payload", ("input",), depends_on=("missing",)),
        )
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "input").write_text("payload\n")
            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                result = self.module.write(
                    Namespace(
                        repo_root=str(root),
                        manifest_root=str(root / "manifests"),
                        component=("synthetic",),
                        stage="payload",
                        profile="release",
                    )
                )
            self.assertEqual(result, 1)
            self.assertIn("dependency-unknown payload missing", output.getvalue())
            self.assertFalse((root / "manifests" / "release" / "synthetic" / "payload.json").exists())

    def test_write_rejects_dependency_cycle_graph(self) -> None:
        self.module.STAGES["synthetic"] = (
            self.module.StageSpec("a", ("input",), depends_on=("b",)),
            self.module.StageSpec("b", ("input",), depends_on=("a",)),
        )
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "input").write_text("payload\n")
            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                result = self.module.write(
                    Namespace(
                        repo_root=str(root),
                        manifest_root=str(root / "manifests"),
                        component=("synthetic",),
                        stage="a",
                        profile="release",
                    )
                )
            self.assertEqual(result, 1)
            self.assertIn("dependency-cycle", output.getvalue())
            self.assertFalse((root / "manifests" / "release" / "synthetic" / "a.json").exists())

    def test_selected_stage_duplicates_are_normalized(self) -> None:
        args = type("Args", (), {"stage": ["source-prep", "source-prep", "headers-install"]})()
        stages = self.module.selected_stages(args, "linux")

        self.assertEqual([stage.name for stage in stages], ["source-prep", "headers-install"])

    def test_selected_component_duplicates_are_normalized(self) -> None:
        class Args:
            pass

        Args.component = ["linux", "linux", "mlibc", "coreutils", "mlibc"]
        self.assertEqual(("linux", "mlibc", "coreutils"), self.module.selected_components(Args))

    def test_selected_stage_write_includes_dependency_closure(self) -> None:
        cases = {
            "linux": ("kernel-archive", ["source-prep", "headers-install", "kernel-archive"]),
            "mlibc": ("compiler-rt", ["source-prep", "compiler-rt"]),
            "coreutils": ("install-rootfs", ["source-prep", "configure-build", "install-rootfs"]),
        }

        for component, (stage_name, expected) in cases.items():
            with self.subTest(component=component):
                args = type("Args", (), {"stage": [stage_name]})()
                stages = self.module.selected_stages_with_dependencies(args, component)

                self.assertEqual([stage.name for stage in stages], expected)

    def test_write_selected_stage_writes_dependency_closure(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            for relative_path in [
                "OrlixOS/Makefile",
                "OrlixOS/Sources/distribution/manifest.mk",
                "OrlixOS/Sources/make/rootfs.mk",
                "Build/OrlixOS/src/coreutils-9.11/stamp",
            ]:
                path = root / relative_path
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(relative_path)
            args = Namespace(
                repo_root=str(root),
                manifest_root="manifests",
                profile="release",
                component=["coreutils"],
                stage=["install-rootfs"],
            )

            with contextlib.redirect_stdout(io.StringIO()):
                result = self.module.write(args)

            self.assertEqual(result, 0)
            manifest_dir = root / "manifests" / "release" / "coreutils"
            self.assertEqual(
                sorted(path.name for path in manifest_dir.glob("*.json")),
                ["configure-build.json", "install-rootfs.json", "source-prep.json"],
            )

    def test_write_requires_output_sentinel_file(self) -> None:
        original_stages = self.module.STAGES.get("synthetic")
        self.module.STAGES["synthetic"] = (self.module.StageSpec("payload", ("input",)),)
        try:
            with tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp)
                (root / "input").write_text("payload")
                args = Namespace(
                    repo_root=str(root),
                    manifest_root="manifests",
                    profile="release",
                    component=["synthetic"],
                    stage=["payload"],
                    requires=["missing.output"],
                )
                output = io.StringIO()

                with contextlib.redirect_stdout(output):
                    result = self.module.write(args)

                self.assertEqual(result, 1)
                self.assertIn("missing required-output", output.getvalue())
                self.assertEqual(list((root / "manifests").glob("**/*.json")), [])
        finally:
            if original_stages is None:
                del self.module.STAGES["synthetic"]
            else:
                self.module.STAGES["synthetic"] = original_stages

    def test_write_rejects_absolute_output_sentinel_outside_repo(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            manifest_root = root / "manifests"
            output = io.StringIO()

            class Args:
                pass

            Args.repo_root = root
            Args.manifest_root = manifest_root
            Args.profile = "release"
            Args.component = ["linux"]
            Args.stage = ["source-prep"]
            Args.requires = ["/private/tmp/outside.output"]

            with contextlib.redirect_stdout(output):
                result = self.module.write(Args)

            self.assertEqual(result, 1)
            self.assertIn("required-output-outside-repo: /private/tmp/outside.output", output.getvalue())

    def test_write_accepts_absolute_output_sentinel_inside_repo(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            manifest_root = root / "manifests"
            output_file = root / "output.file"
            output_file.write_text("ready\n")
            output = io.StringIO()

            class Args:
                pass

            Args.repo_root = root
            Args.manifest_root = manifest_root
            Args.profile = "release"
            Args.component = ["mlibc"]
            Args.stage = ["source-prep"]
            Args.requires = [str(output_file)]

            with contextlib.redirect_stdout(output):
                result = self.module.write(Args)

            self.assertEqual(result, 0)
            self.assertIn("wrote: mlibc:source-prep", output.getvalue())

    def test_write_rejects_repo_escaping_output_sentinel(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            manifest_root = root / "manifests"
            output = io.StringIO()

            class Args:
                pass

            Args.repo_root = root
            Args.manifest_root = manifest_root
            Args.profile = "release"
            Args.component = ["coreutils"]
            Args.stage = ["source-prep"]
            Args.requires = ["../outside.output"]

            with contextlib.redirect_stdout(output):
                result = self.module.write(Args)

            self.assertEqual(result, 1)
            self.assertIn("required-output-outside-repo: ../outside.output", output.getvalue())

    def test_write_rejects_directory_output_sentinel(self) -> None:
        original_stages = self.module.STAGES.get("synthetic")
        self.module.STAGES["synthetic"] = (self.module.StageSpec("payload", ("input",)),)
        try:
            with tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp)
                (root / "input").write_text("payload")
                (root / "output.dir").mkdir()
                args = Namespace(
                    repo_root=str(root),
                    manifest_root="manifests",
                    profile="release",
                    component=["synthetic"],
                    stage=["payload"],
                    requires=["output.dir"],
                )
                output = io.StringIO()

                with contextlib.redirect_stdout(output):
                    result = self.module.write(args)

                self.assertEqual(result, 1)
                self.assertIn("required-output-not-file", output.getvalue())
                self.assertEqual(list((root / "manifests").glob("**/*.json")), [])
        finally:
            if original_stages is None:
                del self.module.STAGES["synthetic"]
            else:
                self.module.STAGES["synthetic"] = original_stages

    def test_write_accepts_present_output_sentinel(self) -> None:
        original_stages = self.module.STAGES.get("synthetic")
        self.module.STAGES["synthetic"] = (self.module.StageSpec("payload", ("input",)),)
        try:
            with tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp)
                (root / "input").write_text("payload")
                (root / "output.file").write_text("ready")
                args = Namespace(
                    repo_root=str(root),
                    manifest_root="manifests",
                    profile="release",
                    component=["synthetic"],
                    stage=["payload"],
                    requires=["output.file"],
                )

                with contextlib.redirect_stdout(io.StringIO()):
                    result = self.module.write(args)

                self.assertEqual(result, 0)
                self.assertTrue((root / "manifests" / "release" / "synthetic" / "payload.json").is_file())
        finally:
            if original_stages is None:
                del self.module.STAGES["synthetic"]
            else:
                self.module.STAGES["synthetic"] = original_stages

    def test_audit_reuses_computed_dependency_manifests(self) -> None:
        original_stages = self.module.STAGES.get("synthetic")
        self.module.STAGES["synthetic"] = (
            self.module.StageSpec("source", ("input",)),
            self.module.StageSpec("mid", ("input",), depends_on=("source",)),
            self.module.StageSpec("leaf", ("input",), depends_on=("mid",)),
        )
        original_stage_manifest = self.module.stage_manifest
        try:
            with tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp)
                (root / "input").write_text("payload")
                write_args = Namespace(
                    repo_root=str(root),
                    manifest_root="manifests",
                    profile="release",
                    component=["synthetic"],
                    stage=None,
                )
                with contextlib.redirect_stdout(io.StringIO()):
                    self.assertEqual(self.module.write(write_args), 0)

                calls = []

                def counted_stage_manifest(repo_root, component, stage, profile):
                    calls.append(stage.name)
                    return original_stage_manifest(repo_root, component, stage, profile)

                self.module.stage_manifest = counted_stage_manifest
                audit_args = Namespace(
                    repo_root=str(root),
                    manifest_root="manifests",
                    profile="release",
                    component=["synthetic"],
                    stage=["source", "mid", "leaf"],
                )

                with contextlib.redirect_stdout(io.StringIO()):
                    self.assertEqual(self.module.audit(audit_args), 0)

                self.assertEqual(calls, ["source", "mid", "leaf"])
        finally:
            self.module.stage_manifest = original_stage_manifest
            if original_stages is None:
                del self.module.STAGES["synthetic"]
            else:
                self.module.STAGES["synthetic"] = original_stages

    def test_write_rejects_unknown_selected_stage(self) -> None:
        self.module.STAGES["synthetic"] = (
            self.module.StageSpec("payload", ("input",)),
        )
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "input").write_text("payload\n")
            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                result = self.module.write(
                    Namespace(
                        repo_root=str(root),
                        manifest_root=str(root / "manifests"),
                        component=("synthetic",),
                        stage=("payload", "typo"),
                        profile="release",
                    )
                )
            self.assertEqual(result, 1)
            self.assertIn("stage-unknown synthetic typo", output.getvalue())
            self.assertFalse((root / "manifests" / "release" / "synthetic" / "payload.json").exists())

    def test_audit_rejects_unknown_selected_stage(self) -> None:
        self.module.STAGES["synthetic"] = (
            self.module.StageSpec("payload", ("input",)),
        )
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "input").write_text("payload\n")
            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                result = self.module.audit(
                    Namespace(
                        repo_root=str(root),
                        manifest_root=str(root / "manifests"),
                        component=("synthetic",),
                        stage=("payload", "typo"),
                        profile="release",
                    )
                )
            self.assertEqual(result, 1)
            self.assertIn("stage-unknown synthetic typo", output.getvalue())

    def test_write_rejects_unknown_component(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                result = self.module.write(
                    Namespace(
                        repo_root=str(root),
                        manifest_root=str(root / "manifests"),
                        component=("typo",),
                        stage=None,
                        profile="release",
                    )
                )
            self.assertEqual(result, 1)
            self.assertIn("component-unknown typo", output.getvalue())
            self.assertFalse((root / "manifests").exists())

    def test_audit_rejects_unknown_component(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                result = self.module.audit(
                    Namespace(
                        repo_root=str(root),
                        manifest_root=str(root / "manifests"),
                        component=("typo",),
                        stage=None,
                        profile="release",
                    )
                )
            self.assertEqual(result, 1)
            self.assertIn("component-unknown typo", output.getvalue())


    def test_write_rejects_directory_manifest_path(self):
        for component in ("linux", "mlibc", "coreutils"):
            with self.subTest(component=component), tempfile.TemporaryDirectory() as tmp:
                manifest_root = Path(tmp) / "manifests"
                manifest_path = manifest_root / "release" / component / "source-prep.json"
                manifest_path.mkdir(parents=True)

                class Args:
                    pass

                Args.component = [component]
                Args.manifest_root = manifest_root
                Args.repo_root = REPO_ROOT
                Args.profile = "release"
                Args.requires = []
                Args.stage = ["source-prep"]

                stdout = io.StringIO()
                with contextlib.redirect_stdout(stdout):
                    result = self.module.write(Args)

                self.assertEqual(result, 1)
                self.assertIn(f"manifest-write-failed {component}:source-prep", stdout.getvalue())
                self.assertEqual([], list(manifest_path.parent.glob(f".{manifest_path.name}.*.tmp")))

    def test_write_replaces_existing_manifest_atomically(self):
        for component in ("linux", "mlibc", "coreutils"):
            with self.subTest(component=component), tempfile.TemporaryDirectory() as tmp:
                manifest_root = Path(tmp) / "manifests"
                manifest_path = manifest_root / "release" / component / "source-prep.json"
                manifest_path.parent.mkdir(parents=True)
                manifest_path.write_text("old manifest\n")

                class Args:
                    pass

                Args.component = [component]
                Args.manifest_root = manifest_root
                Args.repo_root = REPO_ROOT
                Args.profile = "release"
                Args.requires = []
                Args.stage = ["source-prep"]

                fsynced_dirs = []
                original_fsync_directory = self.module.fsync_directory

                def recording_fsync_directory(path):
                    fsynced_dirs.append(path)
                    original_fsync_directory(path)

                stdout = io.StringIO()
                try:
                    self.module.fsync_directory = recording_fsync_directory
                    with contextlib.redirect_stdout(stdout):
                        result = self.module.write(Args)
                finally:
                    self.module.fsync_directory = original_fsync_directory

                self.assertEqual(result, 0)
                self.assertIn(f"wrote: {component}:source-prep", stdout.getvalue())
                self.assertEqual("source-prep", json.loads(manifest_path.read_text())["stage"])
                self.assertEqual([manifest_path.parent], fsynced_dirs)
                self.assertEqual([], list(manifest_path.parent.glob(f".{manifest_path.name}.*.tmp")))


if __name__ == "__main__":
    unittest.main()
