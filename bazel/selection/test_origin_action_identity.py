"""Contract spec for downstream consumer action identity.

Identity is artifact-identity-v2: logical path, type, mode, file bytes, and
symlink target. Origin exec paths and proof metadata are not part of that key.
Equal source and promoted bytes keep the same key. A real byte change misses.
This is not a timed build and not a product build.
"""

from __future__ import annotations

import hashlib
import json
import os
import sys
import tempfile
import unittest
from pathlib import Path


_ROOT = Path(__file__).resolve().parents[2]
if str(_ROOT) not in sys.path:
    sys.path.insert(0, str(_ROOT))

from bazel.content_digest import artifact_manifest_v2


EXCLUDED_FIELDS = (
    "destination",
    "linux_revision",
    "origin_exec_path",
    "profile",
    "source_input_digest",
    "target_triple",
)
PROOF_SIDECARS = (
    "artifacts.lock.json",
    "file-manifest.txt",
    "payload-metadata.txt",
    "source-input.sha256",
)


def _write_file(path: Path, payload: bytes, mode: int = 0o644) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(payload)
    os.chmod(path, mode)


def _product(root: Path, payload: bytes = b"kernel-bytes") -> dict[str, Path]:
    archive = root / "origin" / "OrlixKernel.a"
    dtb = root / "origin" / "nested" / "development.dtb"
    _write_file(archive, payload)
    _write_file(dtb, b"dtb")
    for name in PROOF_SIDECARS:
        _write_file(root / "origin" / name, b"proof-noise\n")
    return {
        "OrlixKernel.a": archive,
        "arch/orlix/boot/dts/development.dtb": dtb,
    }


def consumer_action_key(artifacts: dict[str, Path]) -> tuple[str, dict]:
    manifest = artifact_manifest_v2(artifacts=artifacts)
    payload = json.loads(manifest)
    if set(payload) != {"domain", "entries", "format", "version"}:
        raise AssertionError(sorted(payload))
    return hashlib.sha256(manifest).hexdigest(), payload


class OriginActionIdentityTests(unittest.TestCase):
    def test_identity_is_logical_path_type_mode_bytes_and_symlink_target(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            binary = root / "true"
            _write_file(binary, b"#!/bin/true\n", 0o755)
            link = root / "sh"
            link.symlink_to("bash")
            _key, payload = consumer_action_key({"bin/true": binary, "bin/sh": link})
            self.assertEqual(payload["format"], "artifact-identity-v2")
            self.assertEqual(payload["version"], 2)
            entries = {entry["path"]: entry for entry in payload["entries"]}
            self.assertEqual(entries["bin/true"]["type"], "file")
            self.assertEqual(entries["bin/true"]["mode"], 0o755)
            self.assertEqual(len(entries["bin/true"]["content_sha256"]), 64)
            self.assertNotIn("target", entries["bin/true"])
            self.assertEqual(entries["bin/sh"]["type"], "symlink")
            self.assertEqual(entries["bin/sh"]["target"], "bash")
            self.assertNotIn("content_sha256", entries["bin/sh"])
            for field in EXCLUDED_FIELDS:
                self.assertNotIn(field, payload)
                for entry in payload["entries"]:
                    self.assertNotIn(field, entry)

    def test_origin_exec_path_is_not_a_consumer_input(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = _product(root / "source-exec")
            promoted = _product(root / "promoted-exec")
            source_key, source_payload = consumer_action_key(source)
            promoted_key, promoted_payload = consumer_action_key(promoted)
            self.assertEqual(source_key, promoted_key)
            self.assertEqual(
                [entry["path"] for entry in source_payload["entries"]],
                [entry["path"] for entry in promoted_payload["entries"]],
            )
            encoded = json.dumps(source_payload)
            self.assertNotIn("source-exec", encoded)
            self.assertNotIn("promoted-exec", encoded)
            self.assertIn("arch/orlix/boot/dts/development.dtb", encoded)
            absolute = next(iter(source.values()))
            with self.assertRaises(ValueError):
                artifact_manifest_v2(artifacts={str(absolute): absolute})

    def test_equal_source_and_promoted_bytes_share_a_key_until_bytes_change(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = _product(root / "source", b"same-bytes")
            promoted = _product(root / "promoted", b"same-bytes")
            source_key, source_payload = consumer_action_key(source)
            promoted_key, _promoted_payload = consumer_action_key(promoted)
            self.assertEqual(source_key, promoted_key)
            self.assertEqual(
                [entry["path"] for entry in source_payload["entries"]],
                ["OrlixKernel.a", "arch/orlix/boot/dts/development.dtb"],
            )
            changed = _product(root / "changed", b"different-bytes")
            changed_key, _changed_payload = consumer_action_key(changed)
            self.assertNotEqual(source_key, changed_key)

    def test_proof_sidecar_and_origin_metadata_do_not_change_the_key(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            selected = _product(root)
            key, payload = consumer_action_key(selected)
            for name in PROOF_SIDECARS:
                (root / "origin" / name).write_bytes(b"different-proof\n")
            self.assertEqual(consumer_action_key(selected)[0], key)
            sidecar = root / "origin" / "source-input.sha256"
            with_sidecar = dict(selected)
            with_sidecar["source-input.sha256"] = sidecar
            self.assertNotEqual(consumer_action_key(with_sidecar)[0], key)
            encoded = json.dumps(payload)
            for field in EXCLUDED_FIELDS:
                self.assertNotIn(field, encoded)

    def test_mode_and_symlink_target_are_part_of_the_key(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            first = root / "a"
            second = root / "b"
            _write_file(first, b"same", 0o644)
            _write_file(second, b"same", 0o755)
            mode_a = consumer_action_key({"bin/tool": first})[0]
            mode_b = consumer_action_key({"bin/tool": second})[0]
            self.assertNotEqual(mode_a, mode_b)
            link_a = root / "link-a"
            link_b = root / "link-b"
            link_a.symlink_to("bash")
            link_b.symlink_to("dash")
            self.assertNotEqual(
                consumer_action_key({"bin/sh": link_a})[0],
                consumer_action_key({"bin/sh": link_b})[0],
            )


if __name__ == "__main__":
    unittest.main()
