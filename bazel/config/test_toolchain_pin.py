from __future__ import annotations

import unittest
import hashlib
import subprocess
import tempfile
from pathlib import Path
from types import SimpleNamespace
from unittest import mock

import toolchain_pin as pin


class ToolchainPinTests(unittest.TestCase):
    def test_manifest_records_observed_tools_and_rejects_unknown_xcode(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            binary = root / "tool"
            binary.write_bytes(b"compiler")
            output = root / "toolchain.json"

            def observe(command, **kwargs):
                if command[-1] == "-version":
                    text = "Xcode 26.6\nBuild version 17F113"
                elif command[-1] == "--version":
                    text = "bazel 9.2.0"
                elif "--find" in command or command[-1] == "--show-sdk-path":
                    text = str(binary)
                else:
                    text = "observed-sdk"
                return SimpleNamespace(stdout=text)

            with mock.patch("toolchain_pin.subprocess.run", side_effect=observe):
                payload = pin.capture_manifest(tmp, str(binary), str(output))
            self.assertEqual(payload["tools"]["clang"]["sha256"], hashlib.sha256(b"compiler").hexdigest())
            self.assertEqual(payload["sdks"]["iphoneos"]["build"], "observed-sdk")
            before = output.read_bytes()
            with mock.patch("toolchain_pin.subprocess.run", return_value=SimpleNamespace(stdout="Xcode 99\nBuild version unknown")):
                with self.assertRaises(pin.PinError):
                    pin.capture_manifest(tmp, str(binary), str(output))
            self.assertEqual(output.read_bytes(), before)

    def test_product_pin_is_xcode_26_6(self) -> None:
        product = pin.product_pin()
        self.assertEqual(product["xcode_version"], "26.6")
        self.assertEqual(product["xcode_build"], "17F113")
        self.assertEqual(product["disk_cache_namespace"], "bazel-9.2.0-xcode-17F113")

    def test_xcode_27_0_is_allowed_and_not_the_pin(self) -> None:
        self.assertIn(("27.0", "27A266a"), pin.allowed_identities())
        self.assertEqual(
            pin.namespace_for("27.0", "27A266a"),
            "bazel-9.2.0-xcode-27A266a",
        )
        self.assertNotEqual(pin.product_pin()["xcode_build"], "27A266a")

    def test_unknown_xcode_is_rejected(self) -> None:
        with self.assertRaises(pin.PinError):
            pin.require_identity("99.0", "99A0000", "bazel-9.2.0-xcode-99A0000")

    def test_wrong_disk_cache_namespace_fails(self) -> None:
        with self.assertRaises(pin.PinError):
            pin.require_identity(
                "26.6",
                "17F113",
                "/cache/bazel-9.2.0-xcode-27A5252f",
            )

    def _manifest_fixture(self, root: Path):
        binary = root / "tool"
        binary.write_bytes(b"compiler")
        return binary, root / "toolchain.json"

    def _success_observe(self, binary: Path):
        def observe(command, **kwargs):
            if command[-1] == "-version":
                text = "Xcode 26.6\nBuild version 17F113"
            elif command[-1] == "--version":
                text = "bazel 9.2.0"
            elif "--find" in command or command[-1] == "--show-sdk-path":
                text = str(binary)
            else:
                text = "observed-sdk"
            return SimpleNamespace(stdout=text)

        return observe

    def test_transient_timeout_is_retried_once_and_then_succeeds(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            binary, output = self._manifest_fixture(root)
            calls = []
            slow = ("--sdk", "iphonesimulator", "--show-sdk-build-version")

            def observe(command, **kwargs):
                calls.append(tuple(command))
                if slow[1:] == tuple(command[2:5]) and calls.count(tuple(command)) == 1:
                    raise subprocess.TimeoutExpired(command, 30)
                return self._success_observe(binary)(command, **kwargs)

            with mock.patch("toolchain_pin.subprocess.run", side_effect=observe):
                payload = pin.capture_manifest(tmp, str(binary), str(output))
            self.assertEqual(payload["sdks"]["iphonesimulator"]["build"], "observed-sdk")
            attempts = [call for call in calls if tuple(call[2:5]) == slow[1:]]
            self.assertEqual(len(attempts), 2)

    def test_persistent_timeout_reports_context_without_raw_traceback(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            binary, output = self._manifest_fixture(root)
            calls = []

            def observe(command, **kwargs):
                calls.append(tuple(command))
                if tuple(command[1:4]) == ("--sdk", "iphonesimulator", "--show-sdk-build-version"):
                    raise subprocess.TimeoutExpired(command, 30)
                return self._success_observe(binary)(command, **kwargs)

            with mock.patch("toolchain_pin.subprocess.run", side_effect=observe):
                with self.assertRaises(pin.PinError) as raised:
                    pin.capture_manifest(tmp, str(binary), str(output))
            message = str(raised.exception)
            self.assertIn("--show-sdk-build-version", message)
            self.assertIn("iphonesimulator", message)
            self.assertIn("26.6", message)
            self.assertIn("17F113", message)
            self.assertIn("30", message)
            self.assertIn("elapsed", message)
            self.assertFalse(output.exists())
            sdk_calls = [call for call in calls if tuple(call[1:4]) == ("--sdk", "iphonesimulator", "--show-sdk-build-version")]
            self.assertEqual(len(sdk_calls), 2)

    def test_failed_command_is_not_retried(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            binary, output = self._manifest_fixture(root)
            calls = []

            def observe(command, **kwargs):
                calls.append(tuple(command))
                if "--find" in command:
                    raise subprocess.CalledProcessError(1, command, output="", stderr="no such tool")
                return self._success_observe(binary)(command, **kwargs)

            with mock.patch("toolchain_pin.subprocess.run", side_effect=observe):
                with self.assertRaises(pin.PinError):
                    pin.capture_manifest(tmp, str(binary), str(output))
            finds = [call for call in calls if "--find" in call]
            self.assertEqual(len(finds), 1)
