from __future__ import annotations

import contextlib
import hashlib
import io
import json
import plistlib
import shutil
import subprocess
import tarfile
import tempfile
import time
import unittest
from pathlib import Path
from unittest import mock

import setup_cache

ROOT = Path(__file__).resolve().parents[2]
WORKFLOWS = (
    "bazel-ci.yml",
    "bazel-promoted-consumer-proof.yml",
    "bazel-benchmark.yml",
    "bazel-nightly.yml",
    "bazel-promote.yml",
)
RUNTIME_WORKFLOWS = (
    "bazel-ci.yml",
    "bazel-promoted-consumer-proof.yml",
)
CACHE_ACTION = "actions/cache/"


def workflow_text(name: str) -> str:
    return (ROOT / ".github" / "workflows" / name).read_text(encoding="utf-8")


def write_bootable_bundle(bundle: Path) -> None:
    mobile = bundle / "Contents" / "Resources" / "RuntimeRoot" / "private" / "var" / "mobile"
    mobile.mkdir(parents=True)
    (bundle / "Contents" / "Info.plist").write_bytes(b"plist")
    (bundle / "Contents" / "Resources" / "RuntimeRoot" / "usr").mkdir()


def cache_blocks(text: str) -> list[str]:
    blocks = []
    marker = "uses: actions/cache/"
    start = 0
    while True:
        found = text.find(marker, start)
        if found < 0:
            break
        end = text.find("\n      - ", found + len(marker))
        blocks.append(text[found:] if end < 0 else text[found:end])
        if end < 0:
            break
        start = end
    return blocks


