#!/usr/bin/env python3
"""Keep verified, digest-addressed promoted component trees locally."""

from __future__ import annotations

import argparse
from contextlib import contextmanager
import fcntl
import hashlib
import json
import math
import os
import shutil
import tempfile
import time
import uuid
from pathlib import Path

from compare import tree_digest
from locked_buildset import (
    ARTIFACT_IDENTITY_V2_FORMAT,
    LEGACY_IDENTITY_FORMAT,
    LEGACY_IDENTITY_VERSION,
    entry_artifact_digest,
    load_locked_buildset,
    validate_artifact_identity,
    validate_v2_product,
)


STORE_SCHEMA = 1
DEFAULT_STORE = Path.home() / "Library" / "Caches" / "Orlix" / "Artifacts"
MAX_BYTES = 30 * 1024 * 1024 * 1024
MAX_AGE_SECONDS = 30 * 24 * 60 * 60


class ArtifactStoreError(ValueError):
    pass


def store_from_environment() -> "ArtifactStore":
    return ArtifactStore(os.environ.get("ORLIX_PROMOTED_ARTIFACT_STORE", str(DEFAULT_STORE)))


def _sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _oci_digest_hex(digest: str) -> str:
    value = str(digest).strip()
    if not value.startswith("sha256:") or len(value) != len("sha256:") + 64:
        raise ArtifactStoreError(f"invalid OCI digest: {digest!r}")
    value = value[len("sha256:") :]
    if any(char not in "0123456789abcdef" for char in value):
        raise ArtifactStoreError(f"invalid OCI digest: {digest!r}")
    return value


def _valid_digest(value: object) -> bool:
    return isinstance(value, str) and len(value) == 64 and all(
        char in "0123456789abcdef" for char in value
    )


def _valid_timestamp(value: object) -> bool:
    return (
        isinstance(value, (int, float))
        and not isinstance(value, bool)
        and math.isfinite(value)
    )


