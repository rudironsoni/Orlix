from __future__ import annotations

import json
import os
import tempfile
import unittest
from pathlib import Path

import compare


class PromotionCompareTests(unittest.TestCase):
    def test_matching_digests_write_unsigned_proposal(self) -> None:
        digest = "a" * 64
        with tempfile.TemporaryDirectory() as tmp:
            first = Path(tmp) / "a.sha256"
            second = Path(tmp) / "b.sha256"
            proposal = Path(tmp) / "proposal.json"
            first.write_text(digest + "\n", encoding="utf-8")
            second.write_text(digest + "\n", encoding="utf-8")
            self.assertEqual(compare.compare_digests(str(first), str(second)), digest)
            env = os.environ.get("ORLIX_COSIGN_KEY")
            if env is not None:
                del os.environ["ORLIX_COSIGN_KEY"]
            try:
                self.assertEqual(
                    compare.main(
                        [str(first), str(second), "--component", "uapi", "--proposal", str(proposal)]
                    ),
                    0,
                )
            finally:
                if env is not None:
                    os.environ["ORLIX_COSIGN_KEY"] = env
            payload = json.loads(proposal.read_text(encoding="utf-8"))
            self.assertEqual(payload["unsigned_digest"], digest)
            self.assertIs(payload["signed"], False)
            self.assertIsNone(payload["oci_digest"])
            self.assertEqual(payload["component"], "uapi")
            self.assertNotIn("signature", payload)
            self.assertNotIn("ghcr", json.dumps(payload))

    def test_mismatch_fails_loud(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            first = Path(tmp) / "a.sha256"
            second = Path(tmp) / "b.sha256"
            first.write_text("a" * 64 + "\n", encoding="utf-8")
            second.write_text("b" * 64 + "\n", encoding="utf-8")
            with self.assertRaises(ValueError):
                compare.compare_digests(str(first), str(second))

    def test_unset_cosign_key_does_not_invent_signature(self) -> None:
        digest = "c" * 64
        with tempfile.TemporaryDirectory() as tmp:
            first = Path(tmp) / "a.sha256"
            proposal = Path(tmp) / "proposal.json"
            first.write_text(digest + "\n", encoding="utf-8")
            os.environ.pop("ORLIX_COSIGN_KEY", None)
            compare.write_proposal(str(proposal), "uapi", digest)
            payload = json.loads(proposal.read_text(encoding="utf-8"))
            self.assertIs(payload["signed"], False)
            self.assertIsNone(payload["oci_digest"])


if __name__ == "__main__":
    unittest.main()
