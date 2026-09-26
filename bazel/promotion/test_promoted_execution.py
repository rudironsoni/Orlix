from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path

import promoted_execution
from source_producers import classify_source_producer


PROMOTED_ORIGINS = {
    "kernel": "promoted",
    "uapi": "promoted",
    "mlibc": "promoted",
    "rootfs": "promoted",
}

HYBRID_KERNEL_SOURCE = {
    "kernel": "source",
    "uapi": "promoted",
    "mlibc": "promoted",
    "rootfs": "promoted",
}

SOURCE_ORIGINS = {
    "kernel": "source",
    "uapi": "source",
    "mlibc": "source",
    "rootfs": "source",
}


def _log(root: Path, entries: list[dict]) -> Path:
    path = root / "execution.json"
    path.write_text("\n".join(json.dumps(entry) for entry in entries) + "\n", encoding="utf-8")
    return path


def _promoted_entries() -> list[dict]:
    return [
        {"targetLabel": "//bazel/promotion:promoted_uapi", "mnemonic": "OrlixPromotedUapi"},
        {"targetLabel": "//bazel/promotion:promoted_sysroot", "mnemonic": "OrlixPromotedMlibc"},
        {"targetLabel": "//bazel/promotion:promoted_rootfs", "mnemonic": "OrlixPromotedRootfs"},
        {
            "targetLabel": "//bazel/promotion:promoted_kernel_release_iphonesimulator",
            "mnemonic": "OrlixPromotedKernel",
        },
        {"targetLabel": "//Orlix:Orlix", "mnemonic": "SwiftCompile"},
    ]


