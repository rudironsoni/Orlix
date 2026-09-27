"""Stamp promoted composition provenance without touching the byte-hashed document."""

from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path

from composition_stamp import (
    NULL_BUILDSET,
    CompositionStampError,
    copy_source_composition,
    main,
    stamp_promoted_composition,
)

ROOT = Path(__file__).resolve().parents[2]
DIGEST = "2a4697e1928fd6a33edb08cefe13cdd6c84a621bc9a0f0c694e303b1758b486c"
OTHER = "ab" * 32
HASHED = '{\n  "linux_archive": "aa",\n  "buildset": null,\n  "xcframework": null\n}\n'


class CompositionStampTests(unittest.TestCase):
    def test_promoted_stamp_records_the_locked_buildset(self) -> None:
        stamped = stamp_promoted_composition(HASHED, {"buildset": DIGEST}, DIGEST)
        self.assertIn(NULL_BUILDSET, HASHED)
        self.assertNotIn(DIGEST, HASHED)
        self.assertIn(f'"buildset": "{DIGEST}"', stamped)
        self.assertNotIn(NULL_BUILDSET, stamped)
        self.assertIn('"linux_archive": "aa"', stamped)

    def test_promoted_stamp_rejects_a_mismatched_or_preclaimed_buildset(self) -> None:
        with self.assertRaisesRegex(CompositionStampError, "does not match"):
            stamp_promoted_composition(HASHED, {"buildset": OTHER}, DIGEST)
        claimed = HASHED.replace(NULL_BUILDSET, f'"buildset": "{DIGEST}"')
        with self.assertRaisesRegex(CompositionStampError, "keep buildset null"):
            stamp_promoted_composition(claimed, {"buildset": DIGEST}, DIGEST)
        leaked = HASHED.replace('"linux_archive": "aa"', f'"linux_archive": "{DIGEST}"')
        with self.assertRaisesRegex(CompositionStampError, "must not record"):
            stamp_promoted_composition(leaked, {"buildset": DIGEST}, DIGEST)
        with self.assertRaisesRegex(CompositionStampError, "keep buildset null"):
            stamp_promoted_composition('{"buildset": "missing"}', {"buildset": DIGEST}, DIGEST)

    def test_source_copy_does_not_claim_promoted_provenance(self) -> None:
        copied = copy_source_composition(HASHED)
        self.assertEqual(copied, HASHED)
        self.assertNotIn(DIGEST, copied)
        with self.assertRaisesRegex(CompositionStampError, "keep buildset null"):
            copy_source_composition(HASHED.replace(NULL_BUILDSET, f'"buildset": "{DIGEST}"'))

    def test_cli_promoted_and_source_modes(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            composition = root / "composition.json"
            locked = root / "locked-buildset.json"
            promoted = root / "promoted.json"
            source = root / "source.json"
            composition.write_text(HASHED, encoding="utf-8")
            locked.write_text(json.dumps({"buildset": DIGEST}) + "\n", encoding="utf-8")
            self.assertEqual(
                main(
                    [
                        "--promoted",
                        "--composition",
                        str(composition),
                        "--locked",
                        str(locked),
                        "--expected",
                        DIGEST,
                        "--out",
                        str(promoted),
                    ]
                ),
                0,
            )
            self.assertEqual(
                main(["--source", "--composition", str(composition), "--out", str(source)]),
                0,
            )
            self.assertIn(DIGEST, promoted.read_text(encoding="utf-8"))
            self.assertEqual(source.read_text(encoding="utf-8"), HASHED)
            self.assertIn(NULL_BUILDSET, composition.read_text(encoding="utf-8"))

    def test_byte_hashed_rule_does_not_load_the_lock_digest(self) -> None:
        composition = (ROOT / "bazel/product/composition.bzl").read_text(encoding="utf-8")
        implementation = composition.split("def _kernel_composition_impl", 1)[1].split(
            "orlix_kernel_composition = rule", 1
        )[0]
        self.assertIn(NULL_BUILDSET, implementation)
        self.assertNotIn("LOCKED_BUILDSET", composition)
        self.assertNotIn("artifacts.lock.json", implementation)
        self.assertNotIn("locked_buildset", implementation)
        stamp = (ROOT / "bazel/product/composition_stamp.bzl").read_text(encoding="utf-8")
        self.assertIn("LOCKED_BUILDSET", stamp)
        self.assertIn("OrlixKernelCompositionStamp", stamp)
        self.assertIn("ctx.file.locked_buildset", stamp)
        self.assertNotIn("OrlixKernelMachOArchive", stamp)
        product = (ROOT / "bazel/product/BUILD.bazel").read_text(encoding="utf-8")
        self.assertIn('name = "kernel_composition_stamp"', product)
        self.assertNotIn("locked_buildset", product.split("orlix_kernel_composition(", 1)[1].split(
            "orlix_kernel_composition_stamp(", 1
        )[0])
        app = (ROOT / "Orlix/BUILD.bazel").read_text(encoding="utf-8")
        framework = app.split('name = "OrlixOSFramework"', 1)[1].split("ios_application(", 1)[0]
        promoted, source = framework.split("//bazel/config:component_promoted", 1)[1].split(
            "//conditions:default", 1
        )
        self.assertIn("//bazel/product:kernel_composition_stamp", promoted)
        self.assertNotIn("kernel_composition_stamp", source)
        self.assertNotIn("action_identity_stamp", framework)
        self.assertIn('//bazel/product:kernel_composition"', source)


if __name__ == "__main__":
    unittest.main()
