from __future__ import annotations

import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest import mock

import xcode_select


REPO = Path(__file__).resolve().parents[2]
DIR_26 = "/Applications/Xcode-26.6.0.app/Contents/Developer"
DIR_27 = "/Applications/Xcode-27.0.0-Release.Candidate.app/Contents/Developer"
INSTALLED_BOTH = (
    "26.6 (17F113) [Apple Silicon] (Selected)\t/Applications/Xcode-26.6.0.app\n"
    "27.0 Release Candidate (27A266a) [Apple Silicon]\t"
    "/Applications/Xcode-27.0.0-Release.Candidate.app\n"
)
INSTALLED_27_ONLY = (
    "27.0 Release Candidate (27A266a) [Apple Silicon]\t"
    "/Applications/Xcode-27.0.0-Release.Candidate.app\n"
)


def _runner(installed=INSTALLED_BOTH, developer_dir=DIR_26, version="26.6", build="17F113", calls=None):
    def observe(command, **kwargs):
        if calls is not None:
            calls.append(list(command))
        if command[:2] == ["xcodes", "installed"]:
            return SimpleNamespace(stdout=installed)
        if command[:2] == ["xcodes", "select"]:
            return SimpleNamespace(stdout="")
        if command == ["xcode-select", "-p"]:
            return SimpleNamespace(stdout=developer_dir + "\n")
        if command == ["/usr/bin/xcodebuild", "-version"]:
            return SimpleNamespace(stdout=f"Xcode {version}\nBuild version {build}\n")
        raise AssertionError(f"unexpected command: {command}")

    return observe


class XcodeSelectTests(unittest.TestCase):
    def test_no_build_script_constructs_xcode_application_paths(self) -> None:
        roots = [REPO / "Makefile", REPO / "make", REPO / "bazel" / "config", REPO / ".github" / "workflows"]
        candidates = [REPO / ".bazelrc", REPO / ".bazelrc.local.example"]
        for root in roots:
            if root.is_file():
                candidates.append(root)
            else:
                candidates.extend(sorted(root.rglob("*")))
        offenders = [
            path for path in candidates
            if path.is_file()
            and not path.name.startswith("test_")
            and path.suffix in {".mk", ".yml", ".py", "", ".example", ".bazelrc"}
            and "/Applications/Xcode" in path.read_text(encoding="utf-8", errors="replace")
        ]
        self.assertEqual(offenders, [])

    def test_xcode_version_file_is_the_canonical_release_source(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            (Path(tmp) / ".xcode-version").write_text("26.6\n", encoding="utf-8")
            self.assertEqual(xcode_select.read_requested_version(tmp), "26.6")
        self.assertEqual(xcode_select.read_requested_version(REPO), "26.6")
        makefile = (REPO / "make" / "bazel-migration.mk").read_text(encoding="utf-8")
        self.assertIn(".xcode-version", makefile)

    def test_missing_xcodes_fails_clearly(self) -> None:
        with mock.patch("xcode_select.shutil.which", return_value=None):
            with self.assertRaisesRegex(xcode_select.XcodeSelectError, "brew install xcodesorg/made/xcodes"):
                xcode_select.require_xcodes()
            with self.assertRaises(xcode_select.XcodeSelectError):
                xcode_select.verify(str(REPO), "17F113")

    def test_missing_requested_xcode_fails_clearly(self) -> None:
        with mock.patch("xcode_select.shutil.which", return_value="/opt/homebrew/bin/xcodes"), \
             mock.patch("xcode_select.subprocess.run", side_effect=_runner(installed=INSTALLED_27_ONLY)):
            with self.assertRaisesRegex(xcode_select.XcodeSelectError, r"xcodes install 26\.6"):
                xcode_select.verify(str(REPO), "17F113")

    def test_selection_uses_xcodes(self) -> None:
        calls = []
        with mock.patch("xcode_select.shutil.which", return_value="/opt/homebrew/bin/xcodes"), \
             mock.patch("xcode_select.subprocess.run", side_effect=_runner(calls=calls)):
            payload = xcode_select.select(str(REPO), "17F113")
        self.assertIn(["xcodes", "select", "26.6"], calls)
        self.assertEqual(payload["version"], "26.6")
        self.assertEqual(payload["developer_dir"], DIR_26)

    def test_selected_developer_directory_comes_from_xcode_select(self) -> None:
        with mock.patch("xcode_select.shutil.which", return_value="/opt/homebrew/bin/xcodes"), \
             mock.patch("xcode_select.subprocess.run", side_effect=_runner()):
            payload = xcode_select.verify(str(REPO), "17F113")
        self.assertEqual(payload["developer_dir"], DIR_26)
        other = "/Library/Developer/Xcode-26.6.0.app/Contents/Developer"
        with mock.patch("xcode_select.shutil.which", return_value="/opt/homebrew/bin/xcodes"), \
             mock.patch("xcode_select.subprocess.run", side_effect=_runner(developer_dir=other)):
            payload = xcode_select.verify(str(REPO), "17F113")
        self.assertEqual(payload["developer_dir"], other)

    def test_wrong_xcode_version_fails(self) -> None:
        with mock.patch("xcode_select.shutil.which", return_value="/opt/homebrew/bin/xcodes"), \
             mock.patch("xcode_select.subprocess.run", side_effect=_runner(version="27.0", build="27A266a")):
            with self.assertRaisesRegex(xcode_select.XcodeSelectError, r"expected Xcode 26\.6"):
                xcode_select.verify(str(REPO), "17F113")

    def test_correct_version_with_wrong_build_fails(self) -> None:
        with mock.patch("xcode_select.shutil.which", return_value="/opt/homebrew/bin/xcodes"), \
             mock.patch("xcode_select.subprocess.run", side_effect=_runner(version="26.6", build="27A000a")):
            with self.assertRaisesRegex(xcode_select.XcodeSelectError, "17F113"):
                xcode_select.verify(str(REPO), "17F113")

    def test_xcode_27_selection_does_not_leak_into_acceptance(self) -> None:
        with mock.patch("xcode_select.shutil.which", return_value="/opt/homebrew/bin/xcodes"), \
             mock.patch(
                 "xcode_select.subprocess.run",
                 side_effect=_runner(developer_dir=DIR_27, version="27.0", build="27A266a"),
             ):
            with self.assertRaises(xcode_select.XcodeSelectError):
                xcode_select.verify(str(REPO), "17F113")
        with mock.patch("xcode_select.shutil.which", return_value="/opt/homebrew/bin/xcodes"), \
             mock.patch("xcode_select.subprocess.run", side_effect=_runner()):
            with self.assertRaisesRegex(xcode_select.XcodeSelectError, "does not match"):
                xcode_select.verify(str(REPO), "17F113", developer_dir=DIR_27)

    def test_accepted_selection_reaches_toolchain_verification(self) -> None:
        makefile = (REPO / "make" / "bazel-migration.mk").read_text(encoding="utf-8")
        bootstrap = makefile.split("__bazel-feasibility-bootstrap:", 1)[1].split(
            "__bazel-module-lock-update:", 1
        )[0]
        self.assertIn("bazel/config/xcode_select.py", bootstrap)


if __name__ == "__main__":
    unittest.main()
