from __future__ import annotations

import unittest
import hashlib
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

    def test_xcode_27_is_allowed_and_not_the_pin(self) -> None:
        self.assertIn(("27.0", "27A5252f"), pin.allowed_identities())
        self.assertEqual(
            pin.namespace_for("27.0", "27A5252f"),
            "bazel-9.2.0-xcode-27A5252f",
        )
        self.assertNotEqual(pin.product_pin()["xcode_build"], "27A5252f")

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
