from __future__ import annotations

import json
import os
import tempfile
import unittest
from pathlib import Path
from unittest import mock

import publish


class PublishTests(unittest.TestCase):
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

    def test_pull_verify_uses_proposal_reference(self) -> None:
        os.environ["ORLIX_COSIGN_KEY"] = "file:///unused"
        calls: list[list[str]] = []
        observed = "sha256:" + ("cd" * 32)
        reference = f"ghcr.io/example/orlix/uapi@{observed}"

        def fake_run(argv: list[str]):
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


if __name__ == "__main__":
    unittest.main()
