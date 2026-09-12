from __future__ import annotations

import json
import shutil
import tarfile
import tempfile
import threading
import time
import unittest
from pathlib import Path
from unittest import mock

import artifact_store
import locked_buildset
import reconstruct


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


def _write_lock(path: Path, entry: dict = ENTRY) -> None:
    components = {"uapi": entry}
    path.write_text(
        json.dumps(
            {
                "schema": 1,
                "buildset": locked_buildset.buildset_digest(components),
                "components": components,
            }
        )
        + "\n",
        encoding="utf-8",
    )


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
            _write_lock(lock)
            payload = store.gc(lock, now=100, max_age_seconds=10, max_bytes=128)
            self.assertEqual(payload["removed_objects"], 1)
            self.assertTrue((store.root / "objects" / "oci" / "sha256" / ("ab" * 32)).exists())
            self.assertTrue((store.root / "objects" / "oci" / "sha256" / ("cd" * 32)).exists())
            self.assertFalse((store.root / "objects" / "oci" / "sha256" / ("ef" * 32)).exists())
            with self.assertRaises(artifact_store.ArtifactStoreError):
                store.gc(lock, now=100, max_bytes=64)

    def test_concurrent_reader_and_gc_synchronization(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            blob = _component_tar(root)
            tree = root / "tree"
            tree.mkdir()
            (tree / "uapi.sha256").write_text(ENTRY["unsigned_digest"] + "\n", encoding="utf-8")
            store = artifact_store.ArtifactStore(root / "store")
            store.publish_download("uapi", ENTRY, blob, tree, VERIFICATION)

            dest = root / "destination"
            copy_started = threading.Event()
            proceed_copy = threading.Event()
            gc_finished = threading.Event()
            gc_result = {}
            original_copytree = artifact_store.shutil.copytree

            def slow_copytree(src, dst, **kwargs):
                copy_started.set()
                proceed_copy.wait(timeout=5.0)
                return original_copytree(src, dst, **kwargs)

            def run_gc():
                res = store.gc(now=time.time() + 1000, max_age_seconds=10)
                gc_result.update(res)
                gc_finished.set()

            with mock.patch("artifact_store.shutil.copytree", side_effect=slow_copytree):
                t_reader = threading.Thread(
                    target=lambda: store.materialize_local("uapi", ENTRY, VERIFICATION, dest)
                )
                t_gc = threading.Thread(target=run_gc)
                t_reader.start()
                self.assertTrue(copy_started.wait(timeout=5.0))

                t_gc.start()
                time.sleep(0.05)
                self.assertFalse(gc_finished.is_set())

                proceed_copy.set()
                t_reader.join(timeout=5.0)

                t_gc.join(timeout=5.0)
                self.assertTrue(gc_finished.is_set())

            self.assertTrue((dest / "uapi.sha256").is_file())
            self.assertEqual(
                (dest / "uapi.sha256").read_text(encoding="utf-8").strip(),
                ENTRY["unsigned_digest"],
            )
            self.assertEqual(gc_result.get("removed_objects"), 1)

    def test_replacement_does_not_retain_corrupt_bytes(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            blob = _component_tar(root)
            tree = root / "tree"
            tree.mkdir()
            (tree / "uapi.sha256").write_text(ENTRY["unsigned_digest"] + "\n", encoding="utf-8")
            store = artifact_store.ArtifactStore(root / "store")
            store.publish_download("uapi", ENTRY, blob, tree, VERIFICATION)

            stored_dir = store.root / "objects" / "oci" / "sha256" / ("ab" * 32)
            (stored_dir / "component.tar").write_bytes(b"corrupt")
            store.publish_download("uapi", ENTRY, blob, tree, VERIFICATION)
            self.assertEqual(
                (stored_dir / "component.tar").read_bytes(), blob.read_bytes()
            )
            self.assertFalse((store.root / "quarantine").exists())

    def test_durable_live_consumer_pin_across_gc(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            store = artifact_store.ArtifactStore(root / "store")
            digest = "ab" * 32
            object_dir = store.root / "objects" / "oci" / "sha256" / digest
            object_dir.mkdir(parents=True)
            (object_dir / "payload").write_bytes(digest.encode())

            consumer = root / "active_worktree_consumer"
            consumer.mkdir()
            entry = {"oci_digest": "sha256:" + digest}
            lease_path = store.write_lease("bb" * 32, [entry], consumer)
            malformed = store.root / "leases" / "bad.json"
            malformed.mkdir()

            now = time.time() + (60 * 86400)
            payload = store.gc(lock_path=None, now=now, max_age_seconds=30 * 86400)
            self.assertEqual(payload["removed_objects"], 0)
            self.assertTrue(object_dir.exists())
            self.assertTrue(lease_path.exists())
            self.assertFalse(malformed.exists())

            shutil.rmtree(consumer)

            payload2 = store.gc(lock_path=None, now=now, max_age_seconds=30 * 86400)
            self.assertEqual(payload2["removed_objects"], 1)
            self.assertFalse(object_dir.exists())
            self.assertFalse(lease_path.exists())

    def test_malformed_metadata_recovers_via_reacquisition(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            store = artifact_store.ArtifactStore(root / "store")
            object_dir = store.root / "objects" / "oci" / "sha256" / ("ab" * 32)
            object_dir.mkdir(parents=True)
            (object_dir / "component.tar").write_bytes(b"blob")
            (object_dir / "manifest.json").write_text("{}", encoding="utf-8")
            (object_dir / "tree").mkdir()

            (object_dir / "verification.json").write_text("[]\n", encoding="utf-8")
            self.assertIsNone(store.lookup("uapi", ENTRY, VERIFICATION))

            (object_dir / "verification.json").write_text(
                json.dumps({"schema": 1, "kind": "orlix-promoted-artifact", "artifact_identity": None}) + "\n",
                encoding="utf-8",
            )
            self.assertIsNone(store.lookup("uapi", ENTRY, VERIFICATION))

            (object_dir / "verification.json").write_text(
                json.dumps({
                    "schema": 1,
                    "kind": "orlix-promoted-artifact",
                    "artifact_identity": {
                        "format": artifact_store.LEGACY_IDENTITY_FORMAT,
                        "version": 1,
                        "digest": ENTRY["unsigned_digest"],
                        "marker": None,
                    },
                }) + "\n",
                encoding="utf-8",
            )
            self.assertIsNone(store.lookup("uapi", ENTRY, VERIFICATION))

            lock_file = root / "artifacts.lock.json"
            lock_payload = {
                "schema": 1,
                "buildset": "cc" * 32,
                "components": {
                    "uapi": {
                        "unsigned_digest": ENTRY["unsigned_digest"],
                        "oci_digest": ENTRY["oci_digest"],
                        "oci_reference": ENTRY["oci_reference"],
                    }
                },
            }
            lock_payload["buildset"] = locked_buildset.buildset_digest(lock_payload["components"])
            lock_file.write_text(json.dumps(lock_payload) + "\n", encoding="utf-8")

            def fake_run(argv: list[str], env=None, cwd=None):
                if argv[0] == "oras" and "pull" in argv:
                    dest = Path(argv[argv.index("-o") + 1])
                    with tarfile.open(dest / "component.tar", "w") as archive:
                        marker = dest / "uapi.sha256"
                        marker.write_text(ENTRY["unsigned_digest"] + "\n", encoding="utf-8")
                        archive.add(marker, arcname="uapi.sha256")

                class Result:
                    stdout = ""

                return Result()

            with mock.patch("reconstruct.verification_context", return_value=VERIFICATION), \
                 mock.patch("publish.trusted_public_key", return_value="/unused.pub"), \
                 mock.patch("reconstruct.shutil.which", return_value="/usr/bin/tool"):
                out_dir = root / "reconstruct"
                res = reconstruct.reconstruct(str(lock_file), str(out_dir), run=fake_run, store_root=store.root)

            self.assertIn("uapi", res["components"])
            self.assertTrue(Path(res["components"]["uapi"]["tree"]).exists())
            self.assertIsNotNone(store.lookup("uapi", ENTRY, VERIFICATION))


if __name__ == "__main__":
    unittest.main()
