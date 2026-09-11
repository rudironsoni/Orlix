from __future__ import annotations

import json
import tarfile
import tempfile
import unittest
from pathlib import Path

import artifact_store


ENTRY = {
    "unsigned_digest": "aa" * 32,
    "oci_digest": "sha256:" + "ab" * 32,
    "oci_reference": "ghcr.io/rudironsoni/orlix/uapi@sha256:" + "ab" * 32,
}
VERIFICATION = {
    "signing_key_fingerprint": "11" * 32,
    "trust_policy_sha256": "22" * 32,
    "verification_policy_version": 1,
}


def _component_tar(root: Path) -> Path:
    source = root / "source"
    source.mkdir()
    (source / "uapi.sha256").write_text(ENTRY["unsigned_digest"] + "\n", encoding="utf-8")
    (source / "include").mkdir()
    (source / "include" / "unistd.h").write_text("/* uapi */\n", encoding="utf-8")
    blob = root / "component.tar"
    with tarfile.open(blob, "w") as archive:
        archive.add(source, arcname=".")
    return blob


class ArtifactStoreTests(unittest.TestCase):
    def test_publish_and_lookup_bind_legacy_identity_and_verification(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            blob = _component_tar(root)
            tree = root / "tree"
            tree.mkdir()
            (tree / "uapi.sha256").write_text(ENTRY["unsigned_digest"] + "\n", encoding="utf-8")
            store = artifact_store.ArtifactStore(root / "store")
            store.publish_download("uapi", ENTRY, blob, tree, VERIFICATION)
            object_dir = store.root / "objects" / "oci" / "sha256" / ("ab" * 32)
            record = json.loads((object_dir / "verification.json").read_text(encoding="utf-8"))
            self.assertEqual(record["artifact_identity"]["format"], artifact_store.LEGACY_IDENTITY_FORMAT)
            self.assertEqual(record["artifact_identity"]["version"], 1)
            self.assertEqual(record["artifact_identity"]["digest"], ENTRY["unsigned_digest"])
            self.assertEqual(store.lookup("uapi", ENTRY, VERIFICATION)["needs_reverify"], False)

    def test_corrupt_object_is_not_a_warm_hit(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            blob = _component_tar(root)
            tree = root / "tree"
            tree.mkdir()
            (tree / "uapi.sha256").write_text(ENTRY["unsigned_digest"] + "\n", encoding="utf-8")
            store = artifact_store.ArtifactStore(root / "store")
            store.publish_download("uapi", ENTRY, blob, tree, VERIFICATION)
            stored_blob = store.root / "objects" / "oci" / "sha256" / ("ab" * 32) / "component.tar"
            stored_blob.write_bytes(b"corrupt")
            self.assertIsNone(store.lookup("uapi", ENTRY, VERIFICATION))

    def test_gc_keeps_locked_and_live_leased_objects(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            store = artifact_store.ArtifactStore(root / "store")
            for digest in ("ab" * 32, "cd" * 32, "ef" * 32):
                object_dir = store.root / "objects" / "oci" / "sha256" / digest
                object_dir.mkdir(parents=True)
                (object_dir / "payload").write_bytes(digest.encode())
            live = root / "consumer"
            live.mkdir()
            live_entry = {"oci_digest": "sha256:" + "cd" * 32}
            store.write_lease("aa" * 32, [live_entry], live)
            lock = root / "artifacts.lock.json"
            lock.write_text(
                json.dumps({"components": {"uapi": {"oci_digest": "sha256:" + "ab" * 32}}}) + "\n",
                encoding="utf-8",
            )
            payload = store.gc(lock, now=100, max_age_seconds=10, max_bytes=128)
            self.assertEqual(payload["removed_objects"], 1)
            self.assertTrue((store.root / "objects" / "oci" / "sha256" / ("ab" * 32)).exists())
            self.assertTrue((store.root / "objects" / "oci" / "sha256" / ("cd" * 32)).exists())
            self.assertFalse((store.root / "objects" / "oci" / "sha256" / ("ef" * 32)).exists())
            with self.assertRaises(artifact_store.ArtifactStoreError):
                store.gc(lock, now=100, max_bytes=64)


if __name__ == "__main__":
    unittest.main()
