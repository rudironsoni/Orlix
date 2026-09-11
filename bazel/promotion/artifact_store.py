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


STORE_SCHEMA = 1
LEGACY_IDENTITY_FORMAT = "legacy-marker-sha256"
LEGACY_IDENTITY_VERSION = 1
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
        if not _valid_digest(record.get("unsigned_digest")) or record.get("unsigned_digest") != entry["unsigned_digest"]:
            raise ArtifactStoreError("promoted artifact legacy digest does not match the lock")
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
        identity = record.get("artifact_identity") or {}
        if not isinstance(identity, dict):
            raise ArtifactStoreError("promoted artifact identity must be a JSON object")
        marker = identity.get("marker")
        if identity.get("format") != LEGACY_IDENTITY_FORMAT or identity.get("version") != LEGACY_IDENTITY_VERSION or isinstance(identity.get("version"), bool):
            raise ArtifactStoreError("promoted artifact identity format is not the locked legacy marker format")
        if identity.get("digest") != entry["unsigned_digest"] or not _safe_relative(marker):
            raise ArtifactStoreError("promoted artifact legacy identity does not match the lock")
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
        if marker_text != entry["unsigned_digest"]:
            raise ArtifactStoreError("promoted artifact unsigned digest marker differs from the lock")
        blob_record = record.get("blob") or {}
        if not isinstance(blob_record, dict) or blob_record.get("path") != "component.tar" or not _valid_digest(blob_record.get("sha256")) or blob_record.get("sha256") != _sha256_file(blob):
            raise ArtifactStoreError("promoted artifact tar digest is invalid")
        if not isinstance(blob_record.get("size"), int) or isinstance(blob_record.get("size"), bool) or blob_record.get("size") < 0 or blob_record.get("size") != blob.stat().st_size:
            raise ArtifactStoreError("promoted artifact tar size is invalid")
        tree_record = record.get("tree") or {}
        if not isinstance(tree_record, dict) or tree_record.get("path") != "tree" or not _valid_digest(tree_record.get("sha256")) or tree_record.get("sha256") != tree_digest(tree):
            raise ArtifactStoreError("promoted artifact tree digest is invalid")
        verification = record.get("verification")
        if not isinstance(verification, dict) or not _valid_digest(verification.get("signing_key_fingerprint")) or not _valid_digest(verification.get("trust_policy_sha256")) or not isinstance(verification.get("verification_policy_version"), int) or isinstance(verification.get("verification_policy_version"), bool) or not _valid_timestamp(record.get("last_used_at")):
            raise ArtifactStoreError("promoted artifact verification metadata is invalid")
        return {
            "object": object_dir,
            "tree": tree,
            "marker": marker,
            "record": record,
            "verification": verification,
        }

    def lookup(self, component: str, entry: dict, verification: dict | None = None) -> dict | None:
        with self._locked():
            return self._lookup_unlocked(component, entry, verification)

    def _lookup_unlocked(
        self, component: str, entry: dict, verification: dict | None = None
    ) -> dict | None:
        try:
            found = self._validate(component, entry)
        except (ArtifactStoreError, OSError, TypeError, ValueError):
            return None
        found["needs_reverify"] = verification is not None and found["verification"] != verification
        if not found["needs_reverify"]:
            self._touch_unlocked(found)
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
            return {"marker": found["marker"]}

    def refresh_verification(self, component: str, entry: dict, verification: dict) -> dict:
        with self._locked():
            return self._refresh_unlocked(component, entry, verification)

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
        marker = _unsigned_marker(tree_path, entry["unsigned_digest"], component)
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
                tree_digest_value = tree_digest(staged_tree)
                blob_digest = _sha256_file(staged_object / "component.tar")
                record = {
                "schema": STORE_SCHEMA,
                "kind": "orlix-promoted-artifact",
                "component": component,
                "unsigned_digest": entry["unsigned_digest"],
                "oci_digest": entry["oci_digest"],
                "oci_reference": entry["oci_reference"],
                "artifact_identity": self._expected_identity(entry, marker),
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
                    if destination.is_symlink() or destination.is_file():
                        destination.unlink()
                    else:
                        shutil.rmtree(destination)
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
        *,
        now: float,
    ) -> set[str]:
        pinned: set[str] = set()
        if lock_path:
            try:
                payload = json.loads(Path(lock_path).read_text(encoding="utf-8"))
                for entry in (payload.get("components") or {}).values():
                    digest = entry.get("oci_digest")
                    if isinstance(digest, str):
                        pinned.add(digest)
            except (OSError, ValueError) as error:
                raise ArtifactStoreError(f"cannot read promoted lock for garbage collection: {lock_path}") from error
        leases = self.root / "leases"
        if leases.is_dir():
            for path in leases.glob("*.json"):
                try:
                    lease = json.loads(path.read_text(encoding="utf-8"))
                    if not isinstance(lease, dict) or lease.get("schema") != STORE_SCHEMA or lease.get("kind") != "active-reconstruction":
                        continue
                    consumer_value = lease.get("consumer")
                    digests = lease.get("oci_digests")
                    if not isinstance(consumer_value, str) or not isinstance(digests, list) or not digests or not all(isinstance(digest, str) and _oci_digest_hex(digest) for digest in digests):
                        continue
                    if not _valid_digest(lease.get("buildset")) or not _valid_timestamp(lease.get("updated_at")):
                        continue
                    if Path(consumer_value).exists():
                        pinned.update(digests)
                except (OSError, ValueError, KeyError, TypeError, ArtifactStoreError):
                    continue
        return pinned

    @staticmethod
    def _entry_size(path: Path) -> int:
        total = 0
        for child in path.rglob("*"):
            if child.is_file() and not child.is_symlink():
                total += child.stat().st_size
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
            pinned = self._pinned_digests(lock_path, now=clock)
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
            if retained > max_bytes:
                raise ArtifactStoreError(
                    "pinned promoted artifacts exceed the combined storage budget"
                )
            for size, object_dir in stale:
                shutil.rmtree(object_dir)
                removed += 1
                bytes_removed += size
            candidates.sort(key=lambda item: item[0])
            retained += sum(item[1] for item in candidates)
            for last_used, size, digest, object_dir in candidates:
                del last_used, digest
                if retained > max_bytes:
                    shutil.rmtree(object_dir)
                    removed += 1
                    bytes_removed += size
                    retained -= size
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
