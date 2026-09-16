from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from stage_v2_product import StageError, stage_product


def _manifest_bytes(entries):
    return (
        json.dumps(
            {
                "domain": "orlix.artifact.identity",
                "format": "artifact-identity-v2",
                "version": 2,
                "entries": entries,
            },
            indent=2,
            sort_keys=True,
        )
        + "\n"
    ).encode()


class StageV2ProductTests(unittest.TestCase):
    def test_stages_exactly_the_manifest_files(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            src = root / "product-src"
            (src / "include" / "linux").mkdir(parents=True)
            (src / "include" / "linux" / "a.h").write_bytes(b"header")
            (src / "include" / "b.h").write_bytes(b"other")
            (src / "stray.txt").write_bytes(b"not in manifest")
            entries = [
                {"content_sha256": "00" * 32, "mode": 0o444, "path": "include/linux/a.h", "type": "file"},
                {"content_sha256": "11" * 32, "mode": 0o444, "path": "include/b.h", "type": "file"},
            ]
            (src / "test.artifact-identity-v2.json").write_bytes(_manifest_bytes(entries))
            (src / "test.artifact-identity-v2.sha256").write_text("ab" * 32 + "\n", encoding="ascii")
            payload = stage_product(
                src / "test.artifact-identity-v2.json", src, root / "staged"
            )
            self.assertEqual(payload, {"files": 2, "digest": "ab" * 32})
            self.assertEqual((root / "staged" / "product" / "include" / "linux" / "a.h").read_bytes(), b"header")
            self.assertFalse((root / "staged" / "product" / "stray.txt").exists())
            self.assertTrue((root / "staged" / "artifact-identity-v2.json").is_file())
            self.assertTrue((root / "staged" / "artifact-identity-v2.sha256").is_file())

    def test_missing_manifest_file_fails(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            src = root / "product-src"
            src.mkdir()
            (src / "x").write_bytes(b"x")
            entries = [{"content_sha256": "00" * 32, "mode": 0o444, "path": "gone.h", "type": "file"}]
            (src / "test.artifact-identity-v2.json").write_bytes(_manifest_bytes(entries))
            (src / "test.artifact-identity-v2.sha256").write_text("ab" * 32 + "\n", encoding="ascii")
            with self.assertRaises(StageError):
                stage_product(src / "test.artifact-identity-v2.json", src, root / "staged")

    def test_unreadable_manifest_or_digest_fails(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            with self.assertRaises(StageError):
                stage_product(root / "absent.json", root, root / "staged")
            (root / "bad.json").write_text("not json", encoding="utf-8")
            with self.assertRaises(StageError):
                stage_product(root / "bad.json", root, root / "staged")

    def test_empty_file_list_fails(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "empty.json").write_bytes(_manifest_bytes([]))
            (root / "empty.sha256").write_text("ab" * 32 + "\n", encoding="ascii")
            with self.assertRaises(StageError):
                stage_product(root / "empty.json", root, root / "staged")


if __name__ == "__main__":
    unittest.main()
