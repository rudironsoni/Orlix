from __future__ import annotations

import json
import unittest
from pathlib import Path

MATRIX = Path(__file__).with_name("apple-build-matrix.json")
PIN = Path(__file__).resolve().parents[1] / "config" / "toolchain-pin.json"


class AppleBuildMatrixTests(unittest.TestCase):
    def setUp(self) -> None:
        self.matrix = json.loads(MATRIX.read_text(encoding="utf-8"))
        self.pin = json.loads(PIN.read_text(encoding="utf-8"))
        self.rows = {row["id"]: row for row in self.matrix["rows"]}

    def test_product_pin_is_xcode_26_6(self) -> None:
        self.assertEqual(self.matrix["toolchain_pin"]["xcode"], "26.6")
        self.assertEqual(self.matrix["toolchain_pin"]["xcode_build"], "17F113")
        self.assertEqual(self.pin["product_pin"]["xcode_version"], "26.6")

    def test_ios_15_5_runtime_is_gated(self) -> None:
        row = self.rows["ios-15.5-ci-runtime"]
        self.assertEqual(row["result"], "gated")

    def test_xcode_27_is_local_gated_not_unsupported(self) -> None:
        row = self.rows["xcode-27-local-cache"]
        self.assertEqual(row["result"], "gated")
        self.assertIn("not the product pin", row["gate"])

    def test_unknown_xcode_is_unsupported(self) -> None:
        self.assertEqual(self.rows["unknown-xcode"]["result"], "unsupported")

    def test_every_row_has_a_legal_result(self) -> None:
        for row in self.matrix["rows"]:
            self.assertIn(row["result"], {"supported", "gated", "unsupported"})
