from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path

import substitute


def _signed_lock() -> dict:
    return {
        "schema": 1,
        "buildset": "cd" * 32,
        "components": {
            "uapi": {
                "unsigned_digest": "11" * 32,
                "oci_digest": "sha256:" + ("ab" * 32),
                "oci_reference": "localhost:5001/orlix/uapi@sha256:" + ("ab" * 32),
            },
            "mlibc": {
                "unsigned_digest": "22" * 32,
                "oci_digest": "sha256:" + ("ef" * 32),
                "oci_reference": "localhost:5001/orlix/mlibc@sha256:" + ("ef" * 32),
            },
            "rootfs": {
                "unsigned_digest": "33" * 32,
                "oci_digest": "sha256:" + ("aa" * 32),
                "oci_reference": "localhost:5001/orlix/rootfs@sha256:" + ("aa" * 32),
            },
        },
    }


def _write_tree(root: Path, name: str, digest: str) -> None:
    tree = root / name
    tree.mkdir(parents=True)
    (tree / f"{name}.sha256").write_text(digest + "\n", encoding="utf-8")


class SubstituteTests(unittest.TestCase):
    def test_maps_reconstructed_trees_from_signed_lock(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            lock_path = Path(tmp) / "artifacts.lock.json"
            lock_payload = _signed_lock()
            lock_path.write_text(json.dumps(lock_payload) + "\n", encoding="utf-8")
            reconstruct = Path(tmp) / "reconstruct"
            root = reconstruct / ("cd" * 32)
            _write_tree(root, "uapi", "11" * 32)
            _write_tree(root, "mlibc", "22" * 32)
            _write_tree(root, "rootfs", "33" * 32)
            out_path = Path(tmp) / "promoted-components.json"
            before = lock_path.read_bytes()
            payload = substitute.substitute(str(lock_path), str(reconstruct), str(out_path))
            self.assertEqual(payload["kind"], "promoted-components")
            self.assertEqual(payload["buildset"], "cd" * 32)
            self.assertEqual(set(payload["components"]), {"uapi", "mlibc", "rootfs"})
            self.assertEqual(
                payload["components"]["uapi"]["oci_reference"],
                lock_payload["components"]["uapi"]["oci_reference"],
            )
            stamped = json.loads(out_path.read_text(encoding="utf-8"))
            self.assertEqual(stamped["components"]["rootfs"]["unsigned_digest"], "33" * 32)
            self.assertEqual(lock_path.read_bytes(), before)
            stage = Path(tmp) / "imported"
            substitute.stage_imported(payload, str(stage))
            self.assertTrue((stage / "uapi").is_symlink())
            self.assertEqual(
                (stage / "uapi").resolve(),
                Path(payload["components"]["uapi"]["tree"]).resolve(),
            )
            self.assertTrue((stage / "rootfs" / "rootfs.sha256").is_file())

    def test_digest_mismatch_fails_closed(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            lock_path = Path(tmp) / "artifacts.lock.json"
            lock_path.write_text(json.dumps(_signed_lock()) + "\n", encoding="utf-8")
            reconstruct = Path(tmp) / "reconstruct"
            root = reconstruct / ("cd" * 32)
            _write_tree(root, "uapi", "99" * 32)
            _write_tree(root, "mlibc", "22" * 32)
            _write_tree(root, "rootfs", "33" * 32)
            with self.assertRaises(substitute.SubstituteError) as raised:
                substitute.substitute(
                    str(lock_path), str(reconstruct), str(Path(tmp) / "out.json")
                )
            self.assertIn("missing unsigned digest", str(raised.exception))


if __name__ == "__main__":
    unittest.main()
