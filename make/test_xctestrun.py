from __future__ import annotations

import plistlib
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from xctestrun import assemble_xctrunner, build_xctestrun, resolve_test_bundle, write_xctestrun


def _developer_dir(root: Path, placeholders: bool = True) -> Path:
    developer = root / "Xcode.app" / "Contents" / "Developer"
    agents = developer / "Platforms/iPhoneSimulator.platform/Developer/Library/Xcode/Agents"
    (agents / "XCTRunner.app").mkdir(parents=True)
    info = agents / "XCTRunner.app" / "Info.plist"
    if placeholders:
        info.write_bytes(
            b"<?xml version=\"1.0\"?>"
            b"<dict><key>CFBundleName</key><string>$(WRAPPEDPRODUCTNAME)</string>"
            b"<key>CFBundleIdentifier</key><string>$(WRAPPEDPRODUCTBUNDLEIDENTIFIER)</string></dict>"
        )
    else:
        info.write_bytes(
            b"<?xml version=\"1.0\"?>"
            b"<dict><key>CFBundleName</key><string>Runner</string></dict>"
        )
    (agents / "XCTRunner.app" / "XCTRunner").write_bytes(b"runner")
    libs = developer / "Platforms/iPhoneSimulator.platform/Developer/Library"
    for framework in (
        "Frameworks/XCTest.framework",
        "PrivateFrameworks/XCTestCore.framework",
        "PrivateFrameworks/XCTAutomationSupport.framework",
        "PrivateFrameworks/XCUnit.framework",
        "PrivateFrameworks/XCTestSupport.framework",
        "Frameworks/Testing.framework",
        "Frameworks/XCUIAutomation.framework",
    ):
        (libs / framework).mkdir(parents=True)
    usr_lib = developer / "Platforms/iPhoneSimulator.platform/Developer/usr/lib"
    usr_lib.mkdir(parents=True)
    for dylib in ("libXCTestSwiftSupport.dylib", "libXCTestBundleInject.dylib"):
        (usr_lib / dylib).write_bytes(b"dylib")
    return developer


def _layout(root: Path) -> tuple[Path, Path]:
    bundle = root / "OrlixUITests.xctest"
    bundle.mkdir(parents=True, exist_ok=True)
    (bundle / "OrlixUITests").write_bytes(b"bundle")
    app = root / "Payload" / "Orlix.app"
    app.mkdir(parents=True, exist_ok=True)
    (app / "Info.plist").write_bytes(b"app")
    return bundle, app


def _content(root: Path):
    developer = _developer_dir(root)
    bundle, app = _layout(root / "src")
    work = root / "work"
    coverage = work / "coverage"
    coverage.mkdir(parents=True, exist_ok=True)
    runner = assemble_xctrunner(developer, bundle, "OrlixUITests", work)
    content = build_xctestrun(
        runner["runner_app"] / "PlugIns" / runner["bundle_name"],
        app,
        "com.rudironsoni.Orlix",
        "OrlixUITests",
        runner["runner_name"],
        runner["runner_bundle_id"],
        developer,
        ["AppLaunchSmokeUITests/testLaunchCapturesScreenshot"],
        coverage,
    )
    return content, runner, app


