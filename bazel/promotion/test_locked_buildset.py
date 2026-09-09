from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path

import locked_buildset


def _signed_lock() -> dict:
    oci = "sha256:" + ("ab" * 32)
    payload = {
        "schema": 1,
        "buildset": "cd" * 32,
        "components": {
            "uapi": {
                "unsigned_digest": "11" * 32,
                "oci_digest": oci,
                "oci_reference": f"ghcr.io/rudironsoni/orlix/uapi@{oci}",
            },
            "mlibc": {
                "unsigned_digest": "22" * 32,
                "oci_digest": "sha256:" + ("ef" * 32),
                "oci_reference": "ghcr.io/rudironsoni/orlix/mlibc@sha256:" + ("ef" * 32),
            },
            "rootfs": {
                "unsigned_digest": "33" * 32,
                "oci_digest": "sha256:" + ("aa" * 32),
                "oci_reference": "ghcr.io/rudironsoni/orlix/rootfs@sha256:" + ("aa" * 32),
            },
        },
    }
    payload["buildset"] = locked_buildset.buildset_digest(payload["components"])
    return payload


class LockedBuildsetTests(unittest.TestCase):
    def test_signed_digest_pinned_lock_writes_stamp(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            lock_path = Path(tmp) / "artifacts.lock.json"
            out_path = Path(tmp) / "stamp.json"
            lock_path.write_text(json.dumps(_signed_lock()) + "\n", encoding="utf-8")
            payload = locked_buildset.write_locked_buildset(str(lock_path), str(out_path))
            self.assertEqual(payload["kind"], "locked-buildset")
            self.assertEqual(payload["buildset"], _signed_lock()["buildset"])
            self.assertEqual(set(payload["components"]), {"uapi", "mlibc", "rootfs"})
            stamped = json.loads(out_path.read_text(encoding="utf-8"))
            self.assertEqual(stamped["buildset"], payload["buildset"])

    def test_empty_lock_fails_closed(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            lock_path = Path(tmp) / "artifacts.lock.json"
            lock_path.write_text(
                json.dumps({"schema": 1, "buildset": None, "components": {}}) + "\n",
                encoding="utf-8",
            )
            with self.assertRaises(locked_buildset.LockedBuildsetError) as raised:
                locked_buildset.load_locked_buildset(str(lock_path))
            self.assertIn("signed artifacts.lock.json buildset", str(raised.exception))

    def test_mutable_latest_tag_fails_closed(self) -> None:
        payload = _signed_lock()
        payload["components"]["uapi"]["oci_reference"] = "ghcr.io/rudironsoni/orlix/uapi:latest"
        with tempfile.TemporaryDirectory() as tmp:
            lock_path = Path(tmp) / "artifacts.lock.json"
            lock_path.write_text(json.dumps(payload) + "\n", encoding="utf-8")
            with self.assertRaises(locked_buildset.LockedBuildsetError) as raised:
                locked_buildset.load_locked_buildset(str(lock_path))
            self.assertIn("latest", str(raised.exception))


if __name__ == "__main__":
    unittest.main()
