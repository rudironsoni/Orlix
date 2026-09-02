from __future__ import annotations

import unittest

import toolchain_pin as pin


class ToolchainPinTests(unittest.TestCase):
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
