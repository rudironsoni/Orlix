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

    def test_default_stages_are_valid_manifest_stages(self):
        manifest_tool = importlib.machinery.SourceFileLoader(
            "orlix_build_manifest_for_cache_ready_test",
            str(REPO_ROOT / "tools" / "orlix-build-manifest"),
        ).load_module()
        manifest_stages = {
            component: {stage.name for stage in stages}
            for component, stages in manifest_tool.STAGES.items()
        }

        for component, default_stages in self.module.DEFAULT_STAGES.items():
            with self.subTest(component=component):
                self.assertLessEqual(set(default_stages), manifest_stages[component])

    def test_linux_default_stages_do_not_require_kselftest_package(self):
        self.assertNotIn("kselftest-package", self.module.DEFAULT_STAGES["linux"])

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
            repo_root = Path(tmp)
            manifest_root = repo_root / "manifests"
            linux_manifests = manifest_root / "release" / "linux"
            linux_manifests.mkdir(parents=True)
            for stage in ("source-prep", "headers-install"):
                (linux_manifests / f"{stage}.json").write_text("{}\n")

            with mock.patch.object(self.module, "audit_component", return_value=[]) as audit:
                result = self.module.main(
                    [
                "--repo-root",
                str(repo_root),
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
                "--repo-root",
                str(repo_root),
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

    def test_unknown_stage_fails_even_when_manifest_exists(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            repo_root = root
            root = Path(tmp)
            manifest_root = root / "manifests"
            self.write_manifest(manifest_root, "release", "linux", "typo")
            args = self.module.parse_args(
                [
                    "--repo-root",
                    str(repo_root),
                    "--manifest-root",
                    str(manifest_root),
                    "--profile",
                    "release",
                    "--component",
                    "linux",
                    "--stage",
                    "typo",
                ]
            )

            errors = self.module.readiness_errors(args)

            self.assertEqual(errors, ["stage-unknown linux typo"])

    def test_selected_stage_is_forwarded_to_manifest_audit(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            manifest_root = root / "manifests"
            self.write_manifest(root, "release", "linux", "source-prep")
            args = self.module.parse_args(
                [
                    "--repo-root",
                    str(root),
                    "--manifest-root",
                    str(manifest_root),
                    "--profile",
                    "release",
                    "--component",
                    "linux",
                    "--stage",
                    "source-prep",
                ]
            )
            seen_commands = []

            def fake_run(command, **_kwargs):
                seen_commands.append(command)
                return self.module.subprocess.CompletedProcess(command, 0, stdout="")

            with mock.patch.object(self.module.subprocess, "run", side_effect=fake_run):
                errors = self.module.readiness_errors(args)

            self.assertEqual(errors, [])
            self.assertEqual(seen_commands[0].count("--stage"), 1)
            self.assertIn("source-prep", seen_commands[0])
            self.assertNotIn("headers-install", seen_commands[0])
            self.assertNotIn("kernel-archive", seen_commands[0])

    def test_duplicate_selected_stage_is_audited_once(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            manifest_root = root / "manifests"
            self.write_manifest(root, "release", "linux", "source-prep")
            args = self.module.parse_args(
                [
                    "--repo-root",
                    str(root),
                    "--manifest-root",
                    str(manifest_root),
                    "--profile",
                    "release",
                    "--component",
                    "linux",
                    "--stage",
                    "source-prep",
                    "--stage",
                    "source-prep",
                ]
            )
            seen_commands = []

            def fake_run(command, **_kwargs):
                seen_commands.append(command)
                return self.module.subprocess.CompletedProcess(command, 0, stdout="")

            with mock.patch.object(self.module.subprocess, "run", side_effect=fake_run):
                errors = self.module.readiness_errors(args)

            self.assertEqual(errors, [])
            self.assertEqual(seen_commands[0].count("--stage"), 1)
            self.assertEqual(seen_commands[0].count("source-prep"), 1)

    def test_manifest_directory_is_not_ready(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            manifest_root = root / "manifests"
            manifest = self.module.manifest_path(root, manifest_root, "release", "linux", "source-prep")
            manifest.mkdir(parents=True)
            args = self.module.parse_args(
                [
                    "--repo-root",
                    str(root),
                    "--manifest-root",
                    str(manifest_root),
                    "--profile",
                    "release",
                    "--component",
                    "linux",
                    "--stage",
                    "source-prep",
                ]
            )

            errors = self.module.readiness_errors(args)

            self.assertIn(f"not-file linux/source-prep manifest: {manifest}", errors)

    def test_required_cache_output_directory_is_not_ready(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            manifest_root = root / "manifests"
            required = root / "cache-output"
            required.mkdir()
            self.write_manifest(root, "release", "linux", "source-prep")
            args = self.module.parse_args(
                [
                    "--repo-root",
                    str(root),
                    "--manifest-root",
                    str(manifest_root),
                    "--profile",
                    "release",
                    "--component",
                    "linux",
                    "--stage",
                    "source-prep",
                    "--requires",
                    str(required),
                ]
            )
            self.module.audit_component = lambda *unused: []

            errors = self.module.readiness_errors(args)

            self.assertEqual(errors, [f"not-file cache output: {required}"])

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



    def test_manifest_root_outside_repo_fails_closed(self):
        with tempfile.TemporaryDirectory() as tmp, tempfile.TemporaryDirectory() as outside_tmp:
            repo_root = Path(tmp)
            manifest_root = Path(outside_tmp) / "manifests"

            args = self.module.parse_args(
                [
                    "--repo-root",
                    str(repo_root),
                    "--manifest-root",
                    str(manifest_root),
                    "--profile",
                    "release",
                    "--component",
                    "linux",
                ]
            )

            errors = self.module.readiness_errors(args)
            self.assertEqual(errors, [f"manifest-root-outside-repo: {manifest_root}"])
    def test_manifest_audit_launch_error_fails_closed(self):
        with tempfile.TemporaryDirectory() as tmp:
            repo_root = Path(tmp)
            manifest_root = repo_root / "manifests"
            for stage in ("source-prep", "headers-install", "kernel-archive"):
                self.write_manifest(repo_root, "release", "linux", stage)

            with mock.patch.object(self.module.subprocess, "run", side_effect=OSError("audit unavailable")):
                result = self.module.main([
                    "--profile",
                    "release",
                    "--component",
                    "linux",
                    "--manifest-root",
                    str(manifest_root),
                ])

            self.assertEqual(result, 1)


if __name__ == "__main__":
    unittest.main()