class PromotedExecutionTests(unittest.TestCase):
    def test_full_promoted_accepts_promoted_presence_and_forbids_source(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            log = _log(Path(tmp), _promoted_entries())
            payload = promoted_execution.prove_execution(
                log, PROMOTED_ORIGINS, profile="release", destination="iphonesimulator"
            )
        self.assertTrue(payload["ok"])
        self.assertEqual(payload["required_missing"], [])
        self.assertEqual(payload["forbidden_executed"], [])

    def test_full_promoted_rejects_kernel_source_producer(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            log = _log(
                Path(tmp),
                _promoted_entries()
                + [{"targetLabel": "//bazel/feasibility/kernel:macho", "mnemonic": "OrlixKernelMachOArchive"}],
            )
            payload = promoted_execution.prove_execution(
                log, PROMOTED_ORIGINS, profile="release", destination="iphonesimulator"
            )
        self.assertFalse(payload["ok"])
        self.assertEqual(payload["forbidden_executed"][0]["origin"], "kernel")

    def test_hybrid_allows_kernel_source_and_forbids_the_rest(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            log = _log(
                Path(tmp),
                [
                    {"targetLabel": "//bazel/feasibility/kernel:macho", "mnemonic": "OrlixKernelMachOArchive"},
                    {"targetLabel": "//bazel/promotion:promoted_uapi", "mnemonic": "OrlixPromotedUapi"},
                    {"targetLabel": "//bazel/promotion:promoted_sysroot", "mnemonic": "OrlixPromotedMlibc"},
                    {"targetLabel": "//bazel/promotion:promoted_rootfs", "mnemonic": "OrlixPromotedRootfs"},
                ],
            )
            payload = promoted_execution.prove_execution(
                log, HYBRID_KERNEL_SOURCE, profile="release", destination="iphonesimulator"
            )
        self.assertTrue(payload["ok"])
        self.assertNotIn("kernel", payload["required_promoted"])
        self.assertEqual(["uapi", "mlibc", "rootfs"], payload["required_promoted"])

    def test_hybrid_rejects_uapi_source_producer(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            log = _log(
                Path(tmp),
                [
                    {"targetLabel": "//bazel/feasibility/kernel:macho", "mnemonic": "OrlixKernelMachOArchive"},
                    {"targetLabel": "//bazel/feasibility/kernel:uapi", "mnemonic": "OrlixLinuxHeadersInstall"},
                    {"targetLabel": "//bazel/promotion:promoted_uapi", "mnemonic": "OrlixPromotedUapi"},
                    {"targetLabel": "//bazel/promotion:promoted_sysroot", "mnemonic": "OrlixPromotedMlibc"},
                    {"targetLabel": "//bazel/promotion:promoted_rootfs", "mnemonic": "OrlixPromotedRootfs"},
                ],
            )
            payload = promoted_execution.prove_execution(
                log, HYBRID_KERNEL_SOURCE, profile="release", destination="iphonesimulator"
            )
        self.assertFalse(payload["ok"])
        self.assertEqual(payload["forbidden_executed"][0]["origin"], "uapi")

    def test_source_allows_source_producers(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            log = _log(
                Path(tmp),
                [
                    {"targetLabel": "//bazel/feasibility/kernel:macho", "mnemonic": "OrlixKernelMachOArchive"},
                    {"targetLabel": "//bazel/feasibility/kernel:uapi", "mnemonic": "OrlixLinuxHeadersInstall"},
                    {"targetLabel": "//bazel/feasibility/mlibc:sysroot", "mnemonic": "OrlixMLibCSysroot"},
                    {"targetLabel": "//bazel/feasibility/packages:coreutils", "mnemonic": "OrlixGuestPackage"},
                    {"targetLabel": "//bazel/feasibility/rootfs:rootfs", "mnemonic": "OrlixRootfs"},
                ],
            )
            payload = promoted_execution.prove_execution(
                log, SOURCE_ORIGINS, profile="release", destination="iphonesimulator"
            )
        self.assertTrue(payload["ok"])
        self.assertEqual(payload["required_promoted"], [])

    def test_missing_log_fails_closed(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            with self.assertRaises(promoted_execution.PromotedExecutionError):
                promoted_execution.prove_execution(
                    Path(tmp) / "absent.json",
                    PROMOTED_ORIGINS,
                    profile="release",
                    destination="iphonesimulator",
                )

    def test_concatenated_pretty_json_objects(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "execution.json"
            path.write_text(
                "\n".join(json.dumps(entry, indent=2) for entry in _promoted_entries()) + "\n",
                encoding="utf-8",
            )
            payload = promoted_execution.prove_execution(
                path, PROMOTED_ORIGINS, profile="release", destination="iphonesimulator"
            )
        self.assertTrue(payload["ok"])
        self.assertEqual(payload["total_actions"], 5)

    def test_malformed_log_fails_closed(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "execution.json"
            path.write_text("{not json\n", encoding="utf-8")
            with self.assertRaises(promoted_execution.PromotedExecutionError):
                promoted_execution.prove_execution(
                    path, PROMOTED_ORIGINS, profile="release", destination="iphonesimulator"
                )

    def test_empty_log_fails_closed_when_promoted_required(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "execution.json"
            path.write_text("\n", encoding="utf-8")
            with self.assertRaises(promoted_execution.PromotedExecutionError):
                promoted_execution.prove_execution(
                    path, PROMOTED_ORIGINS, profile="release", destination="iphonesimulator"
                )

    def test_missing_promoted_presence_fails(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            log = _log(Path(tmp), [{"targetLabel": "//Orlix:Orlix", "mnemonic": "SwiftCompile"}])
            payload = promoted_execution.prove_execution(
                log, PROMOTED_ORIGINS, profile="release", destination="iphonesimulator"
            )
        self.assertFalse(payload["ok"])
        self.assertIn("uapi", payload["required_missing"])
        self.assertIn("kernel", payload["required_missing"])

    def test_source_producer_hidden_behind_non_registry_label_is_detected(self) -> None:
        self.assertEqual(
            "kernel",
            classify_source_producer("OrlixKernelMachOArchive", "//bazel/feasibility/kernel:macho"),
        )
        with tempfile.TemporaryDirectory() as tmp:
            log = _log(
                Path(tmp),
                _promoted_entries()
                + [{"targetLabel": "//bazel/feasibility/kernel:not-in-registry", "mnemonic": "OrlixKernelMachOArchive"}],
            )
            payload = promoted_execution.prove_execution(
                log, PROMOTED_ORIGINS, profile="release", destination="iphonesimulator"
            )
        self.assertFalse(payload["ok"])
        self.assertEqual(payload["forbidden_executed"][0]["targetLabel"], "//bazel/feasibility/kernel:not-in-registry")

    def test_package_producer_is_a_rootfs_source_producer(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            log = _log(
                Path(tmp),
                _promoted_entries()
                + [{"targetLabel": "//bazel/feasibility/packages:coreutils", "mnemonic": "OrlixGuestPackage"}],
            )
            payload = promoted_execution.prove_execution(
                log, PROMOTED_ORIGINS, profile="release", destination="iphonesimulator"
            )
        self.assertFalse(payload["ok"])
        self.assertEqual(payload["forbidden_executed"][0]["origin"], "rootfs")


if __name__ == "__main__":
    unittest.main()
