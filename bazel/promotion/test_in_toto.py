from __future__ import annotations

import json
import os
import tempfile
import unittest
from pathlib import Path

import in_toto


class InTotoTests(unittest.TestCase):
    def test_matching_digest_records_subject_without_signature(self) -> None:
        digest = "ef" * 32
        env = os.environ.pop("ORLIX_COSIGN_KEY", None)
        try:
            with tempfile.TemporaryDirectory() as tmp:
                path = Path(tmp) / "in-toto.json"
                payload = in_toto.write_provenance(str(path), "uapi", digest)
                self.assertEqual(payload["subject"][0]["digest"]["sha256"], digest)
                self.assertEqual(payload["subject"][0]["name"], "uapi")
                self.assertIs(payload["signed"], False)
                self.assertIsNone(payload["oci_digest"])
                self.assertNotIn("signature", payload)
                loaded = json.loads(path.read_text(encoding="utf-8"))
                self.assertIs(loaded["signed"], False)
                self.assertIsNone(loaded["oci_digest"])
        finally:
            if env is not None:
                os.environ["ORLIX_COSIGN_KEY"] = env

    def test_invalid_digest_fails_loud(self) -> None:
        with self.assertRaises(ValueError):
            in_toto.write_provenance("/unused.json", "uapi", "nope")


if __name__ == "__main__":
    unittest.main()
