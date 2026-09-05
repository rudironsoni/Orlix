from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path

import lock_proposal


class LockProposalTests(unittest.TestCase):
    def test_unsigned_proposal_does_not_set_signed_buildset(self) -> None:
        digest = "12" * 32
        with tempfile.TemporaryDirectory() as tmp:
            proposal_path = Path(tmp) / "lock-proposal.json"
            lock_path = Path(tmp) / "artifacts.lock.json"
            lock_path.write_text(json.dumps(lock_proposal.EMPTY_LOCK) + "\n", encoding="utf-8")
            payload = lock_proposal.write_lock_proposal(str(proposal_path), "uapi", digest)
            self.assertIs(payload["signed"], False)
            self.assertIsNone(payload["buildset"])
            self.assertIsNone(payload["oci_digest"])
            self.assertEqual(payload["components"]["uapi"]["unsigned_digest"], digest)
            before = lock_path.read_text(encoding="utf-8")
            with self.assertRaises(ValueError):
                lock_proposal.apply_lock_proposal(str(proposal_path), str(lock_path))
            self.assertEqual(lock_path.read_text(encoding="utf-8"), before)
            empty = lock_proposal.assert_lock_unsigned_empty(str(lock_path))
            self.assertIsNone(empty["buildset"])
            self.assertEqual(empty["components"], {})

    def test_invalid_digest_fails_loud(self) -> None:
        with self.assertRaises(ValueError):
            lock_proposal.write_lock_proposal("/unused.json", "uapi", "short")


if __name__ == "__main__":
    unittest.main()
