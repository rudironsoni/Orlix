#!/usr/bin/env python3
"""Reconstruct locked components from the local store before network acquisition."""

from __future__ import annotations

import argparse
import copy
import json
import os
import posixpath
import shutil
import subprocess
import tarfile
import tempfile
from pathlib import Path
from pathlib import PurePosixPath

from artifact_store import ArtifactStore, ArtifactStoreError, store_from_environment
from publish import PublishError, verify_component
from publish import verification_context
from locked_buildset import LockedBuildsetError, load_locked_buildset
from compare import tree_digest


class ReconstructError(ValueError):
    pass


def _run(argv: list[str], env: dict[str, str] | None = None) -> subprocess.CompletedProcess[str]:
    try:
        return subprocess.run(
            argv,
            check=True,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            env=env,
        )
    except subprocess.CalledProcessError as error:
        raise ReconstructError(f"{argv[0]} failed: {error.stdout or ''}") from error


def _extract_component_tar(blob: Path, dest: Path) -> None:
    try:
        dest.mkdir(parents=True, exist_ok=True)
        root = dest.resolve()
        with tarfile.open(blob, "r") as archive:
            directories = []
            for member in archive.getmembers():
                path = PurePosixPath(member.name)
                if path.is_absolute() or ".." in path.parts or member.isdev() or member.isfifo():
                    raise ReconstructError(f"unsafe component tar entry: {member.name}")
                extracted = (dest / member.name).resolve()
                if os.path.commonpath((str(root), str(extracted))) != str(root):
                    raise ReconstructError(f"unsafe component tar entry: {member.name}")
                if member.issym():
                    target = (dest / path.parent / member.linkname).resolve()
                    if os.path.commonpath((str(root), str(target))) != str(root):
                        raise ReconstructError(f"unsafe component tar link: {member.name}")
                if member.islnk():
                    target = (dest / posixpath.normpath(member.linkname)).resolve()
                    if os.path.commonpath((str(root), str(target))) != str(root):
                        raise ReconstructError(f"unsafe component tar link: {member.name}")
                extracted_member = member
                if member.isdir():
                    directories.append(member)
                    extracted_member = copy.copy(member)
                    extracted_member.mode = 0o700
                archive.extract(
                    extracted_member,
                    dest,
                    set_attrs=not member.isdir(),
                )
                if member.isfile():
                    archive.chmod(member, dest / member.name)
            for member in sorted(directories, key=lambda item: item.name, reverse=True):
                directory = dest / member.name
                archive.chown(member, directory, numeric_owner=False)
                archive.utime(member, directory)
                archive.chmod(member, directory)
    except (OSError, tarfile.TarError) as error:
        raise ReconstructError(
            f"{blob.name} is not a component tar; refusing to substitute a raw digest blob"
        ) from error