def _write_json(path: Path, payload: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    pending = path.with_name(f".{path.name}.{uuid.uuid4().hex}.tmp")
    try:
        pending.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
        os.replace(pending, path)
    finally:
        pending.unlink(missing_ok=True)


def _unsigned_marker(tree: Path, expected: str, component: str) -> str:
    for path in sorted(tree.rglob("*.sha256")):
        try:
            if path.is_file() and path.read_text(encoding="utf-8").strip() == expected:
                return path.relative_to(tree).as_posix()
        except (OSError, UnicodeError) as error:
            raise ArtifactStoreError(f"cannot inspect {component} unsigned digest marker") from error
    raise ArtifactStoreError(
        f"{component} reconstructed tree is missing unsigned digest {expected}"
    )


def _safe_relative(path: str) -> bool:
    if not isinstance(path, str):
        return False
    candidate = Path(path)
    return bool(path) and not candidate.is_absolute() and ".." not in candidate.parts


class ArtifactStore:
    def __init__(self, root: str | Path) -> None:
        self.root = Path(root).expanduser()

    @contextmanager
    def _locked(self):
        self.root.mkdir(parents=True, exist_ok=True)
        lock_path = self.root / ".store.lock"
        with lock_path.open("a+") as lock:
            fcntl.flock(lock.fileno(), fcntl.LOCK_EX)
            try:
                self._cleanup_staging_unlocked()
                yield
            finally:
                fcntl.flock(lock.fileno(), fcntl.LOCK_UN)

    def _cleanup_staging_unlocked(self) -> None:
        staging = self.root / ".staging"
        if not staging.is_dir() or staging.is_symlink():
            return
        for path in staging.iterdir():
            if path.is_symlink() or path.is_file():
                path.unlink()
            elif path.is_dir():
                shutil.rmtree(path)

    def _object_dir(self, entry: dict) -> Path:
        return self.root / "objects" / "oci" / "sha256" / _oci_digest_hex(entry["oci_digest"])

    @staticmethod
    def _expected_manifest(component: str, entry: dict) -> dict:
        return {
            "schema": STORE_SCHEMA,
            "kind": "oci-manifest-identity",
            "component": component,
            "oci_digest": entry["oci_digest"],
            "oci_reference": entry["oci_reference"],
        }

    @staticmethod
    def _expected_identity(entry: dict, marker: str) -> dict:
        if entry.get("artifact_identity") is not None:
            return validate_artifact_identity(entry["artifact_identity"])
        return {
            "format": LEGACY_IDENTITY_FORMAT,
            "version": LEGACY_IDENTITY_VERSION,
            "digest": entry["unsigned_digest"],
            "marker": marker,
        }

    def _validate(self, component: str, entry: dict) -> dict:
        object_dir = self._object_dir(entry)
        if object_dir.is_symlink() or not object_dir.is_dir():
            raise ArtifactStoreError("promoted artifact object is missing")
        record_path = object_dir / "verification.json"
        manifest_path = object_dir / "manifest.json"
        blob = object_dir / "component.tar"
        tree = object_dir / "tree"
        if not record_path.is_file() or not manifest_path.is_file() or not blob.is_file():
            raise ArtifactStoreError("promoted artifact object is incomplete")
        if tree.is_symlink() or not tree.is_dir():
            raise ArtifactStoreError("promoted artifact tree is missing")
        try:
            record = json.loads(record_path.read_text(encoding="utf-8"))
            manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        except (OSError, ValueError) as error:
            raise ArtifactStoreError("promoted artifact metadata is invalid") from error
        if not isinstance(record, dict) or not isinstance(manifest, dict):
            raise ArtifactStoreError("promoted artifact metadata must be JSON objects")
        if record.get("schema") != STORE_SCHEMA or record.get("kind") != "orlix-promoted-artifact":
            raise ArtifactStoreError("promoted artifact verification schema is invalid")
        if not isinstance(record.get("component"), str) or record.get("component") != component:
            raise ArtifactStoreError("promoted artifact component does not match the lock")
        try:
            expected_digest = entry_artifact_digest(entry)
        except (KeyError, TypeError, ValueError) as error:
            raise ArtifactStoreError("promoted artifact artifact identity is invalid") from error
        if not _valid_digest(record.get("unsigned_digest")) or record.get("unsigned_digest") != expected_digest:
            raise ArtifactStoreError("promoted artifact digest does not match the lock")
        if not isinstance(record.get("oci_digest"), str) or not isinstance(record.get("oci_reference"), str) or record.get("oci_digest") != entry["oci_digest"] or record.get("oci_reference") != entry["oci_reference"]:
            raise ArtifactStoreError("promoted artifact OCI identity does not match the lock")
        if manifest != self._expected_manifest(component, entry):
            raise ArtifactStoreError("promoted artifact manifest identity does not match the lock")
        oci_manifest = record.get("oci_manifest")
        if not isinstance(oci_manifest, dict) or oci_manifest != {
            "digest": entry["oci_digest"],
            "reference": entry["oci_reference"],
        }:
            raise ArtifactStoreError("promoted artifact manifest record does not match the lock")
        identity = record.get("artifact_identity")
        if not isinstance(identity, dict):
            raise ArtifactStoreError("promoted artifact identity must be a JSON object")
        try:
            locked_identity = validate_artifact_identity(
                entry["artifact_identity"]
            ) if entry.get("artifact_identity") is not None else None
            stored_identity = validate_artifact_identity(identity)
        except (KeyError, TypeError, ValueError) as error:
            raise ArtifactStoreError("promoted artifact identity is invalid") from error
        if locked_identity is not None and stored_identity != locked_identity:
            raise ArtifactStoreError("promoted artifact identity does not match the lock")
        if stored_identity["digest"] != expected_digest:
            raise ArtifactStoreError("promoted artifact identity digest does not match the lock")
        marker = stored_identity.get("marker")
        if stored_identity["format"] == LEGACY_IDENTITY_FORMAT:
            marker_path = tree / marker
            if not marker_path.is_file() or marker_path.is_symlink():
                raise ArtifactStoreError("promoted artifact unsigned digest marker is missing")
            try:
                tree_real = tree.resolve()
                marker_real = marker_path.resolve()
                if os.path.commonpath((str(tree_real), str(marker_real))) != str(tree_real):
                    raise ArtifactStoreError("promoted artifact marker escapes its tree")
                marker_text = marker_path.read_text(encoding="utf-8").strip()
            except (OSError, UnicodeError) as error:
                raise ArtifactStoreError("promoted artifact unsigned digest marker is unreadable") from error
            if marker_text != expected_digest:
                raise ArtifactStoreError("promoted artifact unsigned digest marker differs from the lock")
        else:
            try:
                validate_v2_product(tree, stored_identity, component)
            except (OSError, TypeError, ValueError) as error:
                raise ArtifactStoreError(str(error)) from error
        blob_record = record.get("blob")
        if not isinstance(blob_record, dict):
            raise ArtifactStoreError("promoted artifact blob record must be a JSON object")
        if blob_record.get("path") != "component.tar" or not _valid_digest(blob_record.get("sha256")) or blob_record.get("sha256") != _sha256_file(blob):
            raise ArtifactStoreError("promoted artifact tar digest is invalid")
        if not isinstance(blob_record.get("size"), int) or isinstance(blob_record.get("size"), bool) or blob_record.get("size") < 0 or blob_record.get("size") != blob.stat().st_size:
            raise ArtifactStoreError("promoted artifact tar size is invalid")
        tree_record = record.get("tree")
        if not isinstance(tree_record, dict):
            raise ArtifactStoreError("promoted artifact tree record must be a JSON object")
        if tree_record.get("path") != "tree" or not _valid_digest(tree_record.get("sha256")) or tree_record.get("sha256") != tree_digest(tree):
            raise ArtifactStoreError("promoted artifact tree digest is invalid")
        verification = record.get("verification")
        if not isinstance(verification, dict) or not _valid_digest(verification.get("signing_key_fingerprint")) or not _valid_digest(verification.get("trust_policy_sha256")) or not isinstance(verification.get("verification_policy_version"), int) or isinstance(verification.get("verification_policy_version"), bool) or not _valid_timestamp(record.get("last_used_at")):
            raise ArtifactStoreError("promoted artifact verification metadata is invalid")
        return {
            "object": object_dir,
            "tree": tree,
            "marker": marker,
            "artifact_identity": stored_identity,
            "record": record,
            "verification": verification,
        }

    def lookup(self, component: str, entry: dict, verification: dict | None = None) -> dict | None:
        with self._locked():
            found = self._lookup_unlocked(component, entry, verification)
            if found is not None and not found["needs_reverify"]:
                self._touch_unlocked(found)
            return found

    def _lookup_unlocked(
        self, component: str, entry: dict, verification: dict | None = None
    ) -> dict | None:
        try:
            found = self._validate(component, entry)
        except (ArtifactStoreError, OSError, TypeError, ValueError, AttributeError, KeyError):
            return None
        found["needs_reverify"] = verification is not None and found["verification"] != verification
        return found

    @staticmethod
    def _touch_unlocked(found: dict) -> None:
        record = dict(found["record"])
        record["last_used_at"] = time.time()
        _write_json(found["object"] / "verification.json", record)
        found["record"] = record

    def materialize_local(
        self,
        component: str,
        entry: dict,
        verification: dict,
        destination: str | Path,
        reverify=None,
    ) -> dict | None:
        with self._locked():
            found = self._lookup_unlocked(component, entry, verification)
            if found is None:
                return None
            if found["needs_reverify"]:
                if reverify is None:
                    raise ArtifactStoreError("promoted artifact requires signature reverification")
                reverify()
                found = self._refresh_unlocked(component, entry, verification)
            else:
                self._touch_unlocked(found)
            target = Path(destination)
            if target.is_symlink() or target.exists():
                raise ArtifactStoreError(f"local materialization destination already exists: {target}")
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copytree(found["tree"], target, symlinks=True)
            return {
                "marker": found["marker"],
                "artifact_identity": found["artifact_identity"],
            }

    def _refresh_unlocked(self, component: str, entry: dict, verification: dict) -> dict:
        found = self._validate(component, entry)
        record = dict(found["record"])
        record["verification"] = verification
        record["last_used_at"] = time.time()
        _write_json(found["object"] / "verification.json", record)
        found["record"] = record
        found["verification"] = verification
        found["needs_reverify"] = False
        return found

    def publish_download(
        self,
        component: str,
        entry: dict,
        blob: str | Path,
        tree: str | Path,
        verification: dict,
    ) -> dict:
        blob_path = Path(blob)
        tree_path = Path(tree)
        if not blob_path.is_file():
            raise ArtifactStoreError(f"{component} pull did not write component.tar")
        if tree_path.is_symlink() or not tree_path.is_dir():
            raise ArtifactStoreError(f"{component} extracted tree is missing")
        with self._locked():
            staging_parent = self.root / ".staging"
            staging_parent.mkdir(parents=True, exist_ok=True)
            staging = Path(tempfile.mkdtemp(prefix="artifact-", dir=staging_parent))
            staged_object = staging / "object"
            staged_tree = staged_object / "tree"
            try:
                staged_object.mkdir()
                shutil.copyfile(blob_path, staged_object / "component.tar")
                shutil.copytree(tree_path, staged_tree, symlinks=True)
                if entry.get("artifact_identity") is not None:
                    identity = validate_artifact_identity(entry["artifact_identity"])
                    if identity["format"] == ARTIFACT_IDENTITY_V2_FORMAT:
                        validate_v2_product(staged_tree, identity, component)
                    else:
                        marker = identity["marker"]
                        marker_path = staged_tree / marker
                        if not marker_path.is_file() or marker_path.is_symlink() or marker_path.read_text(encoding="utf-8").strip() != identity["digest"]:
                            raise ArtifactStoreError("promoted artifact legacy identity marker differs from the lock")
                else:
                    marker = _unsigned_marker(staged_tree, entry_artifact_digest(entry), component)
                    identity = self._expected_identity(entry, marker)
                tree_digest_value = tree_digest(staged_tree)
                blob_digest = _sha256_file(staged_object / "component.tar")
                record = {
                    "schema": STORE_SCHEMA,
                    "kind": "orlix-promoted-artifact",
                    "component": component,
                    "unsigned_digest": entry_artifact_digest(entry),
                    "oci_digest": entry["oci_digest"],
                    "oci_reference": entry["oci_reference"],
                    "artifact_identity": identity,
                    "oci_manifest": {
                        "digest": entry["oci_digest"],
                        "reference": entry["oci_reference"],
                    },
                    "blob": {
                        "path": "component.tar",
                        "sha256": blob_digest,
                        "size": (staged_object / "component.tar").stat().st_size,
                    },
                    "tree": {"path": "tree", "sha256": tree_digest_value},
                    "verification": verification,
                    "last_used_at": time.time(),
                }
                (staged_object / "manifest.json").write_text(
                    json.dumps(self._expected_manifest(component, entry), indent=2) + "\n",
                    encoding="utf-8",
                )
                _write_json(staged_object / "verification.json", record)
                destination = self._object_dir(entry)
                destination.parent.mkdir(parents=True, exist_ok=True)
                if destination.is_symlink() or destination.exists():
                    replaced = staging / "replaced"
                    os.replace(destination, replaced)
                    try:
                        os.replace(staged_object, destination)
                    except OSError:
                        os.replace(replaced, destination)
                        raise
                    shutil.rmtree(replaced, ignore_errors=True)
                else:
                    os.replace(staged_object, destination)
                return self._validate(component, entry)
            finally:
                shutil.rmtree(staging, ignore_errors=True)

    def write_lease(self, buildset: str, entries: list[dict], consumer: str | Path) -> Path:
        if len(buildset) != 64 or any(char not in "0123456789abcdef" for char in buildset):
            raise ArtifactStoreError("invalid buildset for promoted artifact lease")
        consumer_path = Path(consumer).expanduser().resolve()
        token = hashlib.sha256(str(consumer_path).encode("utf-8")).hexdigest()
        lease = {
            "schema": STORE_SCHEMA,
            "kind": "active-reconstruction",
            "buildset": buildset,
            "oci_digests": sorted(entry["oci_digest"] for entry in entries),
            "consumer": str(consumer_path),
            "updated_at": time.time(),
        }
        with self._locked():
            path = self.root / "leases" / f"{token}.json"
            _write_json(path, lease)
            return path

    def _pinned_digests(
        self,
        lock_path: str | Path | None,
    ) -> set[str]:
        pinned: set[str] = set()
        if lock_path:
            try:
                payload = json.loads(Path(lock_path).read_text(encoding="utf-8"))
                if not isinstance(payload, dict) or not isinstance(payload.get("components"), dict):
                    raise ArtifactStoreError("promoted lock must contain a component object")
                required = tuple(payload["components"]) if payload.get("schema", 1) == 1 else None
                locked = load_locked_buildset(
                    str(lock_path), required=required
                )
                pinned.update(entry["oci_digest"] for entry in locked["components"].values())
            except (KeyError, OSError, ValueError) as error:
                raise ArtifactStoreError(f"cannot read promoted lock for garbage collection: {lock_path}") from error
        leases = self.root / "leases"
        if leases.is_dir():
            for path in leases.glob("*.json"):
                try:
                    if path.is_symlink() or not path.is_file():
                        raise ArtifactStoreError("invalid promoted artifact lease")
                    lease = json.loads(path.read_text(encoding="utf-8"))
                    if not isinstance(lease, dict) or lease.get("schema") != STORE_SCHEMA or lease.get("kind") != "active-reconstruction":
                        raise ArtifactStoreError("invalid promoted artifact lease")
                    consumer_value = lease.get("consumer")
                    digests = lease.get("oci_digests")
                    if not isinstance(consumer_value, str) or not isinstance(digests, list) or not digests or not all(isinstance(digest, str) and _oci_digest_hex(digest) for digest in digests):
                        raise ArtifactStoreError("invalid promoted artifact lease")
                    if not _valid_digest(lease.get("buildset")) or not _valid_timestamp(lease.get("updated_at")):
                        raise ArtifactStoreError("invalid promoted artifact lease")
                    consumer = Path(consumer_value)
                    if not consumer.is_absolute():
                        raise ArtifactStoreError("invalid promoted artifact lease")
                    if consumer.exists():
                        pinned.update(digests)
                    else:
                        path.unlink(missing_ok=True)
                except (OSError, ValueError, KeyError, TypeError, AttributeError, ArtifactStoreError):
                    if path.is_dir() and not path.is_symlink():
                        shutil.rmtree(path)
                    else:
                        path.unlink(missing_ok=True)
        return pinned

    @staticmethod
    def _entry_size(path: Path) -> int:
        if not path.is_dir() or path.is_symlink():
            try:
                return path.stat().st_size if path.exists() and not path.is_symlink() else 0
            except OSError:
                return 0
        total = 0
        for child in path.rglob("*"):
            if child.is_file() and not child.is_symlink():
                try:
                    total += child.stat().st_size
                except OSError:
                    pass
        return total

    def gc(
        self,
        lock_path: str | Path | None = None,
        *,
        now: float | None = None,
        max_bytes: int = MAX_BYTES,
        max_age_seconds: int = MAX_AGE_SECONDS,
    ) -> dict:
        clock = time.time() if now is None else now
        with self._locked():
            pinned = self._pinned_digests(lock_path)
            objects_root = self.root / "objects" / "oci" / "sha256"
            candidates: list[tuple[float, int, str, Path]] = []
            stale: list[tuple[int, Path]] = []
            retained = 0
            removed = 0
            bytes_removed = 0

            if objects_root.is_dir():
                for object_dir in objects_root.iterdir():
                    if object_dir.is_symlink() or not object_dir.is_dir():
                        continue
                    digest = "sha256:" + object_dir.name
                    size = self._entry_size(object_dir)
                    try:
                        record = json.loads(
                            (object_dir / "verification.json").read_text(encoding="utf-8")
                        )
                        last_used = float(record.get("last_used_at", object_dir.stat().st_mtime))
                    except (OSError, ValueError, TypeError, AttributeError):
                        last_used = object_dir.stat().st_mtime
                    if digest in pinned:
                        retained += size
                    elif clock - last_used > max_age_seconds:
                        stale.append((size, object_dir))
                    else:
                        candidates.append((last_used, size, digest, object_dir))

            for size, path in stale:
                if path.is_dir() and not path.is_symlink():
                    shutil.rmtree(path)
                else:
                    path.unlink(missing_ok=True)
                removed += 1
                bytes_removed += size

            retained += sum(item[1] for item in candidates)

            candidates.sort(key=lambda item: item[0])
            for last_used, size, digest, object_dir in candidates:
                del last_used, digest
                if retained > max_bytes:
                    shutil.rmtree(object_dir)
                    removed += 1
                    bytes_removed += size
                    retained -= size

            if retained > max_bytes:
                required = retained - max_bytes
                raise ArtifactStoreError(
                    f"pinned promoted artifacts exceed the combined storage budget by {required} bytes"
                )

            return {
                "schema": STORE_SCHEMA,
                "root": str(self.root),
                "removed_objects": removed,
                "bytes_removed": bytes_removed,
                "bytes_retained": retained,
                "pinned_objects": len(pinned),
                "prepared_bytes": 0,
                "combined_bytes": retained,
                "max_combined_bytes": max_bytes,
            }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", default=str(DEFAULT_STORE))
    parser.add_argument("--lock")
    args = parser.parse_args(argv)
    payload = ArtifactStore(args.root).gc(args.lock)
    print(json.dumps(payload, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
