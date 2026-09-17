from __future__ import annotations

import plistlib
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from xctestrun import build_xctestrun, resolve_test_bundle, write_xctestrun


def _layout(root: Path) -> tuple[Path, Path]:
    bundle = root / "OrlixUITests.xctest"
    (bundle / "Info.plist").parent.mkdir(parents=True, exist_ok=True)
    (bundle / "Info.plist").write_bytes(b"bundle")
    app = root / "Payload" / "Orlix.app"
    (app / "Info.plist").parent.mkdir(parents=True, exist_ok=True)
    (app / "Info.plist").write_bytes(b"app")
    return bundle, app


class XctestrunTests(unittest.TestCase):
    def test_generated_run_references_exact_products(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            bundle, app = _layout(Path(tmp))
            content = build_xctestrun(bundle, app, "com.rudironsoni.Orlix", "OrlixUITests")
            entry = content["OrlixUITests"]
            self.assertEqual(entry["TestBundlePath"], str(bundle.resolve()))
            self.assertEqual(entry["TestHostPath"], str(app.resolve()))
            self.assertEqual(entry["UITargetAppPath"], str(app.resolve()))
            self.assertEqual(entry["TestHostBundleIdentifier"], "com.rudironsoni.Orlix")
            self.assertTrue(entry["IsUITestBundle"])
            self.assertTrue(entry["IsAppHostedTestBundle"])
            self.assertFalse(entry["IsXCTRunnerHostedTestBundle"])
            self.assertIn(str(bundle.resolve()), entry["DependentProductPaths"])
            self.assertIn(str(app.resolve()), entry["DependentProductPaths"])

    def test_missing_products_fail_loud(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            bundle, app = _layout(Path(tmp))
            with self.assertRaises(ValueError):
                build_xctestrun(Path(tmp) / "absent.xctest", app, "com.rudironsoni.Orlix", "OrlixUITests")
            with self.assertRaises(ValueError):
                build_xctestrun(bundle, Path(tmp) / "absent.app", "com.rudironsoni.Orlix", "OrlixUITests")
            with self.assertRaises(ValueError):
                build_xctestrun(bundle, app, "not-a-bundle-id", "OrlixUITests")
            with self.assertRaises(ValueError):
                build_xctestrun(bundle, app, "com.rudironsoni.Orlix", "")

    def test_written_file_round_trips_as_xml_plist(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            bundle, app = _layout(Path(tmp))
            out = Path(tmp) / "test.xctestrun"
            write_xctestrun(out, build_xctestrun(bundle, app, "com.rudironsoni.Orlix", "OrlixUITests"))
            with out.open("rb") as stream:
                loaded = plistlib.load(stream)
            self.assertEqual(loaded["OrlixUITests"]["TestHostPath"], str(app.resolve()))

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
