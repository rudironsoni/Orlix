from __future__ import annotations

import json
import os
import tempfile
import unittest
from pathlib import Path
from unittest import mock

import reconstruct


class ReconstructTests(unittest.TestCase):
    def test_empty_lock_cannot_reconstruct(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "artifacts.lock.json"
            path.write_text(json.dumps({"schema": 1, "buildset": None, "components": {}}) + "\n")
            with self.assertRaises(reconstruct.ReconstructError):
                reconstruct.reconstruct(str(path))

    def test_pulls_and_verifies_each_locked_reference(self) -> None:
        os.environ["ORLIX_COSIGN_KEY"] = "file:///unused"
        calls: list[list[str]] = []
        reference = "localhost:5001/orlix/uapi@sha256:" + ("ab" * 32)

        def fake_run(argv: list[str]):
            calls.append(list(argv))

            class Result:
                stdout = ""

            return Result()

        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "artifacts.lock.json"
            path.write_text(
                json.dumps(
                    {
                        "schema": 1,
                        "buildset": "ff" * 32,
                        "components": {
                            "uapi": {
                                "unsigned_digest": "aa" * 32,
                                "oci_digest": "sha256:" + ("ab" * 32),
                                "oci_reference": reference,
                            }
                        },
                    }
                )
                + "\n"
            )
            try:
                with mock.patch("reconstruct.shutil.which", return_value="/usr/bin/tool"):
                    payload = reconstruct.reconstruct(str(path), run=fake_run)
            finally:
                os.environ.pop("ORLIX_COSIGN_KEY", None)
        self.assertEqual(payload["components"]["uapi"]["oci_reference"], reference)
        self.assertTrue(any(call[0] == "oras" and "pull" in call and reference in call for call in calls))
        self.assertTrue(any(call[0] == "cosign" and "verify" in call and reference in call for call in calls))


if __name__ == "__main__":
    unittest.main()
