#!/usr/bin/env python3
"""Pinned TCTI ISA archive digest must match the committed blob."""

from __future__ import annotations

import hashlib
import subprocess
import tarfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
ARCHIVE = ROOT / "OrlixKernel/Sources/ports/orlix/isa/prepared-tables.tar.gz"
PIN = ROOT / "OrlixKernel/Sources/ports/orlix/isa/prepared-tables.sha256"
MEMBERS_MAKE = ROOT / "OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/isa/build-time/instruction-artifact-contributors.mk"


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

    def test_archive_members_match_canonical_make_list(self) -> None:
        result = subprocess.run(
            [
                "gmake",
                "--no-print-directory",
                "--no-builtin-rules",
                "-f",
                str(MEMBERS_MAKE),
                "__orlix-tcti-isa-archive-members",
            ],
            cwd=ROOT,
            check=False,
            capture_output=True,
            text=True,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        expected_names = result.stdout.splitlines()
        with tarfile.open(ARCHIVE, "r:gz") as archive:
            members = archive.getmembers()
            names = [member.name for member in members]
            self.assertEqual(names, expected_names)
            for member in members:
                self.assertTrue(member.isreg(), member.name)
                self.assertGreater(member.size, 0, member.name)


if __name__ == "__main__":
    unittest.main()
