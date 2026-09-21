from __future__ import annotations

import subprocess
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest import mock

import xcode_select


REPO = Path(__file__).resolve().parents[2]
DIR_26 = "/Applications/Xcode-26.6.0.app/Contents/Developer"
APP_26 = "/Applications/Xcode-26.6.0.app"
DIR_27 = "/Applications/Xcode-27.0.0-Release.Candidate.app/Contents/Developer"
APP_27 = "/Applications/Xcode-27.0.0-Release.Candidate.app"


def _runner(
    resolved_version="26.6",
    resolved_path=APP_26,
    lookup_versions=("26.6",),
    select_path=APP_26,
    select_error=None,
    developer_dir=DIR_26,
    developer_dir_after_select=None,
    version="26.6",
    build="17F113",
    calls=None,
):
    selected = {"done": False}

    def observe(command, **kwargs):
        if calls is not None:
            calls.append((list(command), dict(kwargs)))
        if command == ["xcodes", "installed"]:
            raise AssertionError("must not parse the human-readable xcodes installed listing")
        if len(command) == 3 and command[:2] == ["xcodes", "installed"]:
            if command[2] in lookup_versions:
                return SimpleNamespace(stdout=resolved_path + "\n", stderr="")
            raise subprocess.CalledProcessError(
                1, command, output="", stderr=f"{command[2]} is not installed.\n"
            )
        if command == ["xcodes", "select", "--print-path"]:
            if select_error is not None:
                raise select_error
            selected["done"] = True
            return SimpleNamespace(stdout=select_path + "\n", stderr="")
        if command == ["xcode-select", "-p"]:
            if selected["done"] and developer_dir_after_select is not None:
                return SimpleNamespace(stdout=developer_dir_after_select + "\n")
            return SimpleNamespace(stdout=developer_dir + "\n")
        if command == ["/usr/bin/xcodebuild", "-version"]:
            return SimpleNamespace(stdout=f"Xcode {version}\nBuild version {build}\n")
        raise AssertionError(f"unexpected command: {command}")

    return observe


def _which():
    return mock.patch("xcode_select.shutil.which", return_value="/opt/homebrew/bin/xcodes")