class SetupCacheDecisionTests(unittest.TestCase):
    def test_two_component_xcode_spec_matches_any_patch(self) -> None:
        self.assertTrue(setup_cache.version_satisfies("26.6", "26.6"))
        self.assertTrue(setup_cache.version_satisfies("26.6.0", "26.6"))
        self.assertTrue(setup_cache.version_satisfies("26.6.1", "26.6"))
        self.assertFalse(setup_cache.version_satisfies("26.5.0", "26.6"))
        self.assertFalse(setup_cache.version_satisfies("26.7.0", "26.6"))
        self.assertFalse(setup_cache.version_satisfies("27.0", "26.6"))

    def test_three_component_xcode_spec_is_exact(self) -> None:
        self.assertTrue(setup_cache.version_satisfies("26.6.0", "26.6.0"))
        self.assertFalse(setup_cache.version_satisfies("26.6.1", "26.6.0"))

    def test_xcode_present_reads_real_app_plists_and_ignores_symlinks(self) -> None:
        with self.subTest("directory"):
            with tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                app = root / "Xcode_26.6.app" / "Contents"
                app.mkdir(parents=True)
                with (app / "version.plist").open("wb") as stream:
                    plistlib.dump({"CFBundleShortVersionString": "26.6"}, stream)
                link = root / "Xcode.app"
                link.symlink_to(root / "Xcode_26.6.app", target_is_directory=True)
                self.assertEqual(setup_cache.installed_xcode_versions(root), ["26.6"])
                self.assertTrue(setup_cache.xcode_present("26.6", root))
                self.assertFalse(setup_cache.xcode_present("27.0", root))

    def test_runtime_on_the_runner_skips_xcodes_install(self) -> None:
        self.assertEqual(setup_cache.runtime_plan(available=True, archive_ready=False), "skip")
        self.assertEqual(setup_cache.runtime_plan(available=True, archive_ready=True), "skip")
        self.assertEqual(
            setup_cache.runtime_commands("skip", "15.5", Path("/tmp/runtime")),
            [],
        )
        self.assertIn("skipping xcodes runtimes install", setup_cache.runtime_message("skip", "26.5", Path("/cache")))

    def test_restored_archive_installs_locally_and_does_not_download(self) -> None:
        archive = Path("/Users/runner/Library/Caches/Orlix/ios15-runtime")
        self.assertEqual(setup_cache.runtime_plan(available=False, archive_ready=True), "install-local")
        command = setup_cache.runtime_commands("install-local", "15.5", archive)[0]
        self.assertEqual(command[:4], ["sudo", "xcodes", "runtimes", "install"])
        self.assertIn(str(archive), command)
        self.assertIn("--keep-archive", command)
        message = setup_cache.runtime_message("install-local", "15.5", archive)
        self.assertIn("not installed", message)
        self.assertIn("not an installed runtime", message)
        self.assertIn("without downloading", message)
        self.assertIn("without copying", message)
        self.assertNotIn("downloading it", message)

    def test_restored_download_archive_is_not_an_installed_runtime(self) -> None:
        """iOS 15.5 stays uninstalled when simctl does not list it.

        The restored download archive does not skip ``xcodes`` and is not copied.
        """
        calls = []

        def fake_run(command, **kwargs):
            calls.append(command)
            if command[:4] == ["xcrun", "simctl", "list", "runtimes"]:
                return subprocess.CompletedProcess(command, 0, stdout='{"runtimes":[]}', stderr="")
            if command[:4] == ["xcrun", "simctl", "runtime", "list"]:
                return subprocess.CompletedProcess(command, 0, stdout='{"runtimes":[]}', stderr="")
            if command[:4] == ["sudo", "xcodes", "runtimes", "install"]:
                return subprocess.CompletedProcess(command, 0)
            raise AssertionError(command)

        with tempfile.TemporaryDirectory() as temporary:
            archive = Path(temporary) / "ios15-runtime"
            archive.mkdir()
            (archive / "runtime.dmg").write_bytes(b"restored-download")
            runtime_dir = Path(temporary) / "Runtimes"
            runtime_dir.mkdir()
            with mock.patch.object(setup_cache.shutil, "copytree", side_effect=AssertionError("copy")):
                with mock.patch.object(setup_cache.subprocess, "run", fake_run):
                    plan = setup_cache.prepare_runtime("15.5", archive, runtime_dir)
        self.assertEqual(plan, "install-local")
        self.assertIn(
            ["sudo", "xcodes", "runtimes", "install", "iOS 15.5"],
            [command[:5] for command in calls],
        )
        self.assertNotIn("skip", plan)

    def test_missing_runtime_downloads_into_the_archive_directory(self) -> None:
        archive = Path("/Users/runner/Library/Caches/Orlix/ios26-runtime")
        self.assertEqual(setup_cache.runtime_plan(available=False, archive_ready=False), "download")
        self.assertIn("downloading it", setup_cache.runtime_message("download", "26.5", archive))

    def test_incomplete_aria2_archive_is_not_reusable(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            image = root / "runtime.dmg"
            image.write_bytes(b"partial")
            (root / "runtime.dmg.aria2").write_text("incomplete", encoding="utf-8")
            self.assertFalse(setup_cache.archive_complete(root))
            image.with_name("runtime.dmg.aria2").unlink()
            self.assertTrue(setup_cache.archive_complete(root))

    def test_simctl_availability_requires_the_exact_version(self) -> None:
        payload = {
            "runtimes": [
                {"version": "26.5", "isAvailable": True},
                {"version": "15.5", "isAvailable": False},
            ]
        }
        self.assertTrue(setup_cache.simctl_runtime_available("26.5", payload))
        self.assertFalse(setup_cache.simctl_runtime_available("15.5", payload))
        self.assertFalse(setup_cache.simctl_runtime_available("18.0", payload))
        patched = {"runtimes": [{"version": "15.5.0", "isAvailable": True, "identifier": "com.apple.CoreSimulator.SimRuntime.iOS-15-5"}]}
        self.assertTrue(setup_cache.simctl_runtime_available("15.5", patched))
        self.assertEqual(
            setup_cache.simctl_runtime_identifier("15.5", patched),
            "com.apple.CoreSimulator.SimRuntime.iOS-15-5",
        )

    def test_permission_error_is_not_a_missing_runtime(self) -> None:
        denied = Path("/Library/Developer/CoreSimulator/Profiles/Runtimes/iOS 15.5.simruntime")
        with mock.patch.object(setup_cache, "path_probe", return_value="permission"):
            self.assertTrue(setup_cache.runtime_bundle_installed("15.5"))
        self.assertEqual(setup_cache.path_probe(denied), "missing")

        def fake_run(command, **kwargs):
            if command[:4] == ["xcrun", "simctl", "list", "runtimes"]:
                raise PermissionError("Permission denied: simctl")
            raise AssertionError(command)

        with tempfile.TemporaryDirectory() as temporary:
            archive = Path(temporary)
            (archive / "runtime.dmg").write_bytes(b"restored-download")
            with mock.patch.object(setup_cache, "runtime_bundle_installed", side_effect=PermissionError("Permission denied")):
                with mock.patch.object(setup_cache.subprocess, "run", fake_run):
                    plan = setup_cache.prepare_runtime("15.5", archive, Path(temporary) / "runtimes")
        self.assertEqual(plan, "skip")
        self.assertEqual(setup_cache.runtime_commands("skip", "15.5", archive), [])

    def test_runtime_list_image_is_not_installed_again(self) -> None:
        """``simctl runtime list`` counts even when ``list runtimes`` omits it."""
        images = json.dumps({
            "runtimes": [{
                "version": "15.5",
                "state": "Ready",
                "name": "iOS 15.5 (19F70)",
                "runtimeIdentifier": "com.apple.CoreSimulator.SimRuntime.iOS-15-5",
                "bundlePath": "/Library/Developer/CoreSimulator/Volumes/iOS_19F70/Library/Developer/CoreSimulator/Profiles/Runtimes/iOS 15.5.simruntime",
            }],
        })
        self.assertTrue(setup_cache.simctl_runtime_listed("15.5", json.loads(images)))
        self.assertTrue(setup_cache.runtime_name_matches("iOS 15.5 (19F70)", "15.5"))
        self.assertFalse(setup_cache.runtime_name_matches("iOS 15.50", "15.5"))

        def fake_run(command, **kwargs):
            if command[:5] == ["xcrun", "simctl", "list", "runtimes", "-j"]:
                return subprocess.CompletedProcess(command, 0, stdout='{"runtimes":[]}', stderr="")
            if command[:5] == ["xcrun", "simctl", "runtime", "list", "-j"]:
                return subprocess.CompletedProcess(command, 0, stdout=images, stderr="")
            raise AssertionError(command)

        with tempfile.TemporaryDirectory() as temporary:
            archive = Path(temporary)
            (archive / "runtime.dmg").write_bytes(b"restored-download")
            with mock.patch.object(setup_cache.shutil, "copytree", side_effect=AssertionError("copy")):
                with mock.patch.object(setup_cache, "runtime_bundle_installed", return_value=False):
                    with mock.patch.object(setup_cache.subprocess, "run", fake_run):
                        plan = setup_cache.prepare_runtime("15.5", archive, Path(temporary) / "runtimes")
        self.assertEqual(plan, "skip")
        self.assertEqual(setup_cache.runtime_commands("skip", "15.5", archive), [])

    def test_listed_unavailable_runtime_is_not_installed(self) -> None:
        payload = json.dumps({
            "runtimes": [{
                "version": "15.5",
                "isAvailable": False,
                "identifier": "com.apple.CoreSimulator.SimRuntime.iOS-15-5",
                "bundlePath": "/Library/Developer/CoreSimulator/Profiles/Runtimes/iOS 15.5.simruntime",
            }],
        })
        parsed = json.loads(payload)
        self.assertTrue(setup_cache.simctl_runtime_listed("15.5", parsed))
        self.assertFalse(setup_cache.simctl_runtime_available("15.5", parsed))
        self.assertEqual(
            setup_cache.simctl_runtime_identifier("15.5", parsed),
            "com.apple.CoreSimulator.SimRuntime.iOS-15-5",
        )

        def fake_run(command, **kwargs):
            if command[:4] == ["xcrun", "simctl", "list", "runtimes"]:
                return subprocess.CompletedProcess(command, 0, stdout=payload, stderr="")
            if command[:4] == ["xcrun", "simctl", "runtime", "list"]:
                return subprocess.CompletedProcess(command, 0, stdout='{"runtimes":[]}', stderr="")
            raise AssertionError(command)

        with tempfile.TemporaryDirectory() as temporary:
            archive = Path(temporary)
            (archive / "runtime.dmg").write_bytes(b"restored-download")
            with mock.patch.object(setup_cache, "runtime_bundle_installed", return_value=False):
                with mock.patch.object(setup_cache.subprocess, "run", fake_run):
                    plan = setup_cache.prepare_runtime("15.5", archive, Path(temporary) / "runtimes")
        self.assertEqual(plan, "skip")

    def test_selected_xcode_skips_the_download(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            developer = Path(temporary) / "Xcode.app" / "Contents" / "Developer"
            developer.mkdir(parents=True)
            with (developer.parent / "version.plist").open("wb") as stream:
                plistlib.dump({"CFBundleShortVersionString": "26.6"}, stream)
            self.assertTrue(setup_cache.xcode_selected("26.6", developer))
            self.assertFalse(setup_cache.xcode_selected("27.0", developer))
            with mock.patch.object(setup_cache, "path_probe", return_value="permission"):
                self.assertTrue(setup_cache.xcode_selected("26.6", developer))

    def test_brewfile_permission_error_is_not_unsatisfied(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            brewfile = Path(temporary) / "Brewfile"
            brewfile.write_text('brew "xcodesorg/made/xcodes"\n', encoding="utf-8")
            prefix = Path(temporary) / "homebrew"
            with mock.patch.object(setup_cache, "declared_tool_status", side_effect=PermissionError("Permission denied")):
                self.assertTrue(setup_cache.bundle_satisfied(brewfile, prefix))

    def test_listed_runtime_is_not_reinstalled_after_the_archive_restores(self) -> None:
        payload = json.dumps({
            "runtimes": [{
                "version": "15.5",
                "isAvailable": True,
                "identifier": "com.apple.CoreSimulator.SimRuntime.iOS-15-5",
                "bundlePath": "/Library/Developer/CoreSimulator/Profiles/Runtimes/iOS 15.5.simruntime",
            }],
        })

        def fake_run(command, **kwargs):
            if command[:4] == ["xcrun", "simctl", "list", "runtimes"]:
                return subprocess.CompletedProcess(command, 0, stdout=payload, stderr="")
            if command[:4] == ["xcrun", "simctl", "runtime", "list"]:
                return subprocess.CompletedProcess(command, 0, stdout='{"runtimes":[]}', stderr="")
            raise AssertionError(command)

        with tempfile.TemporaryDirectory() as temporary:
            archive = Path(temporary)
            (archive / "runtime.dmg").write_bytes(b"restored-download")
            with mock.patch.object(setup_cache.subprocess, "run", fake_run):
                plan = setup_cache.prepare_runtime("15.5", archive, Path(temporary) / "runtimes")
        self.assertEqual(plan, "skip")

    def test_listed_simruntime_is_staged_for_the_cache_save(self) -> None:
        """A runtime simctl lists is not installed again.

        The reported bundle is copied into the Profiles directory the cache
        save uploads. ``simruntime-present`` stays false until those bytes are
        in that directory. A symlink to the simctl path is not enough. A
        restored Profiles bundle is left in place and does not run
        ``xcodes runtimes install``.
        """
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "system" / "iOS 15.5.simruntime"
            (source / "Contents").mkdir(parents=True)
            (source / "Contents" / "Info.plist").write_bytes(b"runtime")
            blocked = source / "Contents" / "Resources" / "RuntimeRoot" / "private" / "var" / "mobile"
            blocked.mkdir(parents=True)
            (blocked / "secret").write_bytes(b"unreadable")
            runtime_dir = root / "Library" / "Developer" / "CoreSimulator" / "Profiles" / "Runtimes"
            archive = root / "ios15-runtime"
            archive.mkdir()
            (archive / "runtime.dmg").write_bytes(b"restored-download")
            images = json.dumps({
                "runtimes": [{
                    "version": "15.5",
                    "state": "Ready",
                    "runtimeIdentifier": "com.apple.CoreSimulator.SimRuntime.iOS-15-5",
                    "bundlePath": str(source),
                }],
            })
            calls = []
            real_iterdir = Path.iterdir

            def fake_run(command, **kwargs):
                calls.append(command)
                if command[:5] == ["xcrun", "simctl", "list", "runtimes", "-j"]:
                    return subprocess.CompletedProcess(command, 0, stdout='{"runtimes":[]}', stderr="")
                if command[:5] == ["xcrun", "simctl", "runtime", "list", "-j"]:
                    return subprocess.CompletedProcess(command, 0, stdout=images, stderr="")
                raise AssertionError(command)

            def guarded_iterdir(self):
                if self.name == "mobile":
                    raise PermissionError(self)
                return real_iterdir(self)

            with mock.patch.object(setup_cache.subprocess, "run", fake_run):
                with mock.patch.object(setup_cache.shutil, "copytree", side_effect=AssertionError("copy")):
                    plan = setup_cache.prepare_runtime("15.5", archive, runtime_dir)
                    self.assertFalse((runtime_dir / "iOS 15.5.simruntime").exists())
                    self.assertFalse(setup_cache.simruntime_present(runtime_dir))
                    with mock.patch.object(Path, "iterdir", guarded_iterdir):
                        setup_cache.stage_listed_simruntime("15.5", runtime_dir)
                        setup_cache.stage_listed_simruntime("26.5", runtime_dir)
            self.assertEqual(plan, "skip")
            self.assertFalse(any(command[:4] == ["sudo", "xcodes", "runtimes", "install"] for command in calls))
            self.assertFalse((runtime_dir / "iOS 26.5.simruntime").exists())
            staged = runtime_dir / "iOS 15.5.simruntime"
            self.assertFalse(staged.exists())
            self.assertFalse(setup_cache.simruntime_present(runtime_dir))

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "installed" / "iOS 15.5.simruntime"
            mobile = source / "Contents" / "Resources" / "RuntimeRoot" / "private" / "var" / "mobile"
            mobile.mkdir(parents=True)
            (source / "Contents" / "Info.plist").write_bytes(b"runtime")
            (source / "Contents" / "Resources" / "RuntimeRoot" / "usr").mkdir()
            runtime_dir = root / "Profiles" / "Runtimes"
            images = json.dumps({
                "runtimes": [{
                    "version": "15.5",
                    "state": "Ready",
                    "runtimeIdentifier": "com.apple.CoreSimulator.SimRuntime.iOS-15-5",
                    "bundlePath": str(source),
                }],
            })

            def fake_run(command, **kwargs):
                if command[:4] == ["xcrun", "simctl", "list", "runtimes"]:
                    return subprocess.CompletedProcess(command, 0, stdout='{"runtimes":[]}', stderr="")
                if command[:4] == ["xcrun", "simctl", "runtime", "list"]:
                    return subprocess.CompletedProcess(command, 0, stdout=images, stderr="")
                raise AssertionError(command)

            with mock.patch.object(setup_cache.subprocess, "run", fake_run):
                setup_cache.stage_listed_simruntime("15.5", runtime_dir)
            staged = runtime_dir / "iOS 15.5.simruntime"
            plist = staged / "Contents" / "Info.plist"
            self.assertFalse(staged.is_symlink())
            self.assertTrue(staged.is_dir())
            self.assertEqual(plist.read_bytes(), b"runtime")
            self.assertNotEqual(plist.stat().st_ino, (source / "Contents" / "Info.plist").stat().st_ino)
            self.assertTrue((staged / "Contents" / "Resources" / "RuntimeRoot" / "private" / "var" / "mobile").is_dir())
            self.assertTrue(setup_cache.simruntime_present(runtime_dir))
            shutil.rmtree(staged)
            staged.symlink_to(source, target_is_directory=True)
            self.assertFalse(setup_cache.simruntime_present(runtime_dir))

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            runtime_dir = root / "Runtimes"
            restored = runtime_dir / "iOS 26.5.simruntime" / "Contents"
            restored.mkdir(parents=True)
            (restored / "Info.plist").write_bytes(b"restored")
            archive = root / "ios26-runtime"
            archive.mkdir()
            (archive / "runtime.dmg").write_bytes(b"restored-download")
            calls = []

            def fake_run(command, **kwargs):
                calls.append(command)
                if command[:2] == ["killall", "-9"]:
                    return subprocess.CompletedProcess(command, 0)
                if command[:4] == ["xcrun", "simctl", "list", "runtimes"]:
                    return subprocess.CompletedProcess(command, 0, stdout='{"runtimes":[]}', stderr="")
                if command[:4] == ["xcrun", "simctl", "runtime", "list"]:
                    return subprocess.CompletedProcess(command, 0, stdout='{"runtimes":[]}', stderr="")
                raise AssertionError(command)

            with mock.patch.object(setup_cache, "user_runtime_profiles", return_value=root / "profiles"):
                with mock.patch.object(setup_cache.subprocess, "run", fake_run):
                    plan = setup_cache.prepare_runtime("26.5", archive, runtime_dir)
            self.assertEqual(plan, "skip")
            self.assertFalse(any(command[:4] == ["sudo", "xcodes", "runtimes", "install"] for command in calls))
            self.assertFalse(any(command[:1] == ["killall"] for command in calls))
            self.assertFalse((runtime_dir / "iOS 26.5.simruntime").is_symlink())
            self.assertEqual(
                (runtime_dir / "iOS 26.5.simruntime" / "Contents" / "Info.plist").read_bytes(),
                b"restored",
            )
            self.assertFalse(setup_cache.simruntime_present(runtime_dir))

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            runtime_dir = root / "Profiles" / "Runtimes"
            runtime_dir.mkdir(parents=True)
            link = runtime_dir / "iOS 15.5.simruntime"
            link.symlink_to(root / "missing-image-runtime", target_is_directory=True)
            self.assertFalse(setup_cache.simruntime_present(runtime_dir))

    def test_image_runtime_is_not_copied_into_profiles(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "Volumes" / "iOS_26.5.simruntime"
            (source / "Contents" / "Resources" / "RuntimeRoot").mkdir(parents=True)
            (source / "Contents" / "Info.plist").write_bytes(b"image")
            (source / "Contents" / "Resources" / "RuntimeRoot" / "usr").mkdir()
            runtime_dir = root / "Library" / "Developer" / "CoreSimulator" / "Profiles" / "Runtimes"
            images = json.dumps({
                "runtimes": [{
                    "version": "26.5",
                    "state": "Ready",
                    "runtimeIdentifier": "com.apple.CoreSimulator.SimRuntime.iOS-26-5",
                    "bundlePath": str(source),
                }],
            })

            def fake_run(command, **kwargs):
                if command[:4] == ["xcrun", "simctl", "list", "runtimes"]:
                    return subprocess.CompletedProcess(command, 0, stdout='{"runtimes":[]}', stderr="")
                if command[:4] == ["xcrun", "simctl", "runtime", "list"]:
                    return subprocess.CompletedProcess(command, 0, stdout=images, stderr="")
                raise AssertionError(command)

            with mock.patch.object(setup_cache.subprocess, "run", fake_run):
                setup_cache.stage_listed_simruntime("26.5", runtime_dir)
            self.assertFalse((runtime_dir / "iOS 26.5.simruntime").exists())
            self.assertFalse(setup_cache.simruntime_present(runtime_dir))

    def test_missing_runtime_boot_fails_on_the_first_attempt(self) -> None:
        calls: list[list[str]] = []

        def missing_runtime(command, **kwargs):
            calls.append(list(command))
            return subprocess.CompletedProcess(
                command, 2, stdout="", stderr="Unable to find a runtime matching the device\n",
            )

        with mock.patch.object(setup_cache.subprocess, "run", missing_runtime):
            with self.assertRaises(SystemExit) as raised:
                setup_cache.boot_simulator("device")
        self.assertEqual(raised.exception.code, 2)
        self.assertEqual(calls, [["xcrun", "simctl", "boot", "device"]])

    def test_restored_simruntime_is_linked_without_an_install_command(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            partial = root / "partial" / "iOS 15.5.simruntime" / "Contents"
            partial.mkdir(parents=True)
            (partial / "Info.plist").write_bytes(b"plist")
            profiles = root / "Profiles" / "Runtimes"
            self.assertEqual(setup_cache.expose_restored_runtimes(root / "partial", profiles), [])
            self.assertFalse((profiles / "iOS 15.5.simruntime").exists())
            write_bootable_bundle(root / "cache" / "iOS 15.5.simruntime")
            write_bootable_bundle(root / "cache" / "iOS 26.5.simruntime")
            linked = setup_cache.expose_restored_runtimes(root / "cache", profiles)
            self.assertEqual(linked, [profiles / "iOS 15.5.simruntime"])
            self.assertTrue((profiles / "iOS 15.5.simruntime").is_symlink())
            self.assertFalse((profiles / "iOS 26.5.simruntime").exists())
            self.assertEqual(setup_cache.runtime_commands("skip", "15.5", root / "cache"), [])

    def test_bootable_archive_skips_xcodes_and_leaves_the_dmg(self) -> None:
        plan = setup_cache.ios15_cache_plan(bootable_archive=True, dmg_archive=True, bundle_bootable=True)
        self.assertEqual(plan["xcodes"], "skip")
        self.assertEqual(plan["dmg"], "leave")
        self.assertEqual(plan["save_bootable_archive"], "no")
        self.assertEqual(setup_cache.BOOTABLE_SIMRUNTIME_CACHE_KEY, "orlix-ios15-simruntime-bootable-v1")
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            runtime_dir = root / "Runtimes"
            write_bootable_bundle(runtime_dir / "iOS 15.5.simruntime")
            archive = root / "ios15-runtime"
            archive.mkdir()
            (archive / "runtime.dmg").write_bytes(b"dmg")
            with mock.patch.object(setup_cache, "user_runtime_profiles", return_value=runtime_dir):
                with mock.patch.object(setup_cache, "archive_complete", side_effect=AssertionError("dmg")):
                    with mock.patch.object(setup_cache.subprocess, "run", side_effect=AssertionError("run")):
                        result = setup_cache.prepare_runtime("15.5", archive, runtime_dir)
            self.assertEqual(result, "skip")
            self.assertEqual((archive / "runtime.dmg").read_bytes(), b"dmg")

    def test_dmg_without_bootable_archive_installs_locally(self) -> None:
        plan = setup_cache.ios15_cache_plan(bootable_archive=False, dmg_archive=True, bundle_bootable=False)
        self.assertEqual(plan["xcodes"], "install-local")
        self.assertEqual(plan["dmg"], "restore")
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            archive = root / "ios15-runtime"
            archive.mkdir()
            (archive / "runtime.dmg").write_bytes(b"dmg")
            calls: list[list[str]] = []

            def fake_run(command, **kwargs):
                calls.append(list(command))
                if command[:4] == ["xcrun", "simctl", "list", "runtimes"]:
                    return subprocess.CompletedProcess(command, 0, stdout='{"runtimes":[]}', stderr="")
                if command[:4] == ["xcrun", "simctl", "runtime", "list"]:
                    return subprocess.CompletedProcess(command, 0, stdout='{"runtimes":[]}', stderr="")
                if command[:4] == ["sudo", "xcodes", "runtimes", "install"]:
                    return subprocess.CompletedProcess(command, 0)
                raise AssertionError(command)

            with mock.patch.object(setup_cache, "user_runtime_profiles", return_value=root / "profiles"):
                with mock.patch.object(setup_cache.subprocess, "run", fake_run):
                    result = setup_cache.prepare_runtime("15.5", archive, root / "Runtimes")
            self.assertEqual(result, "install-local")
            self.assertEqual(calls[-1][:4], ["sudo", "xcodes", "runtimes", "install"])
            self.assertIn(str(archive), calls[-1])
            self.assertFalse(any(command[:2] == ["sudo", "tar"] for command in calls))

    def test_partial_tree_is_not_saved(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            bundle = root / "Runtimes" / "iOS 15.5.simruntime"
            (bundle / "Contents" / "Resources" / "RuntimeRoot" / "usr").mkdir(parents=True)
            (bundle / "Contents" / "Info.plist").write_bytes(b"partial")
            archive_dir = root / "bootable"
            self.assertFalse(setup_cache.simruntime_bootable(bundle))
            self.assertFalse(setup_cache.bootable_archive_save_allowed(bundle, already_cached=False))
            with mock.patch.object(setup_cache, "listed_bundle_path", return_value=None):
                with mock.patch.object(setup_cache, "run_checked", side_effect=AssertionError("tar")):
                    saved = setup_cache.archive_installed_simruntime("15.5", archive_dir, bundle.parent)
            self.assertFalse(saved)
            self.assertFalse(setup_cache.bootable_archive_ready(archive_dir))
            self.assertFalse(archive_dir.exists())

    def test_bootable_bundle_is_archived_with_sudo_tar(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            runtime_dir = root / "Runtimes"
            write_bootable_bundle(runtime_dir / "iOS 15.5.simruntime")
            write_bootable_bundle(runtime_dir / "iOS 26.5.simruntime")
            archive_dir = root / "bootable"
            calls: list[list[str]] = []

            def fake_run(command):
                calls.append(list(command))
                if command[:2] == ["sudo", "tar"] and "-cf" in command:
                    destination = Path(command[command.index("-cf") + 1])
                    destination.parent.mkdir(parents=True, exist_ok=True)
                    destination.write_bytes(b"archive")

            with mock.patch.object(setup_cache, "run_checked", fake_run):
                self.assertFalse(setup_cache.archive_installed_simruntime("26.5", archive_dir, runtime_dir))
                self.assertTrue(setup_cache.archive_installed_simruntime("15.5", archive_dir, runtime_dir))
                self.assertTrue(setup_cache.archive_installed_simruntime("15.5", archive_dir, runtime_dir))
            create = next(command for command in calls if command[:2] == ["sudo", "tar"])
            self.assertNotIn("-h", create)
            self.assertNotIn("--dereference", create)
            self.assertIn("iOS 15.5.simruntime", create)
            self.assertNotIn("iOS 26.5.simruntime", create)
            self.assertEqual(calls.count(create), 1)
            self.assertEqual(calls[1][:2], ["sudo", "chown"])
            self.assertTrue(setup_cache.bootable_archive_ready(archive_dir))

    def test_extract_restores_only_the_ios15_member(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            archive_dir = root / "bootable"
            archive_dir.mkdir()
            setup_cache.bootable_archive_file(archive_dir).write_bytes(b"archive")
            runtime_dir = root / "Runtimes"
            calls: list[list[str]] = []

            def fake_run(command):
                calls.append(list(command))
                self.assertEqual(command[:2], ["sudo", "tar"])
                self.assertIn("-xf", command)
                self.assertNotIn("iOS 26.5.simruntime", command)
                write_bootable_bundle(runtime_dir / command[-1])

            with mock.patch.object(setup_cache, "run_checked", fake_run):
                self.assertTrue(setup_cache.extract_bootable_simruntime(archive_dir, runtime_dir))
            self.assertEqual(calls[0][-1], "iOS 15.5.simruntime")
            self.assertTrue(setup_cache.simruntime_bootable(runtime_dir / "iOS 15.5.simruntime"))
            self.assertFalse((runtime_dir / "iOS 26.5.simruntime").exists())

    def test_brewfile_formulae_keep_the_pinned_xcodes_tap(self) -> None:
        text = (ROOT / "Brewfile").read_text(encoding="utf-8")
        formulae = setup_cache.brewfile_formulae(text)
        self.assertIn("xcodesorg/made/xcodes", formulae)
        self.assertIn("llvm", formulae)
        self.assertIn("ccache", formulae)
        self.assertNotIn("xcodesorg/made", formulae)

    def test_only_source_built_formulae_are_bottled(self) -> None:
        self.assertTrue(setup_cache.formula_was_source_built({
            "formulae": [{"installed": [{"poured_from_bottle": False}]}],
        }))
        self.assertFalse(setup_cache.formula_was_source_built({
            "formulae": [{"installed": [{"poured_from_bottle": True}]}],
        }))
        self.assertFalse(setup_cache.formula_was_source_built({"formulae": [{"installed": []}]}))
        self.assertFalse(setup_cache.formula_was_source_built({"formulae": []}))

    def test_compiler_identity_includes_both_clang_binaries_and_the_xcode_build(self) -> None:
        first = setup_cache.compiler_identity_digest(
            xcode_build="17F113", xcode_clang=b"apple-clang", llvm_clang=b"llvm-clang",
        )
        changed_compiler = setup_cache.compiler_identity_digest(
            xcode_build="17F113", xcode_clang=b"apple-clang", llvm_clang=b"other-clang",
        )
        changed_build = setup_cache.compiler_identity_digest(
            xcode_build="27A266a", xcode_clang=b"apple-clang", llvm_clang=b"llvm-clang",
        )
        self.assertEqual(len(first), 64)
        self.assertNotEqual(first, changed_compiler)
        self.assertNotEqual(first, changed_build)
        self.assertNotIn("buildbuddy", json.dumps({"kind": "pinned-compiler-identity"}))

    def test_observed_compiler_digest_hashes_the_selected_compilers(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            developer = root / "Xcode.app" / "Contents" / "Developer"
            clang = developer / "Toolchains/XcodeDefault.xctoolchain/usr/bin/clang"
            clang.parent.mkdir(parents=True)
            clang.write_bytes(b"apple")
            with (developer.parent / "version.plist").open("wb") as stream:
                plistlib.dump({"ProductBuildVersion": "17F113", "CFBundleShortVersionString": "26.6"}, stream)
            llvm = root / "llvm-clang"
            llvm.write_bytes(b"llvm")
            original = setup_cache.Path
            setup_cache.Path = lambda value: llvm if value == "/opt/homebrew/opt/llvm/bin/clang" else original(value)
            try:
                digest = setup_cache.observed_compiler_digest(developer)
            finally:
                setup_cache.Path = original
            expected = setup_cache.compiler_identity_digest(
                xcode_build="17F113", xcode_clang=b"apple", llvm_clang=b"llvm",
            )
            self.assertEqual(digest, expected)

    def test_ci_brewfile_pins_xcodes_from_the_comment(self) -> None:
        entries = dict(setup_cache.brewfile_entries((ROOT / "bazel/ci/Brewfile").read_text(encoding="utf-8")))
        self.assertEqual(entries["xcodesorg/made/xcodes"], "2.1.0")
        self.assertIsNone(entries["llvm"])
        self.assertNotIn("actionlint", entries)
        self.assertNotIn("cosign", entries)

    def test_formula_version_reads_the_llvm_archive_url(self) -> None:
        text = (
            "class Llvm < Formula\n"
            '  head "https://github.com/llvm/llvm-project.git", branch: "main"\n'
            "  stable do\n"
            '    url "https://github.com/llvm/llvm-project/releases/download/'
            'llvmorg-23.1.3/llvm-project-23.1.3.src.tar.xz"\n'
            "  end\n"
            "  bottle do\n"
            "  end\n"
            "end\n"
        )
        self.assertEqual(setup_cache.formula_declared_version(text), "23.1.3")
        self.assertTrue(setup_cache.formula_version_matches("23.1.3_1", "23.1.3"))
        self.assertFalse(setup_cache.formula_version_matches("20.1.8", "23.1.3"))

    def test_libtool_formula_is_read_from_the_lib_shard(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            prefix = Path(temporary)
            path = (
                prefix / "Library" / "Taps" / "homebrew" / "homebrew-core"
                / "Formula" / "lib" / "libtool.rb"
            )
            path.parent.mkdir(parents=True)
            path.write_text('class Libtool < Formula\n  version "2.6.2"\nend\n', encoding="utf-8")
            self.assertEqual(setup_cache.formula_version_from_prefix(prefix, "libtool"), "2.6.2")

    def test_pinned_tools_skip_brew_without_spawning_brew(self) -> None:
        brewfile_text = (ROOT / "bazel/ci/Brewfile").read_text(encoding="utf-8")
        entries = setup_cache.brewfile_entries(brewfile_text)

        def forbid_brew(*_args, **_kwargs):
            raise AssertionError("brew must not run when the pinned kegs are installed")

        with tempfile.TemporaryDirectory() as temporary:
            prefix = Path(temporary)
            brewfile = prefix / "Brewfile"
            brewfile.write_text(brewfile_text, encoding="utf-8")
            for name, pinned in entries:
                version = pinned or "23.1.3"
                write_formula(prefix, name, version)
                write_keg(prefix, name, version, clang=setup_cache.formula_short_name(name) == "llvm")
            (prefix / "etc").mkdir()
            (prefix / "etc" / "mke2fs.conf").write_text("[defaults]\n", encoding="utf-8")
            started = time.perf_counter()
            with mock.patch.object(setup_cache.subprocess, "run", forbid_brew):
                satisfied = setup_cache.bundle_satisfied(brewfile, prefix)
            elapsed = time.perf_counter() - started
        self.assertTrue(satisfied)
        self.assertLess(elapsed, 1.0, f"pinned tool check took {elapsed:.4f}s")

    def test_missing_compiler_still_requires_brew_bundle(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            prefix = Path(temporary)
            brewfile = prefix / "Brewfile"
            brewfile.write_text('brew "llvm"\nbrew "make"\n', encoding="utf-8")
            write_formula(prefix, "llvm", "23.1.3")
            write_formula(prefix, "make", "4.4.1")
            write_keg(prefix, "make", "4.4.1")
            self.assertFalse(setup_cache.bundle_satisfied(brewfile, prefix))

    def test_runner_llvm20_does_not_replace_the_pinned_clang(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            prefix = Path(temporary)
            brewfile = prefix / "Brewfile"
            brewfile.write_text('brew "llvm"\n', encoding="utf-8")
            write_formula(
                prefix,
                "llvm",
                "23.1.3",
                url=(
                    '  url "https://github.com/llvm/llvm-project/releases/download/'
                    'llvmorg-23.1.3/llvm-project-23.1.3.src.tar.xz"\n'
                ),
            )
            write_keg(prefix, "llvm@20", "20.1.8", clang=True)
            self.assertFalse(setup_cache.bundle_satisfied(brewfile, prefix))
            aliased = prefix / "opt" / "llvm"
            aliased.symlink_to(Path("..") / "Cellar" / "llvm@20" / "20.1.8")
            self.assertFalse(setup_cache.bundle_satisfied(brewfile, prefix))
            write_keg(prefix, "llvm", "20.1.8", clang=True)
            self.assertFalse(setup_cache.bundle_satisfied(brewfile, prefix))
            write_keg(prefix, "llvm", "23.1.3", clang=True)
            self.assertFalse(setup_cache.bundle_satisfied(brewfile, prefix))
            (prefix / "etc").mkdir()
            (prefix / "etc" / "mke2fs.conf").write_text("[defaults]\n", encoding="utf-8")
            self.assertTrue(setup_cache.bundle_satisfied(brewfile, prefix))

    def test_recorded_versions_are_the_toolchain_cache_key_suffix(self) -> None:
        brewfile = ROOT / "bazel/ci/Brewfile"
        with tempfile.TemporaryDirectory() as temporary:
            prefix = Path(temporary)
            for name, pinned in setup_cache.brewfile_entries(brewfile.read_text(encoding="utf-8")):
                write_formula(prefix, name, pinned or "23.1.3")
            record = setup_cache.pinned_tool_versions(brewfile, prefix)
            pin = setup_cache.toolchain_pin_suffix(record)
            digest = setup_cache.brewfile_digest(brewfile)
            key = setup_cache.toolchain_cache_key(digest, pin)
        self.assertEqual(record["llvm"], "23.1.3")
        self.assertEqual(record["xcodesorg/made/xcodes"], "2.1.0")
        self.assertEqual(digest, hashlib.sha256(brewfile.read_bytes()).hexdigest())
        self.assertTrue(pin.startswith("llvm-23.1.3-"))
        self.assertNotIn("20.1.8", pin)
        self.assertNotIn("llvm@20", key)
        self.assertEqual(key, f"orlix-ci-toolchain-{digest}-{pin}")
        changed = dict(record)
        changed["llvm"] = "23.1.4"
        self.assertNotEqual(setup_cache.toolchain_pin_suffix(changed), pin)

    def test_toolchain_archive_restores_the_pinned_clang_only(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            prefix = root / "prefix"
            brewfile = root / "Brewfile"
            brewfile.write_text(
                'brew "llvm"\nbrew "make"\n# xcodes 2.1.0 from tap example\nbrew "xcodesorg/made/xcodes"\n',
                encoding="utf-8",
            )
            write_formula(prefix, "llvm", "23.1.3")
            write_formula(prefix, "make", "4.4.1")
            write_keg(prefix, "llvm", "23.1.3", clang=True)
            clang = prefix / "Cellar" / "llvm" / "23.1.3" / "bin" / "clang"
            clang.write_bytes(b"pinned-clang" * 1024)
            write_keg(prefix, "make", "4.4.1")
            (prefix / "Cellar" / "make" / "4.4.1" / "bin" / "gmake").write_bytes(b"gmake")
            (prefix / "bin").mkdir()
            (prefix / "bin" / "gmake").symlink_to(Path("..") / "opt" / "make" / "bin" / "gmake")
            (prefix / "bin" / "local-tool").write_bytes(b"not-a-brewfile-tool")
            write_keg(prefix, "xcodes", "2.1.0")
            write_keg(prefix, "zstd", "1.5.7")
            write_keg(prefix, "llvm@20", "20.1.8", clang=True)
            write_keg(prefix, "git", "2.50.1")
            archive = root / "ci-toolchain.tar.gz"
            pin = setup_cache.toolchain_pin_suffix(setup_cache.pinned_tool_versions(brewfile, prefix))
            started = time.perf_counter()
            self.assertTrue(setup_cache.pack_toolchain(
                brewfile, archive, prefix, pin=pin, dependencies=["zstd", "llvm@20"],
            ))
            with tarfile.open(archive, "r") as bundle:
                names = [setup_cache.normalize_tar_name(member.name) for member in bundle.getmembers()]
            self.assertIn("Cellar/llvm/23.1.3/bin/clang", names)
            self.assertIn("opt/llvm", names)
            self.assertIn("Cellar/zstd/1.5.7", names)
            self.assertIn("bin/gmake", names)
            self.assertNotIn("bin/local-tool", names)
            self.assertFalse(any("llvm@20" in name for name in names))
            self.assertFalse(any(name == "Cellar" or name.startswith("Cellar/git/") or name == "opt" for name in names))
            self.assertNotIn("/opt/homebrew", names)
            shutil.rmtree(prefix / "Cellar" / "llvm")
            (prefix / "opt" / "llvm").unlink()
            (prefix / "opt" / "llvm").symlink_to(Path("..") / "Cellar" / "llvm@20" / "20.1.8")
            (prefix / "etc").mkdir()
            (prefix / "etc" / "mke2fs.conf").write_text("[defaults]\n", encoding="utf-8")
            self.assertTrue(setup_cache.restore_toolchain(archive, brewfile, prefix, pin=pin))
            elapsed = time.perf_counter() - started
            print(f"local toolchain fixture pack+restore took {elapsed:.4f}s")
            llvm_keg = prefix / "Cellar" / "llvm" / "23.1.3"
            self.assertTrue(setup_cache.pinned_llvm_clang(prefix, llvm_keg))
            self.assertEqual((prefix / "opt" / "llvm" / "bin" / "clang").read_bytes(), b"pinned-clang" * 1024)
            self.assertTrue((prefix / "Cellar" / "llvm@20" / "20.1.8" / "bin" / "clang").is_file())
            self.assertFalse((prefix / CI_TOOLCHAIN_MANIFEST).exists())
            self.assertTrue((prefix / "etc" / "mke2fs.conf").is_file())
            self.assertLess(elapsed, 5.0, f"fixture pack+restore took {elapsed:.4f}s")

    def test_missing_mke2fs_conf_rejects_the_restored_toolchain(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            prefix = root / "prefix"
            brewfile = root / "Brewfile"
            brewfile.write_text('brew "llvm"\n', encoding="utf-8")
            write_formula(prefix, "llvm", "23.1.3")
            write_keg(prefix, "llvm", "23.1.3", clang=True)
            write_keg(prefix, "llvm@20", "20.1.8", clang=True)
            archive = root / "ci-toolchain.tar.gz"
            pin = setup_cache.toolchain_pin_suffix(setup_cache.pinned_tool_versions(brewfile, prefix))
            self.assertTrue(setup_cache.pack_toolchain(brewfile, archive, prefix, pin=pin, dependencies=[]))
            self.assertFalse((prefix / "etc" / "mke2fs.conf").exists())
            stderr = io.StringIO()
            with contextlib.redirect_stderr(stderr):
                restored = setup_cache.restore_toolchain(archive, brewfile, prefix, pin=pin)
            detail = stderr.getvalue()
            self.assertFalse(restored)
            self.assertIn("restored CI toolchain has no mke2fs.conf", detail)
            self.assertNotIn("restored clang is not", detail)
            self.assertTrue((prefix / "Cellar" / "llvm" / "23.1.3").is_dir())
            self.assertTrue((prefix / "opt" / "llvm" / "bin" / "clang").is_file())
            self.assertTrue((prefix / "Cellar" / "llvm@20" / "20.1.8" / "bin" / "clang").is_file())
            self.assertFalse(setup_cache.bundle_satisfied(brewfile, prefix))

    def test_mke2fs_conf_is_linked_from_the_e2fsprogs_keg(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            prefix = root / "prefix"
            brewfile = root / "Brewfile"
            brewfile.write_text('brew "llvm"\n', encoding="utf-8")
            write_formula(prefix, "llvm", "23.1.3")
            write_keg(prefix, "llvm", "23.1.3", clang=True)
            archive = root / "ci-toolchain.tar.gz"
            pin = setup_cache.toolchain_pin_suffix(setup_cache.pinned_tool_versions(brewfile, prefix))
            self.assertTrue(setup_cache.pack_toolchain(brewfile, archive, prefix, pin=pin, dependencies=[]))
            source = prefix / "opt" / "e2fsprogs" / "etc" / "mke2fs.conf"
            source.parent.mkdir(parents=True)
            source.write_text("[defaults]\n", encoding="utf-8")
            self.assertTrue(setup_cache.restore_toolchain(archive, brewfile, prefix, pin=pin))
            conf = prefix / "etc" / "mke2fs.conf"
            self.assertTrue(conf.is_symlink())
            self.assertEqual(conf.read_text(encoding="utf-8"), "[defaults]\n")
            self.assertTrue((prefix / "Cellar" / "llvm" / "23.1.3").is_dir())

    def test_wrong_clang_identity_is_removed_and_llvm20_stays(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            prefix = root / "prefix"
            brewfile = root / "Brewfile"
            brewfile.write_text('brew "llvm"\n', encoding="utf-8")
            write_formula(prefix, "llvm", "23.1.3")
            write_keg(prefix, "llvm", "23.1.3", clang=True)
            write_keg(prefix, "llvm@20", "20.1.8", clang=True)
            archive = root / "ci-toolchain.tar.gz"
            pin = setup_cache.toolchain_pin_suffix(setup_cache.pinned_tool_versions(brewfile, prefix))
            self.assertTrue(setup_cache.pack_toolchain(brewfile, archive, prefix, pin=pin, dependencies=[]))
            replace_tar_bytes(archive, "Cellar/llvm/23.1.3/bin/clang", b"not-the-pinned-clang")
            self.assertFalse(setup_cache.restore_toolchain(archive, brewfile, prefix, pin=pin))
            self.assertFalse((prefix / "Cellar" / "llvm" / "23.1.3").exists())
            self.assertFalse((prefix / "opt" / "llvm").exists())
            self.assertTrue((prefix / "Cellar" / "llvm@20" / "20.1.8" / "bin" / "clang").is_file())
            self.assertFalse(setup_cache.bundle_satisfied(brewfile, prefix))

    def test_prefix_wide_archive_is_not_restored(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            prefix = root / "prefix"
            brewfile = root / "Brewfile"
            brewfile.write_text('brew "llvm"\n', encoding="utf-8")
            write_formula(prefix, "llvm", "23.1.3")
            write_keg(prefix, "llvm", "23.1.3", clang=True)
            write_keg(prefix, "llvm@20", "20.1.8", clang=True)
            archive = root / "ci-toolchain.tar.gz"
            pin = setup_cache.toolchain_pin_suffix(setup_cache.pinned_tool_versions(brewfile, prefix))
            self.assertTrue(setup_cache.pack_toolchain(brewfile, archive, prefix, pin=pin, dependencies=[]))
            append_tar_member(archive, "Cellar/git/2.50.1/README", b"unrelated")
            self.assertFalse(setup_cache.restore_toolchain(archive, brewfile, prefix, pin=pin))
            self.assertFalse((prefix / "Cellar" / "git").exists())
            self.assertTrue((prefix / "Cellar" / "llvm@20" / "20.1.8").is_dir())
            self.assertTrue(setup_cache.pinned_llvm_clang(prefix, prefix / "Cellar" / "llvm" / "23.1.3"))

    def test_toolchain_budget_keeps_the_bootable_runtime(self) -> None:
        self.assertEqual(setup_cache.BOOTABLE_SIMRUNTIME_CACHE_BYTES, 5126954916)
        self.assertEqual(setup_cache.BOOTABLE_SIMRUNTIME_CACHE_KEY, "orlix-ios15-simruntime-bootable-v1")
        members = setup_cache.CI_TOOLCHAIN_MEMBER_BYTES
        bottles = setup_cache.CI_TOOLCHAIN_BOTTLE_BYTES
        runtime = setup_cache.BOOTABLE_SIMRUNTIME_CACHE_BYTES
        self.assertEqual(members, 2_349_225_560)
        self.assertEqual(bottles, 582_115_184)
        self.assertLess(runtime + members, setup_cache.ACTIONS_CACHE_QUOTA_BYTES)
        plan = setup_cache.cache_plan(members)
        self.assertEqual(plan["drop_ccache"], "false")
        self.assertEqual(plan["drop_repository"], "false")
        self.assertEqual(setup_cache.CCACHE_CACHE_BYTES, 97_654_617)
        self.assertEqual(setup_cache.BAZEL_REPOSITORY_CACHE_BYTES, 571_396_067)
        crowded = setup_cache.cache_plan(4_300_000_000)
        self.assertEqual(crowded["drop_ccache"], "true")
        self.assertEqual(crowded["drop_repository"], "false")
        tighter = setup_cache.cache_plan(4_500_000_000)
        self.assertEqual(tighter["drop_ccache"], "false")
        self.assertEqual(tighter["drop_repository"], "true")

    def test_archive_over_the_bootable_budget_is_not_saved(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            prefix = root / "prefix"
            brewfile = root / "Brewfile"
            brewfile.write_text('brew "llvm"\nbrew "make"\n', encoding="utf-8")
            write_formula(prefix, "llvm", "23.1.3")
            write_formula(prefix, "make", "4.4.1")
            write_keg(prefix, "llvm", "23.1.3", clang=True)
            (prefix / "Cellar" / "llvm" / "23.1.3" / "bin" / "clang").write_bytes(b"x" * 64)
            write_keg(prefix, "make", "4.4.1")
            (prefix / "Cellar" / "make" / "4.4.1" / "bin" / "gmake").write_bytes(b"m")
            archive = root / "ci-toolchain.tar.gz"
            pin = setup_cache.toolchain_pin_suffix(setup_cache.pinned_tool_versions(brewfile, prefix))
            self.assertTrue(setup_cache.pack_toolchain(brewfile, archive, prefix, pin=pin, dependencies=[]))
            self.assertTrue(archive.is_file())
            with tarfile.open(archive, "r:gz") as bundle:
                names = [setup_cache.normalize_tar_name(member.name) for member in bundle.getmembers()]
            self.assertIn("Cellar/llvm/23.1.3/bin/clang", names)
            self.assertIn("Cellar/make/4.4.1", names)


CI_TOOLCHAIN_MANIFEST = "orlix-toolchain-manifest.json"


def replace_tar_bytes(archive: Path, member_name: str, payload: bytes) -> None:
    with tarfile.open(archive, "r") as source:
        members = source.getmembers()
        payloads = {}
        for member in members:
            if member.isfile():
                extracted = source.extractfile(member)
                payloads[member.name] = b"" if extracted is None else extracted.read()
    rewritten = archive.with_name(archive.name + ".rewritten")
    with tarfile.open(rewritten, "w:gz", format=tarfile.PAX_FORMAT) as bundle:
        for member in members:
            info = tarfile.TarInfo(member.name)
            info.type = member.type
            info.linkname = member.linkname
            info.mode = member.mode
            info.mtime = member.mtime
            if member.name == member_name:
                info.type = tarfile.REGTYPE
                info.linkname = ""
                info.size = len(payload)
                bundle.addfile(info, io.BytesIO(payload))
            elif member.isfile():
                data = payloads[member.name]
                info.size = len(data)
                bundle.addfile(info, io.BytesIO(data))
            else:
                bundle.addfile(info)
    rewritten.replace(archive)


def append_tar_member(archive: Path, member_name: str, payload: bytes) -> None:
    with tarfile.open(archive, "r:*") as source:
        members = source.getmembers()
        payloads = {}
        for member in members:
            if member.isfile():
                extracted = source.extractfile(member)
                payloads[member.name] = b"" if extracted is None else extracted.read()
    rewritten = archive.with_name(archive.name + ".rewritten")
    with tarfile.open(rewritten, "w:gz", format=tarfile.PAX_FORMAT) as bundle:
        for member in members:
            info = tarfile.TarInfo(member.name)
            info.type = member.type
            info.linkname = member.linkname
            info.mode = member.mode
            info.mtime = member.mtime
            if member.isfile():
                data = payloads[member.name]
                info.size = len(data)
                bundle.addfile(info, io.BytesIO(data))
            else:
                bundle.addfile(info)
        extra = tarfile.TarInfo(member_name)
        extra.size = len(payload)
        extra.mtime = 0
        extra.mode = 0o644
        bundle.addfile(extra, io.BytesIO(payload))
    rewritten.replace(archive)


def write_formula(prefix: Path, name: str, version: str, *, url: str | None = None) -> None:
    short = name.rsplit("/", 1)[-1]
    if name.count("/") == 2:
        user, repo, _formula = name.split("/")
        path = prefix / "Library" / "Taps" / user / f"homebrew-{repo}" / "Formula" / f"{short}.rb"
    else:
        path = (
            prefix / "Library" / "Taps" / "homebrew" / "homebrew-core" / "Formula"
            / short[:1].lower() / f"{short}.rb"
        )
    path.parent.mkdir(parents=True, exist_ok=True)
    if url is None:
        body = f'class FormulaStub < Formula\n  version "{version}"\nend\n'
    else:
        body = f"class FormulaStub < Formula\n{url}end\n"
    path.write_text(body, encoding="utf-8")


def write_keg(prefix: Path, name: str, version: str, *, clang: bool = False) -> None:
    short = name.rsplit("/", 1)[-1]
    keg = prefix / "Cellar" / short / version
    (keg / "bin").mkdir(parents=True, exist_ok=True)
    if clang:
        (keg / "bin" / "clang").write_bytes(b"clang")
    opt = prefix / "opt" / short
    opt.parent.mkdir(parents=True, exist_ok=True)
    if opt.is_symlink() or opt.exists():
        opt.unlink()
    opt.symlink_to(Path("..") / "Cellar" / short / version)


class WorkflowSetupCacheTests(unittest.TestCase):
    def test_tool_workflows_cache_xcode_and_homebrew_without_the_output_base(self) -> None:
        for name in WORKFLOWS:
            text = workflow_text(name)
            self.assertIn("xcode-present", text, name)
            self.assertIn("xcode-version: ${{ steps.xcode-version.outputs.version }}", text, name)
            self.assertIn("steps.xcode-present.outputs.installed == 'true'", text, name)
            self.assertIn("steps.xcode-present.outputs.installed != 'true'", text, name)
            self.assertIn(
                "orlix-xcode-${{ steps.xcode-version.outputs.version }}-17F113",
                text,
                name,
            )
            self.assertIn('test "$(xcodes version)" = "2.1.0"', text, name)
            if name == "bazel-ci.yml":
                self.assertNotIn("homebrew-bottles", text, name)
                self.assertNotIn("install-bottles", text, name)
                self.assertNotIn("capture-bottles", text, name)
                self.assertNotIn("Save Homebrew source bottles", text, name)
                tools = text.split("name: Install declared build tools", 1)[1].split("\n      - name:", 1)[0]
                self.assertLess(tools.index("bundle-satisfied"), tools.index("brew tap xcodesorg/made"), name)
                self.assertLess(tools.index("brew bundle --file bazel/ci/Brewfile"), tools.index('test "$(xcodes version)"'), name)
            else:
                self.assertIn(
                    "orlix-homebrew-${{ hashFiles('Brewfile') }}-xcodes-2.1.0-6e47b55144cb1d1f5948e0bb6f443b13a8a8efc2",
                    text,
                    name,
                )
                self.assertIn("install-bottles", text, name)
                self.assertIn("capture-bottles", text, name)
            self.assertNotIn('test "$(xcodes version)" = "2.0.3"', text, name)
            for block in cache_blocks(text):
                self.assertNotIn("output-base", block, name)
                self.assertNotIn("disk-cache", block, name)
                if name != "bazel-ci.yml":
                    self.assertNotIn("Orlix/ccache", block, name)

    def test_simulator_jobs_skip_install_unless_a_runtime_must_be_prepared(self) -> None:
        for name in RUNTIME_WORKFLOWS:
            text = workflow_text(name)
            self.assertIn('prepare-runtime --version 15.5 --archive-dir "$HOME/Library/Caches/Orlix/ios15-runtime"', text, name)
            self.assertIn('prepare-runtime --version 26.5 --archive-dir "$HOME/Library/Caches/Orlix/ios26-runtime"', text, name)
            self.assertIn('--runtime-dir "$HOME/Library/Developer/CoreSimulator/Profiles/Runtimes"', text, name)
            self.assertIn("orlix-ios15-simruntime-bootable-v1", text, name)
            self.assertIn("extract-bootable-runtime", text, name)
            self.assertLess(
                text.index("name: Restore bootable iOS 15.5 simulator runtime"),
                text.index("name: Restore iOS 15.5 runtime archive"),
                name,
            )
            self.assertLess(text.index("extract-bootable-runtime"), text.index("prepare-runtime --version 15.5"), name)
            self.assertIn("steps.ios15-bootable.outputs.cache-hit != 'true'", text, name)
            self.assertNotIn("orlix-installed-simruntimes-15.5-complete", text, name)
            self.assertNotIn("orlix-installed-simruntimes-15.5-26.5", text, name)
            self.assertIn("runtime-identifier --version 15.5", text, name)
            self.assertIn("runtime-identifier --version 26.5", text, name)
            self.assertNotIn("stage-runtime --version 26.5", text, name)
            self.assertNotIn("sudo xcodes runtimes install", text, name)
            self.assertNotIn('.version == "15.5"', text, name)
            if name == "bazel-ci.yml":
                self.assertNotIn("boot-simulator", text, name)
                self.assertNotIn("orlix-ios-runtime-26.5-arm64", text, name)
                self.assertNotIn("stage-runtime", text, name)
                continue
            self.assertIn('boot-simulator --udid "$current_id"', text, name)
            self.assertIn('boot-simulator --udid "$ios15_id"', text, name)
            self.assertIn("orlix-ios-runtime-26.5-arm64", text, name)
        ci = workflow_text("bazel-ci.yml")
        main_only = '[ "$GITHUB_EVENT_NAME" = push ] && [ "$GITHUB_REF" = refs/heads/main ]'
        prepare = ci.split("name: Prepare both simulator runtimes", 1)[1].split("\n      - name:", 1)[0]
        self.assertLess(prepare.index('prepare-runtime --version 15.5'), prepare.index(main_only))
        self.assertLess(prepare.index('simctl create "Orlix iOS 15.5 CI"'), prepare.index(main_only))
        guarded = prepare.split(main_only, 1)[1].split("\n          fi\n", 1)[0]
        self.assertLess(prepare.index("extract-bootable-runtime"), prepare.index("prepare-runtime --version 15.5"))
        self.assertNotIn("prepare-runtime --version 15.5", guarded)
        self.assertNotIn('simctl create "Orlix iOS 15.5 CI"', guarded)
        self.assertNotIn("boot-simulator", prepare)
        self.assertIn("archive-bootable-runtime --version 15.5", guarded)
        self.assertNotIn("stage-runtime", guarded)
        self.assertIn("ORLIX_IOS15_SIMULATOR_ID=$ios15_id", prepare)
        self.assertGreater(prepare.index("bootable-archive-present"), prepare.index("archive-bootable-runtime"))

    def test_every_installer_workflow_skips_xcode_and_homebrew_when_present(self) -> None:
        for name in WORKFLOWS + (
            "bazel-gc.yml",
            "testflight-beta.yml",
            "ios-build.yml",
            "ios-share.yml",
        ):
            text = workflow_text(name)
            self.assertIn("xcode-present", text, name)
            self.assertIn("xcode-selected", text, name)
            self.assertIn("steps.xcode-present.outputs.installed == 'true'", text, name)
            self.assertIn("steps.xcode-present.outputs.installed != 'true'", text, name)
            if name == "bazel-ci.yml":
                self.assertIn("bundle-satisfied --brewfile bazel/ci/Brewfile", text, name)
                self.assertIn("brew bundle --file bazel/ci/Brewfile", text, name)
                self.assertNotIn("brew bundle --file Brewfile", text, name)
                promoted = text.split("brew list cosign", 1)[1].split("\n      - name:", 1)[0]
                self.assertIn("brew list oras", promoted)
                local = (ROOT / "Brewfile").read_text(encoding="utf-8")
                ci_brew = (ROOT / "bazel/ci/Brewfile").read_text(encoding="utf-8")
                for formula in ("actionlint", "zizmor", "cosign", "oras"):
                    self.assertIn(f'brew "{formula}"', local)
                    self.assertNotIn(f'brew "{formula}"', ci_brew)
                for formula in ("llvm", "lld", "zig", "autoconf", "automake", "libtool"):
                    self.assertIn(f'brew "{formula}"', local)
                    self.assertIn(f'brew "{formula}"', ci_brew)
            else:
                self.assertIn("bundle-satisfied --brewfile Brewfile", text, name)
            self.assertIn("skipping brew bundle", text, name)
        for name in WORKFLOWS:
            if name in RUNTIME_WORKFLOWS:
                continue
            self.assertNotIn("prepare-runtime", workflow_text(name), name)
            self.assertNotIn("ios26-runtime", workflow_text(name), name)

    def test_ccache_is_a_local_compiler_cache_on_the_product_workflow_only(self) -> None:
        text = workflow_text("bazel-ci.yml")
        self.assertIn("path: ~/Library/Caches/Orlix/ccache", text)
        self.assertIn("orlix-ccache-${{ steps.compiler.outputs.digest }}", text)
        self.assertIn("compiler-identity --developer-dir", text)
        self.assertIn("CCACHE_MAXSIZE=4G", text)
        restore = text.split("Restore local compiler object cache", 1)[1].split("\n      - name:", 1)[0]
        self.assertNotIn("remote cache hit", restore)
        self.assertNotIn("BuildBuddy", restore)
        self.assertNotIn("buildbuddy", restore.lower())
        for name in WORKFLOWS:
            if name == "bazel-ci.yml":
                continue
            other = workflow_text(name)
            self.assertNotIn("Orlix/ccache", other, name)
            self.assertNotIn("CCACHE_MAXSIZE", other, name)

    def test_runtime_saves_stay_before_the_apple_ci_operation(self) -> None:
        text = workflow_text("bazel-ci.yml")
        prepare = text.index("name: Prepare both simulator runtimes")
        apple = text.index("name: Run the Make-owned Apple CI operation")
        names = (
            "Save bootable iOS 15.5 simulator runtime",
            "Save iOS 15.5 runtime archive",
        )
        previous = prepare
        for name in names:
            found = text.index(f"name: {name}")
            self.assertEqual(text.count(f"name: {name}"), 1, name)
            self.assertGreater(found, previous, name)
            self.assertLess(found, apple, name)
            previous = found
        self.assertNotIn("Save iOS 26.5 runtime archive", text)
        self.assertNotIn("Restore iOS 26.5 runtime archive", text)
        self.assertNotIn("Save installed simulator runtimes", text)
        self.assertNotIn("orlix-installed-simruntimes-15.5-26.5", text)
        bootable = text.split("name: Save bootable iOS 15.5 simulator runtime", 1)[1].split("\n      - name:", 1)[0]
        ios15 = text.split("name: Save iOS 15.5 runtime archive", 1)[1].split("\n      - name:", 1)[0]
        self.assertIn(
            "github.event_name == 'push' && github.ref == 'refs/heads/main' && steps.ios15-bootable.outputs.cache-hit != 'true' && steps.runtimes.outputs.bootable_archive == 'true'",
            bootable,
        )
        self.assertIn("path: ~/Library/Caches/Orlix/ios15-simruntime-bootable/iOS-15.5.simruntime.tar", bootable)
        self.assertIn("key: orlix-ios15-simruntime-bootable-v1", bootable)
        self.assertIn(
            "github.event_name == 'push' && github.ref == 'refs/heads/main' && steps.ios15-bootable.outputs.cache-hit != 'true' && steps.runtimes.outputs.ios15_archive == 'true' && steps.runtimes.outputs.bootable_archive != 'true'",
            ios15,
        )
        self.assertIn("path: ~/Library/Caches/Orlix/ios15-runtime", ios15)

    def test_pull_requests_do_not_publish_setup_caches(self) -> None:
        text = workflow_text("bazel-ci.yml")
        save = text.split("Save Bazel repository cache", 1)[1]
        self.assertIn("github.event_name == 'push' && github.ref == 'refs/heads/main'", save)
        self.assertIn("Save local compiler object cache", save)
        self.assertNotIn("Save Homebrew source bottles", text)
        self.assertNotIn("homebrew-bottles", text)
        proof = workflow_text("bazel-promoted-consumer-proof.yml")
        self.assertNotIn("actions/cache/save@", proof)

    def test_ordinary_ci_restores_one_toolchain_archive_before_brew_bundle(self) -> None:
        text = workflow_text("bazel-ci.yml")
        key = "orlix-ci-toolchain-${{ hashFiles('bazel/ci/Brewfile') }}-${{ steps.toolchain-pin.outputs.pin }}"
        self.assertEqual(text.count(f"key: {key}"), 2)
        self.assertLess(text.index("name: Restore CI toolchain bundle"), text.index("name: Install declared build tools"))
        self.assertLess(
            text.index("name: Restore bootable iOS 15.5 simulator runtime"),
            text.index("name: Restore CI toolchain bundle"),
        )
        tools = text.split("name: Install declared build tools", 1)[1].split("\n      - name:", 1)[0]
        self.assertLess(tools.index("restore-toolchain"), tools.index("brew bundle --file bazel/ci/Brewfile"))
        self.assertLess(tools.index("bundle-satisfied"), tools.index("brew tap xcodesorg/made"))
        self.assertLess(tools.index("brew bundle --file bazel/ci/Brewfile"), tools.index("brew reinstall e2fsprogs"))
        self.assertLess(tools.index("brew reinstall e2fsprogs"), tools.index('test "$(xcodes version)"'))
        self.assertIn("[ ! -f /opt/homebrew/etc/mke2fs.conf ]", tools)
        self.assertIn("brew install e2fsprogs", tools)
        self.assertIn("pinned clang is /opt/homebrew/opt/llvm/bin/clang; skipping brew bundle", tools)
        self.assertIn("declared build tools took", tools)
        self.assertIn("toolchain archive bytes=", tools)
        self.assertNotIn("30 seconds", tools)
        self.assertNotIn("under 30", tools)
        main_only = '[ "$GITHUB_EVENT_NAME" = push ] && [ "$GITHUB_REF" = refs/heads/main ]'
        self.assertLess(tools.index(main_only), tools.index("pack-toolchain"))
        save = text.split("name: Save CI toolchain bundle", 1)[1].split("\n      - name:", 1)[0]
        self.assertIn("github.event_name == 'push' && github.ref == 'refs/heads/main'", save)
        self.assertIn("steps.ci-toolchain.outputs.cache-hit != 'true'", save)
        self.assertIn("path: ~/Library/Caches/Orlix/ci-toolchain/ci-toolchain.tar.gz", save)
        self.assertIn("cache-retention --toolchain-bytes", tools)
        self.assertIn("97654617", tools)
        self.assertIn("571396067", tools)
        self.assertIn("steps.tools.outputs.drop_ccache != 'true'", text)
        self.assertIn("steps.tools.outputs.drop_repository != 'true'", text)
        self.assertNotIn("2000000000", text)
        for block in cache_blocks(text):
            self.assertNotIn("/opt/homebrew", block)
            self.assertNotIn("path: /opt/homebrew", block)
        self.assertIn("key: orlix-ios15-simruntime-bootable-v1", text)
        self.assertNotIn("boot-simulator", text)
        self.assertNotIn("llvm@20", text)
        prepare = text.split("name: Prepare both simulator runtimes", 1)[1].split("\n      - name:", 1)[0]
        self.assertLess(prepare.index("extract-bootable-runtime"), prepare.index("prepare-runtime --version 15.5"))
        self.assertNotIn("boot-simulator", prepare)


if __name__ == "__main__":
    unittest.main()
