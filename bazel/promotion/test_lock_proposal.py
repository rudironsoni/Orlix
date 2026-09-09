from __future__ import annotations

import json
import tempfile
import unittest
from unittest import mock
import subprocess

import locked_buildset
from pathlib import Path

import lock_proposal


class LockProposalTests(unittest.TestCase):
    def test_forged_signed_flag_cannot_change_lock(self) -> None:
        component = {
            "unsigned_digest": "ab" * 32,
            "oci_digest": "sha256:" + "cd" * 32,
            "oci_reference": "ghcr.io/rudironsoni/orlix/uapi@sha256:" + "cd" * 32,
        }
        components = {name: {**component, "oci_reference": component["oci_reference"].replace("/uapi@", f"/{name}@")} for name in locked_buildset.REQUIRED}
        with tempfile.TemporaryDirectory() as tmp:
            lock = Path(tmp) / "artifacts.lock.json"
            proposal = Path(tmp) / "proposal.json"
            lock.write_text(json.dumps(lock_proposal.EMPTY_LOCK))
            before = lock.read_bytes()
            proposal.write_text(json.dumps({
                "signed": True,
                "buildset": locked_buildset.buildset_digest(components),
                "components": components,
            }))
            with mock.patch("publish.trusted_public_key", return_value="/unused.pub"), \
                 mock.patch("publish.shutil.which", return_value="cosign"), \
                 mock.patch("publish.subprocess.run", side_effect=subprocess.CalledProcessError(1, "cosign", output="invalid signature")):
                with self.assertRaisesRegex(ValueError, "invalid signature"):
                    lock_proposal.apply_lock_proposal(str(proposal), str(lock))
            self.assertEqual(lock.read_bytes(), before)

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

    def test_unsigned_proposals_match_lock_and_do_not_apply(self) -> None:
        digest = "12" * 32
        with tempfile.TemporaryDirectory() as tmp:
            proposal_path = Path(tmp) / "uapi-lock-proposal.json"
            lock_path = Path(tmp) / "artifacts.lock.json"
            lock_path.write_text(
                json.dumps(
                    {
                        "schema": 1,
                        "buildset": "ab" * 32,
                        "components": {"uapi": {"unsigned_digest": digest}},
                    }
                )
                + "\n",
                encoding="utf-8",
            )
            lock_proposal.write_lock_proposal(str(proposal_path), "uapi", digest)
            before = lock_path.read_text(encoding="utf-8")
            lock_proposal.assert_unsigned_lock_proposals([str(proposal_path)], str(lock_path))
            self.assertEqual(lock_path.read_text(encoding="utf-8"), before)

    def test_invalid_digest_fails_loud(self) -> None:
        with self.assertRaises(ValueError):
            lock_proposal.write_lock_proposal("/unused.json", "uapi", "short")

    @mock.patch("lock_proposal.verify_component", side_effect=lambda p: locked_buildset.validate_component(p["component"], p))
    def test_signed_components_write_stable_buildset_and_can_apply(self, verify) -> None:
        unsigned = "ab" * 32
        oci = "sha256:" + ("cd" * 32)
        reference = f"ghcr.io/rudironsoni/orlix/uapi@{oci}"
        with tempfile.TemporaryDirectory() as tmp:
            signed_path = Path(tmp) / "uapi-signed.json"
            signed_path.write_text(
                json.dumps(
                    {
                        "schema": 1,
                        "component": "uapi",
                        "unsigned_digest": unsigned,
                        "signed": True,
                        "oci_digest": oci,
                        "oci_reference": reference,
                    }
                )
                + "\n",
                encoding="utf-8",
            )
            signed_paths = [str(signed_path)]
            for name in ("mlibc", "rootfs"):
                payload = json.loads(signed_path.read_text())
                payload["component"] = name
                payload["oci_reference"] = reference.replace("/uapi@", f"/{name}@")
                path = Path(tmp) / f"{name}-signed.json"
                path.write_text(json.dumps(payload))
                signed_paths.append(str(path))
            out1 = Path(tmp) / "lock-1.json"
            out2 = Path(tmp) / "lock-2.json"
            first = lock_proposal.write_signed_lock_proposal(str(out1), signed_paths)
            second = lock_proposal.write_signed_lock_proposal(str(out2), signed_paths)
            self.assertIs(first["signed"], True)
            self.assertEqual(first["buildset"], second["buildset"])
            self.assertEqual(len(first["buildset"]), 64)
            self.assertEqual(first["components"]["uapi"]["oci_digest"], oci)
            self.assertEqual(first["components"]["uapi"]["oci_reference"], reference)
            lock_path = Path(tmp) / "artifacts.lock.json"
            lock_path.write_text(json.dumps(lock_proposal.EMPTY_LOCK) + "\n", encoding="utf-8")
            lock_proposal.apply_lock_proposal(str(out1), str(lock_path))
            lock = json.loads(lock_path.read_text(encoding="utf-8"))
            self.assertEqual(lock["buildset"], first["buildset"])
            self.assertEqual(lock["components"]["uapi"]["unsigned_digest"], unsigned)
            self.assertNotIn("kind", lock)
            self.assertEqual(verify.call_count, 9)

    def test_unsigned_signed_json_cannot_enter_signed_lock(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            signed_path = Path(tmp) / "uapi-signed.json"
            signed_path.write_text(
                json.dumps(
                    {
                        "schema": 1,
                        "component": "uapi",
                        "unsigned_digest": "ab" * 32,
                        "signed": False,
                        "oci_digest": None,
                    }
                )
                + "\n",
                encoding="utf-8",
            )
            with self.assertRaises(ValueError):
                lock_proposal.write_signed_lock_proposal(str(Path(tmp) / "out.json"), [str(signed_path)])


if __name__ == "__main__":
    unittest.main()