def reconstruct(
    lock_path: str,
    out_dir: str,
    run=_run,
    store_root: str | Path | None = None,
    evidence_path: str | None = None,
    component_names: list[str] | None = None,
    local_only: bool = False,
) -> dict:
    lock = json.loads(Path(lock_path).read_text(encoding="utf-8"))
    buildset = lock.get("buildset")
    components = lock.get("components") or {}
    if not buildset or not components:
        raise ReconstructError("empty lock cannot reconstruct a buildset")
    if not out_dir:
        raise ReconstructError("reconstruct out_dir is required")
    try:
        required = tuple(components) if lock.get("schema", 1) == 1 else None
        lock = load_locked_buildset(lock_path, required=required)
    except (KeyError, ValueError) as error:
        raise ReconstructError(str(error)) from error
    available = lock["components"]
    if component_names:
        unknown = [name for name in component_names if name not in available]
        if unknown:
            raise ReconstructError(f"unknown promoted components: {', '.join(unknown)}")
        components = {name: available[name] for name in component_names}
    else:
        components = available
    verification = None
    if not local_only:
        try:
            verification = verification_context()
        except (OSError, PublishError, ValueError, KeyError) as error:
            raise ReconstructError(str(error)) from error
    store = ArtifactStore(store_root) if store_root is not None else store_from_environment()
    buildset = lock["buildset"]
    acquired = False
    pulled = {}
    acquisition: dict[str, dict] = {}
    network_downloads = 0
    local_store_hits = 0
    root = Path(out_dir) / buildset
    root.parent.mkdir(parents=True, exist_ok=True)
    if root.is_symlink():
        raise ReconstructError("reconstruction must not replace a symlinked buildset")
    with tempfile.TemporaryDirectory(prefix="orlix-reconstruct-", dir=root.parent) as tmp:
        staged = Path(tmp) / "tree"
        staged.mkdir()
        for name, component in sorted(components.items()):
            reference = component.get("oci_reference")
            digest = component.get("oci_digest")
            unsigned = component.get("unsigned_digest")
            if not reference or not digest or not unsigned:
                raise ReconstructError(
                    f"{name} lock entry missing oci_reference, oci_digest, or unsigned_digest"
                )
            dest = staged / name
            def _reverify():
                verify_component(
                    {**component, "component": name, "signed": True, "schema": lock["schema"]}, run=run
                )

            try:
                materialized = store.materialize_local(
                    name,
                    component,
                    verification,
                    dest,
                    reverify=_reverify,
                )
            except (ArtifactStoreError, PublishError, OSError) as error:
                raise ReconstructError(str(error)) from error

            if materialized is not None:
                marker = materialized["marker"]
                identity = materialized.get("artifact_identity")
                local_store_hits += 1
                acquisition[name] = {"source": "local-store", "signature_verified": True}
            else:
                if local_only:
                    raise ReconstructError(f"{name} is not in the local artifact store")
                if shutil.which("cosign") is None:
                    raise ReconstructError("cosign is required to reconstruct")
                if shutil.which("oras") is None:
                    raise ReconstructError("oras is required to reconstruct")
                try:
                    verify_component(
                        {**component, "component": name, "signed": True, "schema": lock["schema"]}, run=run
                    )
                except (PublishError, LockedBuildsetError, OSError) as error:
                    raise ReconstructError(str(error)) from error
                pull_dest = Path(tmp) / name
                pull_dest.mkdir()
                pull = ["oras", "pull", reference, "-o", str(pull_dest)]
                config = os.environ.get("ORLIX_ORAS_REGISTRY_CONFIG")
                if config:
                    pull[2:2] = ["--registry-config", config]
                run(pull)
                blob = pull_dest / "component.tar"
                if not blob.is_file():
                    raise ReconstructError(f"{name} pull did not write component.tar")
                dest = staged / name
                dest.mkdir(parents=True, exist_ok=True)
                _extract_component_tar(blob, dest)
                try:
                    stored = store.publish_download(
                        name, component, blob, dest, verification
                    )
                except (ArtifactStoreError, OSError) as error:
                    raise ReconstructError(str(error)) from error
                marker = stored["marker"]
                identity = stored.get("artifact_identity")
                acquired = True
                network_downloads += 1
                acquisition[name] = {"source": "network", "signature_verified": True}
            pulled[name] = {
                "oci_digest": digest,
                "oci_reference": reference,
                "unsigned_digest": unsigned,
                "tree": str(root / name),
            }
            if identity is not None:
                pulled[name]["artifact_identity"] = identity
            acquisition[name]["oci_digest"] = digest
            if isinstance(identity, dict):
                acquisition[name]["artifact_identity"] = identity
            if marker is not None:
                pulled[name]["unsigned_digest_path"] = str(root / name / marker)
        if acquired:
            try:
                store.gc(lock_path)
            except (ArtifactStoreError, OSError) as error:
                raise ReconstructError(str(error)) from error
        if root.exists():
            if component_names:
                shutil.rmtree(root)
                staged.rename(root)
            elif tree_digest(root) != tree_digest(staged):
                raise ReconstructError("existing reconstructed contents differ from signed artifacts")
        else:
            staged.rename(root)
    try:
        store.write_lease(buildset, list(components.values()), root)
    except (ArtifactStoreError, OSError) as error:
        raise ReconstructError(str(error)) from error
    evidence = {
        "schema": 1,
        "kind": "promoted-acquisition",
        "buildset": buildset,
        "components_total": len(components),
        "network_downloads": network_downloads,
        "local_store_hits": local_store_hits,
        "components": acquisition,
    }
    if evidence_path:
        destination = Path(evidence_path)
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_text(json.dumps(evidence, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return {"buildset": buildset, "components": pulled, "acquisition": evidence}


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--lock", default="artifacts.lock.json")
    parser.add_argument("--out-dir", required=True)
    parser.add_argument("--store")
    parser.add_argument("--evidence")
    parser.add_argument(
        "--components",
        help="comma-separated lock component names to acquire; default is the full lock",
    )
    parser.add_argument(
        "--local-only",
        action="store_true",
        help="copy digest-addressed store hits; do not download or require a signing key",
    )
    args = parser.parse_args(argv)
    names = [part.strip() for part in args.components.split(",") if part.strip()] if args.components else None
    payload = reconstruct(
        args.lock,
        args.out_dir,
        store_root=args.store,
        evidence_path=args.evidence,
        component_names=names,
        local_only=args.local_only,
    )
    acquisition = payload["acquisition"]
    print(
        "promoted-acquisition network_downloads=%s local_store_hits=%s"
        % (acquisition["network_downloads"], acquisition["local_store_hits"])
    )
    print(payload["buildset"])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
