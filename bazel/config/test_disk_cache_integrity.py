from __future__ import annotations

import hashlib
import os
import tempfile
import unittest
from pathlib import Path
from unittest import mock

import disk_cache_integrity as integrity


def _blob(root: Path, payload: bytes, *, name: str | None = None) -> Path:
    digest = name or hashlib.sha256(payload).hexdigest()
    path = root / "cas" / digest[:2] / digest
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(payload)
    return path


class DiskCacheIntegrityTests(unittest.TestCase):
    def test_corrupt_blob_is_removed_and_a_matching_blob_stays(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            good = _blob(root, b"action-output")
            corrupt = _blob(root, b"corrupt", name="ab" * 32)
            previous = root / "previously-generated"
            previous.write_bytes(b"local output")
            record = root / "ac" / "cd" / ("cd" * 32)
            record.parent.mkdir(parents=True)
            record.write_bytes(b"action-result")

            result = integrity.discard_corrupt_entries(root)

            self.assertEqual(good.read_bytes(), b"action-output")
            self.assertFalse(corrupt.exists())
            self.assertEqual(previous.read_bytes(), b"local output")
            self.assertEqual(record.read_bytes(), b"action-result")
            self.assertEqual(result["checked"], 2)
            self.assertEqual(result["removed"], 1)
            self.assertFalse(result["missing"])

    def test_missing_cache_is_not_created(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary) / "disk-cache"
            result = integrity.discard_corrupt_entries(root)
            self.assertEqual(result, {"checked": 0, "removed": 0, "missing": True})
            self.assertFalse(root.exists())

    def test_symlink_blob_is_removed_without_following_it(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            outside = root / "outside"
            outside.write_bytes(b"elsewhere")
            link = root / "cas" / "ab" / ("ab" * 32)
            link.parent.mkdir(parents=True)
            link.symlink_to(outside)
            result = integrity.discard_corrupt_entries(root)
            self.assertFalse(link.exists())
            self.assertEqual(outside.read_bytes(), b"elsewhere")
            self.assertEqual(result["removed"], 1)
            self.assertEqual(result["checked"], 0)

    def test_nested_digest_cas_uses_the_same_rule(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            payload = b"nested"
            name = hashlib.sha256(payload).hexdigest()
            good = root / "sha256" / "cas" / name[:2] / name
            good.parent.mkdir(parents=True)
            good.write_bytes(payload)
            corrupt = root / "sha256" / "cas" / "ab" / ("ab" * 32)
            corrupt.parent.mkdir(parents=True, exist_ok=True)
            corrupt.write_bytes(b"corrupt")
            integrity.discard_corrupt_entries(root)
            self.assertEqual(good.read_bytes(), payload)
            self.assertFalse(corrupt.exists())

    def test_same_run_rechecks_a_blob_after_its_mtime_changes(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            path = _blob(root, b"action-output")
            with mock.patch.dict(os.environ, {"ORLIX_BAZEL_RUN_ID": "run-1"}):
                integrity.discard_corrupt_entries(root)
                integrity.discard_corrupt_entries(root)
                self.assertEqual(path.read_bytes(), b"action-output")
                path.write_bytes(b"corrupt")
                integrity.discard_corrupt_entries(root)
            self.assertFalse(path.exists())

    def test_symlinked_cas_directory_is_removed_without_following_it(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            base = Path(temporary)
            root = base / "cache"
            root.mkdir()
            outside = base / "outside"
            blob = outside / "ab" / ("ab" * 32)
            blob.parent.mkdir(parents=True)
            blob.write_bytes(b"corrupt")
            link = root / "cas"
            link.symlink_to(outside, target_is_directory=True)
            result = integrity.discard_corrupt_entries(root)
            self.assertFalse(link.exists())
            self.assertFalse(link.is_symlink())
            self.assertEqual(blob.read_bytes(), b"corrupt")
            self.assertEqual(result["removed"], 1)
            self.assertEqual(result["checked"], 0)

    def test_symlinked_digest_root_is_removed_without_following_it(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            base = Path(temporary)
            root = base / "cache"
            root.mkdir()
            outside = base / "outside"
            blob = outside / "cas" / "ab" / ("ab" * 32)
            blob.parent.mkdir(parents=True)
            blob.write_bytes(b"corrupt")
            link = root / "sha256"
            link.symlink_to(outside, target_is_directory=True)
            result = integrity.discard_corrupt_entries(root)
            self.assertFalse(link.exists())
            self.assertFalse(link.is_symlink())
            self.assertEqual(blob.read_bytes(), b"corrupt")
            self.assertEqual(result["removed"], 1)
            self.assertEqual(result["checked"], 0)

    def test_symlinked_nested_cas_is_removed_without_following_it(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            base = Path(temporary)
            root = base / "cache"
            root.mkdir()
            outside = base / "outside"
            blob = outside / "ab" / ("ab" * 32)
            blob.parent.mkdir(parents=True)
            blob.write_bytes(b"corrupt")
            digest = root / "sha256"
            digest.mkdir()
            link = digest / "cas"
            link.symlink_to(outside, target_is_directory=True)
            result = integrity.discard_corrupt_entries(root)
            self.assertTrue(digest.is_dir())
            self.assertFalse(link.exists())
            self.assertFalse(link.is_symlink())
            self.assertEqual(blob.read_bytes(), b"corrupt")
            self.assertEqual(result["removed"], 1)
            self.assertEqual(result["checked"], 0)

    def test_symlink_root_fails_closed(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            target = root / "real"
            target.mkdir()
            link = root / "link"
            link.symlink_to(target, target_is_directory=True)
            with self.assertRaises(integrity.DiskCacheIntegrityError):
                integrity.discard_corrupt_entries(link)
            self.assertTrue(target.is_dir())


if __name__ == "__main__":
    unittest.main()
