from __future__ import annotations

import json
import os
import tarfile
import tempfile
import unittest
from pathlib import Path
from unittest import mock

import reconstruct


def _lock_payload(reference: str) -> dict:
    return {
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


class ReconstructTests(unittest.TestCase):
    def test_empty_lock_cannot_reconstruct(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "artifacts.lock.json"
            path.write_text(json.dumps({"schema": 1, "buildset": None, "components": {}}) + "\n")
            with self.assertRaises(reconstruct.ReconstructError):
                reconstruct.reconstruct(str(path), tmp)

    def test_missing_oras_fails_closed(self) -> None:
        os.environ["ORLIX_COSIGN_KEY"] = "file:///unused"
        try:
            with tempfile.TemporaryDirectory() as tmp:
                path = Path(tmp) / "artifacts.lock.json"
                path.write_text(json.dumps(_lock_payload("localhost:5001/orlix/uapi@sha256:" + ("ab" * 32))) + "\n")
                with mock.patch(
                    "reconstruct.shutil.which",
                    side_effect=lambda name: None if name == "oras" else "/usr/bin/cosign",
                ):
                    with self.assertRaises(reconstruct.ReconstructError) as raised:
                        reconstruct.reconstruct(str(path), tmp)
            self.assertIn("oras and cosign are required", str(raised.exception))
        finally:
            os.environ.pop("ORLIX_COSIGN_KEY", None)

    def test_missing_cosign_fails_closed(self) -> None:
        os.environ["ORLIX_COSIGN_KEY"] = "file:///unused"
        try:
            with tempfile.TemporaryDirectory() as tmp:
                path = Path(tmp) / "artifacts.lock.json"
                path.write_text(json.dumps(_lock_payload("localhost:5001/orlix/uapi@sha256:" + ("ab" * 32))) + "\n")
                with mock.patch(
                    "reconstruct.shutil.which",
                    side_effect=lambda name: None if name == "cosign" else "/usr/bin/oras",
                ):
                    with self.assertRaises(reconstruct.ReconstructError) as raised:
                        reconstruct.reconstruct(str(path), tmp)
            self.assertIn("oras and cosign are required", str(raised.exception))
        finally:
            os.environ.pop("ORLIX_COSIGN_KEY", None)

    def test_missing_key_fails_closed(self) -> None:
        os.environ.pop("ORLIX_COSIGN_KEY", None)
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "artifacts.lock.json"
            path.write_text(json.dumps(_lock_payload("localhost:5001/orlix/uapi@sha256:" + ("ab" * 32))) + "\n")
            with mock.patch("reconstruct.shutil.which", return_value="/usr/bin/tool"):
                with self.assertRaises(reconstruct.ReconstructError) as raised:
                    reconstruct.reconstruct(str(path), tmp)
        self.assertIn("ORLIX_COSIGN_KEY", str(raised.exception))

    def test_localhost_registry_is_rejected(self) -> None:
        os.environ["ORLIX_COSIGN_KEY"] = "file:///unused"
        try:
            with tempfile.TemporaryDirectory() as tmp:
                path = Path(tmp) / "artifacts.lock.json"
                path.write_text(
                    json.dumps(
                        _lock_payload("localhost:5001/orlix/uapi@sha256:" + ("ab" * 32))
                    )
                    + "\n"
                )
                with mock.patch("reconstruct.shutil.which", return_value="/usr/bin/tool"):
                    with self.assertRaises(reconstruct.ReconstructError) as raised:
                        reconstruct.reconstruct(str(path), tmp)
            self.assertIn("GHCR", str(raised.exception))
            self.assertIn("localhost", str(raised.exception))
        finally:
            os.environ.pop("ORLIX_COSIGN_KEY", None)

    def test_unsigned_lock_fails_closed(self) -> None:
        os.environ["ORLIX_COSIGN_KEY"] = "file:///unused"
        payload = {
            "schema": 1,
            "buildset": "ff" * 32,
            "components": {
                "uapi": {
                    "unsigned_digest": "aa" * 32,
                    "oci_digest": None,
                    "oci_reference": None,
                }
            },
        }
        try:
            with tempfile.TemporaryDirectory() as tmp:
                path = Path(tmp) / "artifacts.lock.json"
                path.write_text(json.dumps(payload) + "\n")
                with mock.patch("reconstruct.shutil.which", return_value="/usr/bin/tool"):
                    with self.assertRaises(reconstruct.ReconstructError) as raised:
                        reconstruct.reconstruct(str(path), tmp)
            self.assertIn("oci_reference", str(raised.exception))
        finally:
            os.environ.pop("ORLIX_COSIGN_KEY", None)

    def test_pulls_extracts_component_tar_and_verifies(self) -> None:
        os.environ["ORLIX_COSIGN_KEY"] = "file:///unused"
        calls: list[list[str]] = []
        reference = "ghcr.io/rudironsoni/orlix/uapi@sha256:" + ("ab" * 32)

        def fake_run(argv: list[str], env=None, cwd=None):
            calls.append(list(argv))
            if argv[0] == "oras" and "pull" in argv:
                dest = Path(argv[argv.index("-o") + 1])
                blob = dest / "component.tar"
                with tarfile.open(blob, "w") as archive:
                    digest = dest / "uapi.sha256"
                    digest.write_text("aa" * 32 + "\n", encoding="utf-8")
                    archive.add(digest, arcname="uapi.sha256")
                    headers = dest / "include"
                    headers.mkdir()
                    (headers / "unistd.h").write_text("/* uapi */\n", encoding="utf-8")
                    archive.add(headers, arcname="include")

            class Result:
                stdout = ""

            return Result()

        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "artifacts.lock.json"
            path.write_text(json.dumps(_lock_payload(reference)) + "\n")
            out_dir = Path(tmp) / "reconstruct"
            try:
                with mock.patch("reconstruct.shutil.which", return_value="/usr/bin/tool"):
                    payload = reconstruct.reconstruct(str(path), str(out_dir), run=fake_run)
            finally:
                os.environ.pop("ORLIX_COSIGN_KEY", None)
            tree = Path(payload["components"]["uapi"]["tree"])
            self.assertTrue((tree / "include" / "unistd.h").is_file())
            self.assertEqual((tree / "uapi.sha256").read_text(encoding="utf-8").strip(), "aa" * 32)
        self.assertEqual(payload["components"]["uapi"]["oci_reference"], reference)
        self.assertTrue(any(call[0] == "oras" and "pull" in call and reference in call for call in calls))
        self.assertTrue(any(call[0] == "cosign" and "verify" in call and reference in call for call in calls))

    def test_raw_digest_blob_cannot_substitute(self) -> None:
        os.environ["ORLIX_COSIGN_KEY"] = "file:///unused"
        reference = "ghcr.io/rudironsoni/orlix/uapi@sha256:" + ("ab" * 32)

        def fake_run(argv: list[str], env=None, cwd=None):
            if argv[0] == "oras" and "pull" in argv:
                dest = Path(argv[argv.index("-o") + 1])
                (dest / "component.tar").write_text("aa" * 32 + "\n", encoding="utf-8")

            class Result:
                stdout = ""

            return Result()

        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "artifacts.lock.json"
            path.write_text(json.dumps(_lock_payload(reference)) + "\n")
            try:
                with mock.patch("reconstruct.shutil.which", return_value="/usr/bin/tool"):
                    with self.assertRaises(reconstruct.ReconstructError) as raised:
                        reconstruct.reconstruct(str(path), tmp, run=fake_run)
            finally:
                os.environ.pop("ORLIX_COSIGN_KEY", None)
        self.assertIn("not a component tar", str(raised.exception))


if __name__ == "__main__":
    unittest.main()
