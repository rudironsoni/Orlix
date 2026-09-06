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

    def test_ios_15_5_runtime_is_supported_locally(self) -> None:
        row = self.rows["ios-15.5-ci-runtime"]
        self.assertEqual(row["result"], "supported")
        self.assertIn("ios15-simulator-gate", row["proof"])

    def test_xcode_27_is_local_gated_not_unsupported(self) -> None:
        row = self.rows["xcode-27-local-cache"]
        self.assertEqual(row["result"], "gated")
        self.assertIn("not the product pin", row["gate"])

    def test_unknown_xcode_is_unsupported(self) -> None:
        self.assertEqual(self.rows["unknown-xcode"]["result"], "unsupported")

    def test_every_row_has_a_legal_result(self) -> None:
        for row in self.matrix["rows"]:
            self.assertIn(row["result"], {"supported", "gated", "unsupported"})

    def test_ios_15_later_system_features_stay_unavailable(self) -> None:
        gates = {item["feature"]: item["ios_15"] for item in self.matrix["feature_gates"]}
        self.assertEqual(gates["Live Activities / ActivityKit"], "unavailable")
        self.assertIn("disabled", gates["MLX execution"])
        self.assertIn("weak-linked", gates["AppIntents.framework"])

    def test_promoted_mode_is_gated_until_lock(self) -> None:
        row = self.rows["ios-15.0-promoted-buildset"]
        self.assertEqual(row["result"], "gated")
        self.assertIn("artifacts.lock.json", row["gate"])
        self.assertIn("259dc911", row["gate"])
        self.assertIn("does not substitute OCI components", row["gate"])

    def test_public_make_operations_are_named(self) -> None:
        names = {item["public_name"] for item in self.matrix["make_operations"]}
        for required in ("build", "test", "runtime-tests", "beta-archive", "xcodeproj", "ios15-simulator-gate"):
            self.assertIn(required, names)

    def test_build_bazel_shadow_is_opt_in(self) -> None:
        build = next(item for item in self.matrix["make_operations"] if item["public_name"] == "build")
        self.assertEqual(build["bazel_shadow"], "__bazel-orlix-app")
        self.assertIn("ADR 0037 cutover", build["now"])
        self.assertIn("Bazel is product-compile authority", self.matrix["authority"])

    def test_public_shadows_stay_opt_in(self) -> None:
        ops = {item["public_name"]: item for item in self.matrix["make_operations"]}
        self.assertEqual(ops["xcodeproj"]["bazel_shadow"], "__bazel-feasibility-xcodeproj")
        self.assertEqual(ops["headers_install"]["bazel_shadow"], "__bazel-kernel-uapi")
        self.assertIn("ADR 0037 cutover", ops["xcodeproj"]["now"])
        self.assertIn("ADR 0037 cutover", ops["headers_install"]["now"])

    def test_product_app_does_not_declare_a_kernel_framework(self) -> None:
        build = (Path(__file__).resolve().parents[2] / "Orlix" / "BUILD.bazel").read_text(encoding="utf-8")
        self.assertNotIn('name = "OrlixKernelFramework"', build)
        self.assertIn("//bazel/feasibility/kernel:macho_link", build)
        self.assertIn('name = "Orlix"', build)
        self.assertIn('name = "OrlixOSFramework"', build)
