#!/usr/bin/env python3
"""Pinned TCTI ISA archive digest must match the committed blob."""

from __future__ import annotations

import hashlib
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
ARCHIVE = ROOT / "OrlixKernel/Sources/ports/orlix/isa/prepared-tables.tar.gz"
PIN = ROOT / "OrlixKernel/Sources/ports/orlix/isa/prepared-tables.sha256"
REQUIRED = (
    "manifest",
    "source_manifest.def",
    "target_asl_availability.def",
    "target_feature_applicability.def",
    "target_feature_artifact.def",
    "target_feature_field_domain_binding.def",
    "target_instruction_artifact_generated.h",
    "target_register_artifact.def",
    "target_runtime_capability_cohort_artifact.def",
    "target_system_accessor_reconciliation.def",
)


class TctiIsaPinTest(unittest.TestCase):
    def test_archive_digest_matches_pin(self) -> None:
        self.assertTrue(ARCHIVE.is_file(), ARCHIVE)
        self.assertTrue(PIN.is_file(), PIN)
        expected = PIN.read_text(encoding="utf-8").split()[0]
        self.assertEqual(len(expected), 64)
        digest = hashlib.sha256(ARCHIVE.read_bytes()).hexdigest()
        self.assertEqual(digest, expected)

    def test_pin_names_the_archive(self) -> None:
        name = PIN.read_text(encoding="utf-8").split()[1]
        self.assertEqual(name, ARCHIVE.name)

    def test_archive_contains_required_tables(self) -> None:
        import tarfile

        with tarfile.open(ARCHIVE, "r:gz") as archive:
            names = set(archive.getnames())
        for required in REQUIRED:
            self.assertIn(required, names, required)


if __name__ == "__main__":
    unittest.main()
