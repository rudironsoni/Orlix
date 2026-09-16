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

    def test_promoted_mode_consumes_signed_lock(self) -> None:
        row = self.rows["ios-15.0-promoted-buildset"]
        self.assertEqual(row["result"], "supported")
        self.assertIn("payload", row["gate"])
        self.assertIn("runtime proof", row["gate"])
        self.assertIn("make __bazel-orlix-app", row["proof"])
        self.assertIn("ce931c25", row["proof"])
        self.assertIn("794e4da2", row["proof"])
        self.assertIn("--config=promoted", row["proof"])
        self.assertNotIn("latest", row["proof"])
        self.assertNotIn("localhost:5001", row["proof"])
        self.assertNotIn("localhost:5001", row["gate"])

    def test_product_graph_names_kernel_and_public_sdk(self) -> None:
        root = Path(__file__).resolve().parents[2]
        product = (root / "bazel/product/BUILD.bazel").read_text(encoding="utf-8")
        app = (root / "Orlix/BUILD.bazel").read_text(encoding="utf-8")
        session = (root / "OrlixOS/Sources/Session/BUILD.bazel").read_text(encoding="utf-8")
        mk = (root / "make/bazel-migration.mk").read_text(encoding="utf-8")
        self.assertIn('name = "kernel_composition"', product)
        self.assertIn("linux_archive = \"//bazel/feasibility/kernel:macho\"", product)
        self.assertIn("hostadapter = \"//OrlixHostAdapter/Sources:OrlixHostAdapter_srcs\"", product)
        self.assertIn('name = "OrlixOS"', session)
        self.assertIn('name = "OrlixOSFramework"', app)
        self.assertIn("//Orlix:OrlixOSFramework", mk)
        self.assertIn("//bazel/product:kernel_composition", mk)

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
        self.assertEqual(ops["test"]["bazel_shadow"], "__bazel-matrix-check")
        self.assertEqual(ops["rebuild"]["bazel_shadow"], "__bazel-orlix-app")
        self.assertEqual(ops["runtime-tests"]["bazel_shadow"], "__bazel-feasibility-xcodeproj")
        self.assertEqual(ops["ios15-simulator-gate"]["bazel_shadow"], "__bazel-ios15-simulator-gate")
        self.assertEqual(ops["beta-archive"]["bazel_shadow"], "__bazel-orlix-archive")
        self.assertIn("ADR 0037 cutover", ops["xcodeproj"]["now"])
        self.assertIn("ADR 0037 cutover", ops["headers_install"]["now"])
        self.assertIn("ADR 0037 cutover", ops["test"]["now"])
        self.assertIn("ADR 0037 cutover", ops["rebuild"]["now"])
        self.assertIn("ADR 0037 cutover", ops["runtime-tests"]["now"])
        self.assertIn("ADR 0037 cutover", ops["ios15-simulator-gate"]["now"])
        self.assertIn("ADR 0037 cutover", ops["beta-archive"]["now"])

    def test_product_app_does_not_declare_a_kernel_framework(self) -> None:
        build = (Path(__file__).resolve().parents[2] / "Orlix" / "BUILD.bazel").read_text(encoding="utf-8")
        self.assertNotIn('name = "OrlixKernelFramework"', build)
        self.assertIn("//bazel/feasibility/kernel:macho_link", build)
        self.assertIn('name = "Orlix"', build)
        self.assertIn('name = "OrlixOSFramework"', build)
        self.assertIn('name = "OrlixUITests"', build)
        self.assertIn("ios_ui_test", build)
        makefile = (Path(__file__).resolve().parents[2] / "Makefile").read_text(encoding="utf-8")
        self.assertIn("ORLIX_DEVELOPMENT_TEAM ?= ZQ3L7M567L", makefile)
        mk = (Path(__file__).resolve().parents[2] / "make" / "bazel-migration.mk").read_text(encoding="utf-8")
        self.assertIn("ios15_simulator_gate", mk)
        self.assertIn("validate_simulator_app", mk)
        bazelrc = (Path(__file__).resolve().parents[2] / ".bazelrc").read_text(encoding="utf-8")
        self.assertIn("common --repository_cache=~/Library/Caches/Orlix/Bazel/repository-cache", bazelrc)

    def test_promoted_app_selects_locked_buildset(self) -> None:
        root = Path(__file__).resolve().parents[2]
        build = (root / "Orlix" / "BUILD.bazel").read_text(encoding="utf-8")
        self.assertIn("//bazel/promotion:locked_buildset", build)
        self.assertIn("//bazel/config:component_promoted", build)
        mk = (root / "make" / "bazel-migration.mk").read_text(encoding="utf-8")
        self.assertIn("ORLIX_BAZEL_APP_TARGETS ?= //Orlix:Orlix", mk)
        self.assertIn("build $(ORLIX_BAZEL_APP_TARGETS) ", mk)
        self.assertIn("//bazel/product:kernel_composition", build)
        self.assertIn('"$${os_binary%/*}/composition.json"', mk)
        self.assertIn("ORLIX_BAZEL_COMPONENT_MODE ?= promoted", mk)
        self.assertNotIn("uapi:latest", mk)
