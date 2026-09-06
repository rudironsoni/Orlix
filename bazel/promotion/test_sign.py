from __future__ import annotations

import os
import tempfile
import unittest
from pathlib import Path
from unittest import mock

import sign


class SignTests(unittest.TestCase):
    def test_unset_key_does_not_invent_signature(self) -> None:
        os.environ.pop("ORLIX_COSIGN_KEY", None)
        with self.assertRaises(sign.SignError) as raised:
            sign.sign_digest("ab" * 32, "uapi")
        self.assertIn("ORLIX_COSIGN_KEY", str(raised.exception))

    def test_missing_cosign_does_not_invent_signature(self) -> None:
        os.environ["ORLIX_COSIGN_KEY"] = "file:///unused"
        try:
            with mock.patch("sign.shutil.which", return_value=None):
                with self.assertRaises(sign.SignError) as raised:
                    sign.sign_digest("cd" * 32, "mlibc")
            self.assertIn("cosign is required", str(raised.exception))
        finally:
            os.environ.pop("ORLIX_COSIGN_KEY", None)

    def test_missing_artifact_does_not_invent_oci_digest(self) -> None:
        os.environ["ORLIX_COSIGN_KEY"] = "file:///unused"
        os.environ.pop("ORLIX_PROMOTE_ARTIFACT", None)
        try:
            with mock.patch("sign.shutil.which", return_value="/usr/bin/cosign"):
                with self.assertRaises(sign.SignError) as raised:
                    sign.sign_digest("ef" * 32, "rootfs")
            self.assertIn("artifact path is required", str(raised.exception))
        finally:
            os.environ.pop("ORLIX_COSIGN_KEY", None)

    def test_oras_digest_is_recorded_not_invented(self) -> None:
        os.environ["ORLIX_COSIGN_KEY"] = "file:///unused"
        observed = "11" * 32
        calls: list[list[str]] = []

        class Result:
            def __init__(self, stdout: str) -> None:
                self.stdout = stdout

        def fake_run(argv: list[str], env=None, cwd=None):
            calls.append(list(argv))
            if argv[0] == "oras" and "push" in argv:
                return Result(f"Pushed [registry] ghcr.io/example/orlix/uapi\nDigest: sha256:{observed}\n")
            if argv[0] == "cosign" and "sign" in argv:
                return Result("")
            raise AssertionError(argv)

        with tempfile.TemporaryDirectory() as tmp:
            artifact = Path(tmp) / "uapi.sha256"
            artifact.write_text("ab" * 32 + "\n", encoding="utf-8")
            try:
                with mock.patch("sign.shutil.which", return_value="/usr/bin/tool"):
                    payload = sign.sign_digest(
                        "ab" * 32,
                        "uapi",
                        artifact=str(artifact),
                        repository="ghcr.io/example/orlix",
                        run=fake_run,
                    )
            finally:
                os.environ.pop("ORLIX_COSIGN_KEY", None)
        self.assertTrue(payload["signed"])
        self.assertEqual(payload["oci_digest"], f"sha256:{observed}")
        self.assertEqual(payload["oci_reference"], f"ghcr.io/example/orlix/uapi@sha256:{observed}")
        self.assertTrue(any(call[0] == "oras" and "push" in call for call in calls))
        self.assertTrue(any(call[0] == "cosign" and "sign" in call for call in calls))

    def test_invalid_digest_fails_loud(self) -> None:
        with self.assertRaises(ValueError):
            sign.sign_digest("short", "uapi")


if __name__ == "__main__":
    unittest.main()
