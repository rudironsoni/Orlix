import importlib.machinery
import importlib.util
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock


REPO_ROOT = Path(__file__).resolve().parents[2]
TOOL_PATH = REPO_ROOT / "tools" / "orlix-cache-ready"


def load_tool():
    loader = importlib.machinery.SourceFileLoader("orlix_cache_ready", str(TOOL_PATH))
    spec = importlib.util.spec_from_loader("orlix_cache_ready", loader)
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    loader.exec_module(module)
    return module


class CacheReadyTests(unittest.TestCase):
    def setUp(self):
        self.module = load_tool()

    def write_manifest(self, root, profile, component, stage):
        path = Path(root) / "manifests" / profile / component / f"{stage}.json"
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text("{}\n")

    def test_ready_requires_manifest_output_and_clean_audit(self):
        with tempfile.TemporaryDirectory() as tmp:
            self.write_manifest(tmp, "release", "coreutils", "source-prep")
            self.write_manifest(tmp, "release", "coreutils", "configure-build")
            self.write_manifest(tmp, "release", "coreutils", "install-rootfs")
            output = Path(tmp) / "rootfs" / "bin" / "ls"
            output.parent.mkdir(parents=True)
            output.write_text("ls\n")

            with mock.patch.object(self.module, "audit_component", return_value=[]):
                result = self.module.main(
                    [
                        "--repo-root",
                        tmp,
                        "--manifest-root",
                        "manifests",
                        "--profile",
                        "release",
                        "--component",
                        "coreutils",
                        "--requires",
                        str(output),
                    ]
                )

            self.assertEqual(result, 0)

    def test_linux_default_requires_kernel_archive_manifest(self):
        with tempfile.TemporaryDirectory() as tmp:
            manifest_root = Path(tmp) / "manifests"
            linux_manifests = manifest_root / "release" / "linux"
            linux_manifests.mkdir(parents=True)
            for stage in ("source-prep", "headers-install"):
                (linux_manifests / f"{stage}.json").write_text("{}\n")

            with mock.patch.object(self.module, "audit_component", return_value=[]) as audit:
                result = self.module.main(
                    [
                        "--manifest-root",
                        str(manifest_root),
                        "--profile",
                        "release",
                        "--component",
                        "linux",
                    ]
                )

            self.assertEqual(result, 1)
            audit.assert_not_called()

            (linux_manifests / "kernel-archive.json").write_text("{}\n")

            with mock.patch.object(self.module, "audit_component", return_value=[]):
                result = self.module.main(
                    [
                        "--manifest-root",
                        str(manifest_root),
                        "--profile",
                        "release",
                        "--component",
                        "linux",
                    ]
                )

            self.assertEqual(result, 0)

    def test_missing_manifest_fails_before_audit(self):
        with tempfile.TemporaryDirectory() as tmp:
            output = Path(tmp) / "rootfs" / "bin" / "ls"
            output.parent.mkdir(parents=True)
            output.write_text("ls\n")

            with mock.patch.object(self.module, "audit_component") as audit:
                result = self.module.main(
                    [
                        "--repo-root",
                        tmp,
                        "--manifest-root",
                        "manifests",
                        "--profile",
                        "release",
                        "--component",
                        "coreutils",
                        "--requires",
                        str(output),
                    ]
                )

            self.assertEqual(result, 1)
            audit.assert_not_called()

    def test_stale_audit_fails(self):
        with tempfile.TemporaryDirectory() as tmp:
            self.write_manifest(tmp, "release", "coreutils", "source-prep")
            self.write_manifest(tmp, "release", "coreutils", "configure-build")
            self.write_manifest(tmp, "release", "coreutils", "install-rootfs")
            output = Path(tmp) / "rootfs" / "bin" / "ls"
            output.parent.mkdir(parents=True)
            output.write_text("ls\n")

            with mock.patch.object(
                self.module, "audit_component", return_value=["manifest audit failed"]
            ):
                result = self.module.main(
                    [
                        "--repo-root",
                        tmp,
                        "--manifest-root",
                        "manifests",
                        "--profile",
                        "release",
                        "--component",
                        "coreutils",
                        "--requires",
                        str(output),
                    ]
                )

            self.assertEqual(result, 1)

    def test_ready_command_mode_skips_command(self):
        with tempfile.TemporaryDirectory() as tmp:
            self.write_manifest(tmp, "release", "coreutils", "source-prep")
            self.write_manifest(tmp, "release", "coreutils", "configure-build")
            self.write_manifest(tmp, "release", "coreutils", "install-rootfs")
            marker = Path(tmp) / "marker"

            with mock.patch.object(self.module, "audit_component", return_value=[]):
                result = self.module.main(
                    [
                        "--repo-root",
                        tmp,
                        "--manifest-root",
                        "manifests",
                        "--profile",
                        "release",
                        "--component",
                        "coreutils",
                        "--",
                        sys.executable,
                        "-c",
                        f"from pathlib import Path; Path({str(marker)!r}).write_text('ran')",
                    ]
                )

            self.assertEqual(result, 0)
            self.assertFalse(marker.exists())

    def test_not_ready_command_mode_runs_command(self):
        with tempfile.TemporaryDirectory() as tmp:
            marker = Path(tmp) / "marker"

            result = self.module.main(
                [
                    "--repo-root",
                    tmp,
                    "--manifest-root",
                    "manifests",
                    "--profile",
                    "release",
                    "--component",
                    "coreutils",
                    "--",
                    sys.executable,
                    "-c",
                    f"from pathlib import Path; Path({str(marker)!r}).write_text('ran')",
                ]
            )

            self.assertEqual(result, 0)
            self.assertEqual(marker.read_text(), "ran")


if __name__ == "__main__":
    unittest.main()
