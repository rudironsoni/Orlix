from __future__ import annotations

import json
import plistlib
import subprocess
import tempfile
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
        def fake_run(command, **kwargs):
            self.assertEqual(command[:3], ["brew", "bundle", "check"])
            return subprocess.CompletedProcess(command, 1, stdout="", stderr="Error: Permission denied")

        with tempfile.TemporaryDirectory() as temporary:
            brewfile = Path(temporary) / "Brewfile"
            brewfile.write_text('brew "xcodesorg/made/xcodes"\n', encoding="utf-8")
            with mock.patch.object(setup_cache.subprocess, "run", fake_run):
                self.assertTrue(setup_cache.bundle_satisfied(brewfile))

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
                    with mock.patch.object(Path, "iterdir", guarded_iterdir):
                        self.assertFalse(setup_cache.simruntime_present(runtime_dir))
                        plan = setup_cache.prepare_runtime("15.5", archive, runtime_dir)
            self.assertEqual(plan, "skip")
            self.assertFalse(any(command[:4] == ["sudo", "xcodes", "runtimes", "install"] for command in calls))
            staged = runtime_dir / "iOS 15.5.simruntime"
            self.assertFalse(staged.is_symlink())
            self.assertTrue(staged.is_dir())
            self.assertNotEqual(staged.resolve(), source.resolve())
            staged_plist = staged / "Contents" / "Info.plist"
            source_plist = source / "Contents" / "Info.plist"
            self.assertFalse(staged_plist.is_symlink())
            self.assertEqual(staged_plist.read_bytes(), b"runtime")
            self.assertNotEqual(staged_plist.stat().st_ino, source_plist.stat().st_ino)
            self.assertFalse((staged / "Contents" / "Resources" / "RuntimeRoot" / "private" / "var" / "mobile").exists())
            self.assertTrue(setup_cache.simruntime_present(runtime_dir))

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
            self.assertFalse((runtime_dir / "iOS 26.5.simruntime").is_symlink())
            self.assertEqual(
                (runtime_dir / "iOS 26.5.simruntime" / "Contents" / "Info.plist").read_bytes(),
                b"restored",
            )
            self.assertTrue(setup_cache.simruntime_present(runtime_dir))

    def test_launchd_sim_boot_is_retried_and_a_missing_runtime_fails(self) -> None:
        launchd = (
            "Unable to boot the Simulator.\n"
            "Interrupted system call\n"
            "Failed to start launchd_sim: could not bind to session, "
            "launchd_sim may have crashed or quit responding\n"
        )
        missing = "Unable to find a runtime matching the device\n"
        calls: list[list[str]] = []

        def flaky(command, **kwargs):
            calls.append(command)
            if command[:4] == ["xcrun", "simctl", "shutdown", "all"]:
                return subprocess.CompletedProcess(command, 0, stdout="", stderr="")
            if command[:3] == ["killall", "-9", "com.apple.CoreSimulator.CoreSimulatorService"]:
                return subprocess.CompletedProcess(command, 0, stdout="", stderr="")
            boots = [item for item in calls if item[:3] == ["xcrun", "simctl", "boot"]]
            if command[:4] == ["xcrun", "simctl", "boot", "device"]:
                if len(boots) == 1:
                    return subprocess.CompletedProcess(command, 1, stdout="", stderr=launchd)
                return subprocess.CompletedProcess(command, 0, stdout="", stderr="")
            if command[:5] == ["xcrun", "simctl", "bootstatus", "device", "-b"]:
                return subprocess.CompletedProcess(command, 0, stdout="Device booted.\n", stderr="")
            raise AssertionError(command)

        with mock.patch.object(setup_cache.time, "sleep") as sleep:
            with mock.patch.object(setup_cache.subprocess, "run", flaky):
                setup_cache.boot_simulator("device", attempts=3)
        self.assertEqual(sleep.call_count, 1)
        self.assertEqual(
            calls,
            [
                ["xcrun", "simctl", "boot", "device"],
                ["xcrun", "simctl", "shutdown", "all"],
                ["killall", "-9", "com.apple.CoreSimulator.CoreSimulatorService"],
                ["xcrun", "simctl", "boot", "device"],
                ["xcrun", "simctl", "bootstatus", "device", "-b"],
            ],
        )

        missing_calls: list[list[str]] = []

        def missing_runtime(command, **kwargs):
            missing_calls.append(command)
            return subprocess.CompletedProcess(command, 2, stdout="", stderr=missing)

        with mock.patch.object(setup_cache.subprocess, "run", missing_runtime):
            with self.assertRaises(SystemExit) as raised:
                setup_cache.boot_simulator("device", attempts=3)
        self.assertEqual(raised.exception.code, 2)
        self.assertEqual(len(missing_calls), 1)

        def stuck(command, **kwargs):
            return subprocess.CompletedProcess(command, 1, stdout="", stderr=launchd)

        with mock.patch.object(setup_cache.time, "sleep"):
            with mock.patch.object(setup_cache.subprocess, "run", stuck):
                with self.assertRaises(SystemExit) as raised:
                    setup_cache.boot_simulator("device", attempts=3)
        self.assertEqual(raised.exception.code, 1)

    def test_restored_simruntime_is_linked_without_an_install_command(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            bundle = root / "cache" / "iOS 15.5.simruntime" / "Contents"
            bundle.mkdir(parents=True)
            (bundle / "Info.plist").write_bytes(b"plist")
            profiles = root / "Profiles" / "Runtimes"
            linked = setup_cache.expose_restored_runtimes(root / "cache", profiles)
            self.assertEqual(linked, [profiles / "iOS 15.5.simruntime"])
            self.assertTrue((profiles / "iOS 15.5.simruntime").is_symlink())
            self.assertEqual(setup_cache.runtime_commands("skip", "15.5", root / "cache"), [])

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
            self.assertIn(
                "orlix-homebrew-${{ hashFiles('Brewfile') }}-xcodes-2.1.0-6e47b55144cb1d1f5948e0bb6f443b13a8a8efc2",
                text,
                name,
            )
            self.assertIn('test "$(xcodes version)" = "2.1.0"', text, name)
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
            self.assertIn("orlix-installed-simruntimes-15.5-26.5", text, name)
            self.assertIn("runtime-identifier --version 15.5", text, name)
            self.assertIn("runtime-identifier --version 26.5", text, name)
            self.assertIn('boot-simulator --udid "$current_id"', text, name)
            self.assertIn('boot-simulator --udid "$ios15_id"', text, name)
            self.assertIn("orlix-ios-runtime-26.5-arm64", text, name)
            self.assertNotIn("sudo xcodes runtimes install", text, name)
            self.assertNotIn('.version == "15.5"', text, name)

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
            "Save iOS 15.5 runtime archive",
            "Save iOS 26.5 runtime archive",
            "Save installed simulator runtimes",
        )
        previous = prepare
        for name in names:
            found = text.index(f"name: {name}")
            self.assertEqual(text.count(f"name: {name}"), 1, name)
            self.assertGreater(found, previous, name)
            self.assertLess(found, apple, name)
            previous = found
        ios15 = text.split("name: Save iOS 15.5 runtime archive", 1)[1].split("\n      - name:", 1)[0]
        ios26 = text.split("name: Save iOS 26.5 runtime archive", 1)[1].split("\n      - name:", 1)[0]
        installed = text.split("name: Save installed simulator runtimes", 1)[1].split("\n      - name:", 1)[0]
        self.assertIn(
            "github.event_name == 'push' && github.ref == 'refs/heads/main' && steps.runtimes.outputs.ios15_archive == 'true'",
            ios15,
        )
        self.assertIn("path: ~/Library/Caches/Orlix/ios15-runtime", ios15)
        self.assertIn(
            "github.event_name == 'push' && github.ref == 'refs/heads/main' && steps.runtimes.outputs.ios26_archive == 'true'",
            ios26,
        )
        self.assertIn("path: ~/Library/Caches/Orlix/ios26-runtime", ios26)
        self.assertIn(
            "github.event_name == 'push' && github.ref == 'refs/heads/main' && steps.installed-runtimes.outputs.cache-hit != 'true' && steps.runtimes.outputs.simruntime == 'true'",
            installed,
        )
        self.assertIn("path: ~/Library/Developer/CoreSimulator/Profiles/Runtimes", installed)

    def test_pull_requests_do_not_publish_setup_caches(self) -> None:
        text = workflow_text("bazel-ci.yml")
        save = text.split("Save Bazel repository cache", 1)[1]
        self.assertIn("github.event_name == 'push' && github.ref == 'refs/heads/main'", save)
        self.assertIn("Save local compiler object cache", save)
        self.assertIn("Save Homebrew source bottles", save)
        proof = workflow_text("bazel-promoted-consumer-proof.yml")
        self.assertNotIn("actions/cache/save@", proof)


if __name__ == "__main__":
    unittest.main()
