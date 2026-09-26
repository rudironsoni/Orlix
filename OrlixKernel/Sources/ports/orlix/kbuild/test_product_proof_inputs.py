#!/usr/bin/env python3
"""The shipped proof digest matches one sha256 per input file."""

from __future__ import annotations

import hashlib
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

SCRIPT = Path(__file__).resolve().parent / "product_proof_inputs.py"


class ProofInputTests(unittest.TestCase):
    def test_cli_hashes_prefix_and_files_in_order(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            first = root / "first"
            second = root / "second"
            first.write_bytes(b"abc\n")
            second.write_bytes(b"")
            listing = root / "inputs"
            listing.write_text(f"{first}\n{second}\n")
            prefix = "platform=iphonesimulator\ntarget=arm64\nsource_revision=ab\n"
            completed = subprocess.run(
                [sys.executable, str(SCRIPT), "--prefix", prefix, "--inputs", str(listing)],
                check=True,
                capture_output=True,
                text=True,
            )
            manifest = prefix.encode()
            for path in (first, second):
                manifest += hashlib.sha256(path.read_bytes()).hexdigest().encode() + b"\n"
            self.assertEqual(completed.stdout.strip(), hashlib.sha256(manifest).hexdigest())


if __name__ == "__main__":
    unittest.main()
