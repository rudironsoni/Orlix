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


def acquisition_record(source: str, verification: dict | None) -> dict:
    """Record where a member came from.

    ``signature_verified`` is true only for a network fetch performed after
    ``verification_context()``. A local hit still verifies stored bytes, but it
    does not set the flag and does not authorize promotion or release.
    """
    if source not in {"local-store", "network"}:
        raise ReconstructError(f"unknown acquisition source: {source}")
    return {
        "source": source,
        "signature_verified": source == "network" and verification is not None,
    }


def _select_components(available: dict, component_names: list[str] | None) -> dict:
    """None keeps the full lock. An empty list acquires nothing."""
    if component_names is None:
        return available
    unknown = [name for name in component_names if name not in available]
    if unknown:
        raise ReconstructError(f"unknown promoted components: {', '.join(unknown)}")
    return {name: available[name] for name in component_names}


def _install_named(staged: Path, root: Path) -> None:
    """Place only the acquired members.

    The reconstruct root and every unrequested sibling tree stay in place.
    """
    if root.is_symlink():
        raise ReconstructError("reconstruction must not replace a symlinked buildset")
    if root.exists() and not root.is_dir():
        raise ReconstructError("reconstruction must not replace a non-directory buildset")
    moves: list[str] = []
    for child in sorted(staged.iterdir()):
        dest = root / child.name
        if dest.is_symlink() or (dest.exists() and not dest.is_dir()):
            raise ReconstructError(f"reconstruction must not replace {child.name}")
        if dest.is_dir():
            if tree_digest(dest) != tree_digest(child):
                raise ReconstructError(
                    f"existing reconstructed {child.name} differs from signed artifacts"
                )
        else:
            moves.append(child.name)
    if not moves:
        return
    root.mkdir(parents=True, exist_ok=True)
    for name in moves:
        (staged / name).rename(root / name)


def _acquisition_evidence(
    buildset: str,
    components: dict,
    acquisition: dict[str, dict],
    network_downloads: int,
    local_store_hits: int,
) -> dict:
    return {
        "schema": 1,
        "kind": "promoted-acquisition",
        "buildset": buildset,
        "components_total": len(components),
        "network_downloads": network_downloads,
        "local_store_hits": local_store_hits,
        "components": acquisition,
    }


def _write_evidence(evidence_path: str | None, evidence: dict) -> None:
    if not evidence_path:
        return
    destination = Path(evidence_path)
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(json.dumps(evidence, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def acquire_components(
    components: dict,
    staged: Path,
    root: Path,
    *,
    verification: dict | None,
    store: ArtifactStore,
    run,
    schema: int,
) -> dict:
    """Fetch only the promoted members in ``components``.

    This does not create, delete, or replace the reconstruct root. Callers
    install the staged trees. An empty member map is a legal no-op.
    """
    if components and verification is None:
        raise ReconstructError("signature verification requires verification_context()")
    pulled: dict[str, dict] = {}
    acquisition: dict[str, dict] = {}
    network_downloads = 0
    local_store_hits = 0
    downloaded = False
    for name, component in sorted(components.items()):
        reference = component.get("oci_reference")
        digest = component.get("oci_digest")
        unsigned = component.get("unsigned_digest")
        if not reference or not digest or not unsigned:
            raise ReconstructError(
                f"{name} lock entry missing oci_reference, oci_digest, or unsigned_digest"
            )
        dest = staged / name

        def _reverify(component=component, name=name):
            verify_component(
                {**component, "component": name, "signed": True, "schema": schema}, run=run
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
            acquisition[name] = acquisition_record("local-store", verification)
        else:
            if shutil.which("cosign") is None:
                raise ReconstructError("cosign is required to reconstruct")
            if shutil.which("oras") is None:
                raise ReconstructError("oras is required to reconstruct")
            try:
                verify_component(
                    {**component, "component": name, "signed": True, "schema": schema}, run=run
                )
            except (PublishError, LockedBuildsetError, OSError) as error:
                raise ReconstructError(str(error)) from error
            pull_dest = staged.parent / name
            pull_dest.mkdir()
            pull = ["oras", "pull", reference, "-o", str(pull_dest)]
            config = os.environ.get("ORLIX_ORAS_REGISTRY_CONFIG")
            if config:
                pull[2:2] = ["--registry-config", config]
            run(pull)
            blob = pull_dest / "component.tar"
            if not blob.is_file():
                raise ReconstructError(f"{name} pull did not write component.tar")
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
            downloaded = True
            network_downloads += 1
            acquisition[name] = acquisition_record("network", verification)
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
    return {
        "pulled": pulled,
        "acquisition": acquisition,
        "network_downloads": network_downloads,
        "local_store_hits": local_store_hits,
        "downloaded": downloaded,
    }


def reconstruct(
    lock_path: str,
    out_dir: str,
    run=_run,
    store_root: str | Path | None = None,
    evidence_path: str | None = None,
    component_names: list[str] | None = None,
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
    buildset = lock["buildset"]
    selected = _select_components(lock["components"], component_names)
    root = Path(out_dir) / buildset
    if not selected:
        evidence = _acquisition_evidence(buildset, selected, {}, 0, 0)
        _write_evidence(evidence_path, evidence)
        return {"buildset": buildset, "components": {}, "acquisition": evidence}
    try:
        verification = verification_context()
    except (OSError, PublishError, ValueError, KeyError) as error:
        raise ReconstructError(str(error)) from error
    store = ArtifactStore(store_root) if store_root is not None else store_from_environment()
    full_lock = component_names is None
    root.parent.mkdir(parents=True, exist_ok=True)
    if full_lock and root.is_symlink():
        raise ReconstructError("reconstruction must not replace a symlinked buildset")
    with tempfile.TemporaryDirectory(prefix="orlix-reconstruct-", dir=root.parent) as tmp:
        staged = Path(tmp) / "tree"
        staged.mkdir()
        fetched = acquire_components(
            selected,
            staged,
            root,
            verification=verification,
            store=store,
            run=run,
            schema=lock["schema"],
        )
        if fetched["downloaded"]:
            try:
                store.gc(lock_path)
            except (ArtifactStoreError, OSError) as error:
                raise ReconstructError(str(error)) from error
        if full_lock:
            if root.exists():
                if tree_digest(root) != tree_digest(staged):
                    raise ReconstructError(
                        "existing reconstructed contents differ from signed artifacts"
                    )
            else:
                staged.rename(root)
        else:
            _install_named(staged, root)
    try:
        store.write_lease(buildset, list(selected.values()), root)
    except (ArtifactStoreError, OSError) as error:
        raise ReconstructError(str(error)) from error
    evidence = _acquisition_evidence(
        buildset,
        selected,
        fetched["acquisition"],
        fetched["network_downloads"],
        fetched["local_store_hits"],
    )
    _write_evidence(evidence_path, evidence)
    return {"buildset": buildset, "components": fetched["pulled"], "acquisition": evidence}


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--lock", default="artifacts.lock.json")
    parser.add_argument("--out-dir", required=True)
    parser.add_argument("--store")
    parser.add_argument("--evidence")
    parser.add_argument(
        "--components",
        help="comma-separated lock component names to acquire; omit for the full lock",
    )
    args = parser.parse_args(argv)
    if args.components is None:
        names = None
    else:
        names = [part.strip() for part in args.components.split(",") if part.strip()]
    payload = reconstruct(
        args.lock,
        args.out_dir,
        store_root=args.store,
        evidence_path=args.evidence,
        component_names=names,
    )
    print(payload["buildset"])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
