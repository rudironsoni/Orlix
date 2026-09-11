#!/usr/bin/env python3
"""Reconstruct locked components from the local store before network acquisition."""

from __future__ import annotations

import argparse
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
        with tarfile.open(blob, "r") as archive:
            for member in archive.getmembers():
                path = PurePosixPath(member.name)
                if path.is_absolute() or ".." in path.parts or member.isdev() or member.isfifo():
                    raise ReconstructError(f"unsafe component tar entry: {member.name}")
                if member.issym():
                    target = posixpath.normpath((path.parent / member.linkname).as_posix())
                    if member.linkname.startswith("/") or target == ".." or target.startswith("../"):
                        raise ReconstructError(f"unsafe component tar link: {member.name}")
                if member.islnk():
                    target = posixpath.normpath(member.linkname)
                    if member.linkname.startswith("/") or target == ".." or target.startswith("../"):
                        raise ReconstructError(f"unsafe component tar link: {member.name}")
            archive.extractall(dest)
    except (OSError, tarfile.TarError) as error:
        raise ReconstructError(
            f"{blob.name} is not a component tar; refusing to substitute a raw digest blob"
        ) from error


def reconstruct(
    lock_path: str,
    out_dir: str,
    run=_run,
    store_root: str | Path | None = None,
) -> dict:
    lock = json.loads(Path(lock_path).read_text(encoding="utf-8"))
    buildset = lock.get("buildset")
    components = lock.get("components") or {}
    if not buildset or not components:
        raise ReconstructError("empty lock cannot reconstruct a buildset")
    if not out_dir:
        raise ReconstructError("reconstruct out_dir is required")
    try:
        lock = load_locked_buildset(lock_path, required=tuple(components))
    except (KeyError, ValueError) as error:
        raise ReconstructError(str(error)) from error
    try:
        verification = verification_context()
    except (OSError, PublishError, ValueError, KeyError) as error:
        raise ReconstructError(str(error)) from error
    store = ArtifactStore(store_root) if store_root is not None else store_from_environment()
    buildset = lock["buildset"]
    components = lock["components"]
    pulled = {}
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
            local = store.lookup(name, component, verification)
            if local is not None:
                if local["needs_reverify"]:
                    try:
                        verify_component(
                            {**component, "component": name, "signed": True}, run=run
                        )
                        local = store.refresh_verification(name, component, verification)
                    except (PublishError, ArtifactStoreError, OSError) as error:
                        raise ReconstructError(str(error)) from error
                dest = staged / name
                shutil.copytree(local["tree"], dest, symlinks=True)
                marker = local["marker"]
            else:
                if shutil.which("cosign") is None:
                    raise ReconstructError("cosign is required to reconstruct")
                if shutil.which("oras") is None:
                    raise ReconstructError("oras is required to reconstruct")
                try:
                    verify_component(
                        {**component, "component": name, "signed": True}, run=run
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
            pulled[name] = {
                "oci_digest": digest,
                "oci_reference": reference,
                "unsigned_digest": unsigned,
                "tree": str(root / name),
                "unsigned_digest_path": str(root / name / marker),
            }
        if root.exists():
            if tree_digest(root) != tree_digest(staged):
                raise ReconstructError("existing reconstructed contents differ from signed artifacts")
        else:
            staged.rename(root)
    try:
        store.write_lease(buildset, list(components.values()), root)
    except (ArtifactStoreError, OSError) as error:
        raise ReconstructError(str(error)) from error
    return {"buildset": buildset, "components": pulled}


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--lock", default="artifacts.lock.json")
    parser.add_argument("--out-dir", required=True)
    parser.add_argument("--store")
    args = parser.parse_args(argv)
    payload = reconstruct(args.lock, args.out_dir, store_root=args.store)
    print(payload["buildset"])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
