from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path

import sbom


class SbomTests(unittest.TestCase):
    def test_matching_digest_writes_uapi_subject(self) -> None:
        digest = "ab" * 32
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "sbom.json"
            payload = sbom.write_sbom(str(path), "uapi", digest, actual_digest=digest)
            self.assertEqual(payload["component"], "uapi")
            self.assertEqual(payload["subject_digest"], digest)
            loaded = json.loads(path.read_text(encoding="utf-8"))
            self.assertEqual(loaded["subject_digest"], digest)
            self.assertEqual(loaded["component"], "uapi")

    def test_mismatched_or_missing_subject_fails_loud(self) -> None:
        with self.assertRaises(ValueError):
            sbom.bind_subject("ab" * 32, "cd" * 32)
        with self.assertRaises(ValueError):
            sbom.write_sbom("/unused.json", "uapi", "not-a-digest")
        with self.assertRaises(ValueError):
            sbom.write_sbom("/unused.json", "uapi", "ab" * 32, actual_digest="cd" * 32)
        with self.assertRaises(ValueError):
            sbom.write_sbom("/unused.json", "", "ab" * 32)


if __name__ == "__main__":
    unittest.main()
