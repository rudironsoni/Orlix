from __future__ import annotations

import hashlib
import json
import os
import tempfile
import unittest
from pathlib import Path
from unittest import mock

import publish


class PublishTests(unittest.TestCase):
    def test_inline_public_key_materializes_to_a_file(self) -> None:
        pem = "-----BEGIN PUBLIC KEY-----\nMIIB\n-----END PUBLIC KEY-----\n"
        fingerprint = hashlib.sha256(pem.encode("utf-8")).hexdigest()
        with mock.patch.dict(os.environ, {"ORLIX_COSIGN_PUB": pem}), \
             mock.patch("publish.json.loads", return_value={"accepted_key_ids": [fingerprint]}):
            materialized = publish.trusted_public_key()
            self.assertTrue(Path(materialized).is_file())
            self.assertEqual(Path(materialized).read_text(encoding="utf-8"), pem)

    def test_existing_public_key_path_is_returned_unchanged(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            key = Path(tmp) / "trusted.pub"
            key.write_text("key-bytes\n", encoding="utf-8")
            fingerprint = hashlib.sha256(key.read_bytes()).hexdigest()
            with mock.patch.dict(os.environ, {"ORLIX_COSIGN_PUB": str(key)}), \
                 mock.patch("publish.json.loads", return_value={"accepted_key_ids": [fingerprint]}):
                self.assertEqual(publish.trusted_public_key(), str(key))

    def test_unsigned_proposal_must_not_publish(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "proposal.json"
            path.write_text(
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
            with self.assertRaises(publish.PublishError):
                publish.publish(str(path))

    def test_signed_without_oci_digest_must_not_publish(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "proposal.json"
            path.write_text(
                json.dumps(
                    {
                        "schema": 1,
                        "component": "uapi",
                        "unsigned_digest": "cd" * 32,
                        "signed": True,
                        "oci_digest": None,
                    }
                )
                + "\n",
                encoding="utf-8",
            )
            with self.assertRaises(publish.PublishError):
                publish.publish(str(path))

    def test_signed_without_oci_reference_must_not_publish(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "proposal.json"
            path.write_text(
                json.dumps(
                    {
                        "schema": 1,
                        "component": "uapi",
                        "unsigned_digest": "ef" * 32,
                        "signed": True,
                        "oci_digest": "sha256:" + ("ab" * 32),
                    }
                )
                + "\n",
                encoding="utf-8",
            )
            with self.assertRaises(publish.PublishError):
                publish.publish(str(path))

    @mock.patch("publish.trusted_public_key", return_value="/unused.pub")
    def test_pull_verify_uses_proposal_reference(self, public_key) -> None:
        os.environ["ORLIX_COSIGN_KEY"] = "file:///unused"
        calls: list[list[str]] = []
        observed = "sha256:" + ("cd" * 32)
        reference = f"ghcr.io/rudironsoni/orlix/uapi@{observed}"

        def fake_run(argv: list[str], env=None, cwd=None):
            calls.append(list(argv))

            class Result:
                stdout = ""

            return Result()

        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "proposal.json"
            path.write_text(
                json.dumps(
                    {
                        "schema": 1,
                        "component": "uapi",
                        "unsigned_digest": "ab" * 32,
                        "signed": True,
                        "oci_digest": observed,
                        "oci_reference": reference,
                    }
                )
                + "\n",
                encoding="utf-8",
            )
            try:
                with mock.patch("publish.shutil.which", return_value="/usr/bin/tool"):
                    payload = publish.publish(str(path), run=fake_run)
            finally:
                os.environ.pop("ORLIX_COSIGN_KEY", None)
        self.assertEqual(payload["oci_digest"], observed)
        self.assertTrue(any(call[0] == "oras" and "pull" in call and reference in call for call in calls))
        self.assertTrue(any(call[0] == "cosign" and "verify" in call and reference in call for call in calls))

    @mock.patch("publish.trusted_public_key", return_value="/unused.pub")
    def test_ghcr_pull_failure_does_not_count_as_publish(self, public_key) -> None:
        os.environ["ORLIX_COSIGN_KEY"] = "file:///unused"
        observed = "sha256:" + ("cd" * 32)
        reference = f"ghcr.io/rudironsoni/orlix/uapi@{observed}"

        def fake_run(argv: list[str], env=None):
            raise publish.PublishError("oras failed: denied: permission_denied")

        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "proposal.json"
            path.write_text(
                json.dumps(
                    {
                        "schema": 1,
                        "component": "uapi",
                        "unsigned_digest": "ab" * 32,
                        "signed": True,
                        "oci_digest": observed,
                        "oci_reference": reference,
                    }
                )
                + "\n",
                encoding="utf-8",
            )
            try:
                with mock.patch("publish.shutil.which", return_value="/usr/bin/tool"):
                    with self.assertRaises(publish.PublishError) as raised:
                        publish.publish(str(path), run=fake_run)
                self.assertIn("permission_denied", str(raised.exception))
            finally:
                os.environ.pop("ORLIX_COSIGN_KEY", None)


if __name__ == "__main__":
    unittest.main()