class XctestrunTests(unittest.TestCase):
    def test_runner_is_xctrunner_hosted_not_the_app_under_test(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            content, runner, app = _content(Path(tmp))
            entry = content["OrlixUITests"]
            self.assertEqual(entry["TestHostPath"], "__TESTROOT__/OrlixUITests-Runner.app")
            self.assertEqual(entry["TestHostBundleIdentifier"], "com.apple.test.OrlixUITests-Runner")
            self.assertEqual(entry["TestBundlePath"], "__TESTHOST__/PlugIns/OrlixUITests.xctest")
            self.assertEqual(entry["UITargetAppPath"], str(app.resolve()))
            self.assertEqual(entry["UITargetAppBundleIdentifier"], "com.rudironsoni.Orlix")
            self.assertTrue(entry["IsUITestBundle"])
            self.assertTrue(entry["IsXCTRunnerHostedTestBundle"])
            self.assertTrue((runner["runner_app"] / "PlugIns" / "OrlixUITests.xctest").is_dir())
            self.assertTrue((runner["runner_app"] / "Frameworks" / "XCTest.framework").is_dir())
            with (runner["runner_app"] / "Info.plist").open("rb") as stream:
                info = plistlib.load(stream)
            self.assertEqual(info["CFBundleIdentifier"], "com.apple.test.OrlixUITests-Runner")

    def test_runner_identity_is_set_without_shipped_placeholders(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            developer = _developer_dir(root, placeholders=False)
            bundle, _ = _layout(root / "src")
            runner = assemble_xctrunner(developer, bundle, "OrlixUITests", root / "work")
            with (runner["runner_app"] / "Info.plist").open("rb") as stream:
                info = plistlib.load(stream)
            self.assertEqual(info["CFBundleIdentifier"], "com.apple.test.OrlixUITests-Runner")

    def test_selection_travels_inside_the_xctestrun(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            content, _, _ = _content(Path(tmp))
            entry = content["OrlixUITests"]
            self.assertEqual(entry["OnlyTestIdentifiers"], ["AppLaunchSmokeUITests/testLaunchCapturesScreenshot"])
            modern = entry["OnlyTestingIdentifiers"]
            self.assertEqual(modern["suites"], [{"name": "AppLaunchSmokeUITests", "testFunctions": ["testLaunchCapturesScreenshot"]}])
            self.assertEqual(modern["xctestClasses"], [{"name": "AppLaunchSmokeUITests", "xctestMethods": ["testLaunchCapturesScreenshot"]}])

    def test_metadata_names_architecture(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            content, _, _ = _content(Path(tmp))
            infos = content["__xctestrun_metadata__"]["CodeCoverageBuildableInfos"]
            self.assertEqual(infos[0]["Architecture"], "arm64")
            self.assertEqual(infos[0]["Name"], "OrlixUITests.xctest")

    def test_profile_data_directory_is_provided(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            content, _, _ = _content(Path(tmp))
            coverage = Path(tmp) / "work" / "coverage"
            self.assertEqual(content["OrlixUITests"]["ClangProfileDataDirectoryPath"], str(coverage))
            self.assertTrue(coverage.is_dir())

    def test_missing_products_fail_loud(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            developer = _developer_dir(root)
            bundle, app = _layout(root / "src")
            runner = assemble_xctrunner(developer, bundle, "OrlixUITests", root / "work")
            staged = runner["runner_app"] / "PlugIns" / runner["bundle_name"]
            with self.assertRaises(ValueError):
                build_xctestrun(root / "absent.xctest", app, "com.rudironsoni.Orlix", "OrlixUITests", "R", "com.apple.test.R", developer, ["C/m"], root / "coverage")
            with self.assertRaises(ValueError):
                build_xctestrun(staged, app, "not-a-bundle-id", "OrlixUITests", "R", "com.apple.test.R", developer, ["C/m"], root / "coverage")
            with self.assertRaises(ValueError):
                build_xctestrun(staged, app, "com.rudironsoni.Orlix", "OrlixUITests", "R", "com.apple.test.R", developer, [], root / "coverage")

    def test_written_file_round_trips_as_xml_plist(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            content, _, app = _content(Path(tmp))
            out = Path(tmp) / "work" / "test.xctestrun"
            write_xctestrun(out, content)
            with out.open("rb") as stream:
                loaded = plistlib.load(stream)
            self.assertEqual(loaded["OrlixUITests"]["UITargetAppPath"], str(app.resolve()))

    def test_resolve_prefers_single_xctest_dir(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            target = root / "exec" / "bazel-out" / "x" / "OrlixUITests.xctest"
            target.mkdir(parents=True)
            outputs = root / "outputs.txt"
            outputs.write_text("bazel-out/x/OrlixUITests.xctest\n", encoding="utf-8")
            resolved = resolve_test_bundle(root / "exec", outputs, root / "work")
            self.assertEqual(resolved, str(target))

    def test_resolve_rejects_ambiguity(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            outputs = root / "outputs.txt"
            outputs.write_text("a.xctest\nb.zip\n", encoding="utf-8")
            with self.assertRaises(ValueError):
                resolve_test_bundle(root, outputs, root / "work")


if __name__ == "__main__":
    unittest.main()