class XcodeSelectTests(unittest.TestCase):
    def test_no_build_script_constructs_xcode_application_paths(self) -> None:
        roots = [REPO / "Makefile", REPO / "make", REPO / "bazel" / "config", REPO / "bazel" / "extensions", REPO / ".github" / "workflows"]
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
            and path.suffix in {".mk", ".yml", ".py", "", ".example", ".bazelrc", ".bzl"}
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
        with _which(), mock.patch("xcode_select.subprocess.run", side_effect=_runner(lookup_versions=())):
            with self.assertRaisesRegex(
                xcode_select.XcodeSelectError,
                r"xcodes could not resolve Xcode 26\.6 in its configured Xcode directory",
            ):
                xcode_select.verify(str(REPO), "17F113")

    def test_resolution_failure_does_not_claim_global_absence(self) -> None:
        with _which(), mock.patch("xcode_select.subprocess.run", side_effect=_runner(lookup_versions=())):
            with self.assertRaisesRegex(xcode_select.XcodeSelectError, r"xcodes install 26\.6") as raised:
                xcode_select.verify(str(REPO), "17F113")
        self.assertNotIn("Xcode 26.6 is not installed;", str(raised.exception))
        self.assertIn("configured Xcode directory", str(raised.exception))

    def test_selection_uses_xcodes(self) -> None:
        calls = []
        with _which(), mock.patch("xcode_select.subprocess.run", side_effect=_runner(calls=calls)):
            payload = xcode_select.select(str(REPO), "17F113")
        selects = [command for command, _ in calls if command[:2] == ["xcodes", "select"]]
        self.assertEqual(selects, [["xcodes", "select", "--print-path"]])
        self.assertEqual(payload["version"], "26.6")
        self.assertEqual(payload["developer_dir"], DIR_26)

    def test_xcode_version_file_drives_xcodes_select(self) -> None:
        calls = []
        with tempfile.TemporaryDirectory() as tmp:
            (Path(tmp) / ".xcode-version").write_text("26.6\n", encoding="utf-8")
            with _which(), mock.patch(
                "xcode_select.subprocess.run", side_effect=_runner(calls=calls)
            ):
                xcode_select.select(tmp, "17F113")
        selects = [(command, kwargs) for command, kwargs in calls if command[:2] == ["xcodes", "select"]]
        self.assertEqual(len(selects), 1)
        command, kwargs = selects[0]
        self.assertNotIn("26.6", command)
        self.assertEqual(kwargs.get("cwd"), str(tmp))

    def test_no_listing_parsing_and_no_version_normalization(self) -> None:
        calls = []
        with _which(), mock.patch("xcode_select.subprocess.run", side_effect=_runner(calls=calls)):
            xcode_select.select(str(REPO), "17F113")
            xcode_select.verify(str(REPO), "17F113")
        for command, _ in calls:
            self.assertNotEqual(command, ["xcodes", "installed"])
        source = Path(xcode_select.__file__).read_text(encoding="utf-8")
        self.assertNotIn("installed_versions", source)

    def test_xcodes_directory_is_honored(self) -> None:
        calls = []
        with _which(), mock.patch("xcode_select.subprocess.run", side_effect=_runner(calls=calls)):
            xcode_select.select(str(REPO), "17F113")
            xcode_select.verify(str(REPO), "17F113")
        for command, kwargs in calls:
            if command[0] == "xcodes":
                self.assertNotIn("--directory", command)
                self.assertNotIn("env", kwargs)

    def test_xcodes_resolves_the_version(self) -> None:
        with _which(), mock.patch("xcode_select.subprocess.run", side_effect=_runner()):
            self.assertEqual(xcode_select.resolve_requested("26.6"), APP_26)

    def test_selected_developer_directory_comes_from_xcode_select(self) -> None:
        with _which(), mock.patch("xcode_select.subprocess.run", side_effect=_runner()):
            payload = xcode_select.verify(str(REPO), "17F113")
        self.assertEqual(payload["developer_dir"], DIR_26)
        self.assertEqual(payload["xcodes_resolved_path"], APP_26)
        other = "/Library/Developer/Xcode-26.6.0.app/Contents/Developer"
        with _which(), mock.patch(
            "xcode_select.subprocess.run", side_effect=_runner(developer_dir=other)
        ):
            payload = xcode_select.verify(str(REPO), "17F113")
        self.assertEqual(payload["developer_dir"], other)

    def test_wrong_xcode_version_fails(self) -> None:
        with _which(), mock.patch(
            "xcode_select.subprocess.run", side_effect=_runner(version="27.0", build="27A266a")
        ):
            with self.assertRaisesRegex(xcode_select.XcodeSelectError, r"expected Xcode 26\.6"):
                xcode_select.verify(str(REPO), "17F113")

    def test_correct_version_with_wrong_build_fails(self) -> None:
        with _which(), mock.patch(
            "xcode_select.subprocess.run", side_effect=_runner(version="26.6", build="27A000a")
        ):
            with self.assertRaisesRegex(xcode_select.XcodeSelectError, "17F113"):
                xcode_select.verify(str(REPO), "17F113")

    def test_explicit_allowed_local_build_does_not_require_the_product_pin(self) -> None:
        calls = []
        with _which(), mock.patch(
            "xcode_select.subprocess.run",
            side_effect=_runner(
                developer_dir=DIR_27,
                version="27.0",
                build="27A266a",
                calls=calls,
            ),
        ):
            payload = xcode_select.verify(str(REPO), "27A266a", developer_dir=DIR_27)
        self.assertEqual(payload["version"], "27.0")
        self.assertEqual(payload["build"], "27A266a")
        self.assertFalse(payload["product_pin"])
        self.assertFalse(any(command[:2] == ["xcodes", "installed"] for command, _ in calls))

    def test_xcode_27_selection_does_not_leak_into_acceptance(self) -> None:
        with _which(), mock.patch(
            "xcode_select.subprocess.run",
            side_effect=_runner(developer_dir=DIR_27, version="27.0", build="27A266a"),
        ):
            with self.assertRaises(xcode_select.XcodeSelectError):
                xcode_select.verify(str(REPO), "17F113")
        with _which(), mock.patch("xcode_select.subprocess.run", side_effect=_runner()):
            with self.assertRaisesRegex(xcode_select.XcodeSelectError, "does not match"):
                xcode_select.verify(str(REPO), "17F113", developer_dir=DIR_27)

    def test_initial_27_selection_moves_to_26_for_orlix(self) -> None:
        calls = []
        with _which(), mock.patch(
            "xcode_select.subprocess.run",
            side_effect=_runner(
                developer_dir=DIR_27,
                developer_dir_after_select=DIR_26,
                select_path=APP_26,
                calls=calls,
            ),
        ):
            payload = xcode_select.select(str(REPO), "17F113")
        selects = [(c, k.get("cwd")) for c, k in calls if c[:2] == ["xcodes", "select"]]
        self.assertEqual(selects, [(["xcodes", "select", "--print-path"], str(REPO))])
        self.assertEqual(payload["version"], "26.6")
        self.assertEqual(payload["build"], "17F113")
        self.assertEqual(payload["developer_dir"], DIR_26)
        self.assertEqual(payload["xcodes_selected_path"], APP_26)

    def test_accepted_selection_reaches_toolchain_verification(self) -> None:
        makefile = (REPO / "make" / "bazel-migration.mk").read_text(encoding="utf-8")
        bootstrap = makefile.split("__bazel-feasibility-bootstrap:", 1)[1].split(
            "__bazel-module-lock-update:", 1
        )[0]
        self.assertIn("bazel/config/xcode_select.py", bootstrap)


if __name__ == "__main__":
    unittest.main()
