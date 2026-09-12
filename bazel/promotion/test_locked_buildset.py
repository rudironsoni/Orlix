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


def _schema2_entry(name: str, digest: str, oci_hex: str, *, marker: str | None = None) -> dict:
    identity = {
        "format": locked_buildset.LEGACY_IDENTITY_FORMAT if marker else locked_buildset.ARTIFACT_IDENTITY_V2_FORMAT,
        "version": locked_buildset.LEGACY_IDENTITY_VERSION if marker else locked_buildset.ARTIFACT_IDENTITY_V2_VERSION,
        "digest": digest,
    }
    if marker:
        identity["marker"] = marker
    oci = "sha256:" + oci_hex
    return {
        "unsigned_digest": digest,
        "artifact_identity": identity,
        "oci_digest": oci,
        "oci_reference": f"ghcr.io/rudironsoni/orlix/{name}@{oci}",
    }


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

    def test_schema2_binds_typed_identities_and_all_kernel_variants(self) -> None:
        components = {
            "uapi": _schema2_entry("uapi", "11" * 32, "ab" * 32, marker="uapi.sha256"),
            "mlibc": _schema2_entry("mlibc", "22" * 32, "ac" * 32, marker="sysroot.sha256"),
            "rootfs": _schema2_entry("rootfs", "33" * 32, "ad" * 32, marker="source-input.sha256"),
        }
        for index, name in enumerate(locked_buildset.KERNEL_COMPONENTS, start=1):
            components[name] = _schema2_entry(name, f"{index:02x}" * 32, f"{index + 10:02x}" * 32)
        payload = {
            "schema": locked_buildset.SCHEMA2,
            "buildset": locked_buildset.buildset_digest(components, schema=2),
            "components": components,
        }
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "artifacts.lock.json"
            path.write_text(json.dumps(payload) + "\n", encoding="utf-8")
            loaded = locked_buildset.load_locked_buildset(str(path))
        self.assertEqual(loaded["schema"], locked_buildset.SCHEMA2)
        self.assertEqual(set(loaded["components"]), set(components))
        self.assertEqual(
            loaded["components"]["kernel-release-iphoneos"]["artifact_identity"]["format"],
            locked_buildset.ARTIFACT_IDENTITY_V2_FORMAT,
        )
        mutated = dict(components)
        mutated["kernel-release-iphoneos"] = {
            **components["kernel-release-iphoneos"],
            "unsigned_digest": "ff" * 32,
            "artifact_identity": {
                **components["kernel-release-iphoneos"]["artifact_identity"],
                "digest": "ff" * 32,
            },
        }
        self.assertNotEqual(
            locked_buildset.buildset_digest(mutated, schema=2),
            payload["buildset"],
        )
        partial = dict(components)
        partial.pop("kernel-development-iphonesimulator")
        payload["components"] = partial
        payload["buildset"] = locked_buildset.buildset_digest(partial, schema=2)
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "artifacts.lock.json"
            path.write_text(json.dumps(payload) + "\n", encoding="utf-8")
            with self.assertRaisesRegex(
                locked_buildset.LockedBuildsetError,
                "missing required components",
            ):
                locked_buildset.load_locked_buildset(str(path))

    def test_schema2_rejects_unknown_kernel_variant(self) -> None:
        with self.assertRaisesRegex(locked_buildset.LockedBuildsetError, "unsupported Kernel"):
            locked_buildset.validate_component(
                "kernel-release-ios",
                _schema2_entry("kernel-release-ios", "11" * 32, "ab" * 32),
                schema=2,
            )


if __name__ == "__main__":
    unittest.main()
