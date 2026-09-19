from __future__ import annotations

import json
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import components

ROOT = Path(__file__).resolve().parents[2]
EXPECTED = (
    "uapi",
    "mlibc",
    "rootfs",
    "kernel-release-iphoneos",
    "kernel-release-iphonesimulator",
    "kernel-development-iphoneos",
    "kernel-development-iphonesimulator",
)


class ComponentRegistryTests(unittest.TestCase):
    def test_registry_has_seven_components_with_required_fields(self) -> None:
        registry = components.load_registry()
        self.assertEqual(registry["schema"], 1)
        self.assertEqual([entry["name"] for entry in registry["components"]], list(EXPECTED))

    def test_make_loops_derive_from_registry(self) -> None:
        mk = (ROOT / "make" / "bazel-migration.mk").read_text(encoding="utf-8")
        seven = "uapi mlibc rootfs kernel-release-iphoneos kernel-release-iphonesimulator kernel-development-iphoneos kernel-development-iphonesimulator"
        self.assertNotIn(f"for component in {seven}", mk)
        self.assertGreater(mk.count("components.py"), 2)

    def test_workflow_publish_loop_derives_from_registry(self) -> None:
        workflow = (ROOT / ".github" / "workflows" / "bazel-promote.yml").read_text(encoding="utf-8")
        seven = "uapi mlibc rootfs kernel-release-iphoneos kernel-release-iphonesimulator kernel-development-iphoneos kernel-development-iphonesimulator"
        self.assertNotIn(f"for component in {seven}", workflow)
        self.assertIn("components.py --names", workflow)

    def test_eval_target_lines_match_registry(self) -> None:
        mk = (ROOT / "make" / "bazel-migration.mk").read_text(encoding="utf-8")
        evaluated = set()
        for macro in ("ORLIX_BAZEL_PROMOTE", "ORLIX_BAZEL_PUBLISH", "ORLIX_BAZEL_PROMOTE_KERNEL"):
            for match in re.finditer(rf"call {macro},([a-z0-9-]+)", mk):
                evaluated.add(match.group(1))
        self.assertEqual(evaluated, set(EXPECTED))

    def test_workflow_artifact_list_covers_registry(self) -> None:
        workflow = (ROOT / ".github" / "workflows" / "bazel-promote.yml").read_text(encoding="utf-8")
        for name in EXPECTED:
            self.assertIn(f"Build/Bazel/promote/{name}/{name}-signed.json", workflow)
            self.assertIn(f"Build/Bazel/promote/{name}/{name}-sbom.json", workflow)
            self.assertIn(f"Build/Bazel/promote/{name}/{name}-in-toto.json", workflow)
        self.assertIn("Build/Bazel/promote/toolchain-manifest.json", workflow)
        self.assertIn("Build/Bazel/promote/promotion-proof-index.json", workflow)

    def test_python_component_tuples_match_registry(self) -> None:
        from locked_buildset import KERNEL_COMPONENTS, REQUIRED

        self.assertEqual(set(REQUIRED) | set(KERNEL_COMPONENTS), set(EXPECTED))

    def test_hypothetical_eighth_component_flows_through(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            copy = Path(tmp) / "components.json"
            payload = json.loads(components.registry_path().read_text(encoding="utf-8"))
            payload["components"].append(
                {
                    "name": "hypothetical",
                    "label": "//bazel/feasibility/hypothetical:hypothetical",
                    "digest_path": "bazel/feasibility/hypothetical/hypothetical.artifact-identity-v2.sha256",
                    "manifest_stem": "hypothetical",
                    "product": {"kind": "tree", "subdir": "hypothetical"},
                    "type": "hypothetical",
                }
            )
            copy.write_text(json.dumps(payload), encoding="utf-8")
            names = subprocess.run(
                [sys.executable, str(Path(__file__).resolve().parent / "components.py"),
                 "--registry", str(copy), "--names"],
                check=True, text=True, capture_output=True,
            ).stdout.split()
            self.assertIn("hypothetical", names)
            self.assertEqual(len(names), 8)
            labels = subprocess.run(
                [sys.executable, str(Path(__file__).resolve().parent / "components.py"),
                 "--registry", str(copy), "--labels"],
                check=True, text=True, capture_output=True,
            ).stdout.split()
            self.assertIn("//bazel/feasibility/hypothetical:hypothetical", labels)


if __name__ == "__main__":
    unittest.main()
