#!/usr/bin/env python3
"""Pull and Cosign-verify every locked component. Fail closed if the lock is unsigned."""

from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import tarfile
import tempfile
from pathlib import Path

from publish import PublishError, verify_component
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
            archive.extractall(dest, filter="data")
    except tarfile.TarError as error:
        raise ReconstructError(
            f"{blob.name} is not a component tar; refusing to substitute a raw digest blob"
        ) from error


def _require_unsigned_digest(dest: Path, expected: str, name: str) -> str:
    matches: list[Path] = []
    for path in dest.rglob("*.sha256"):
        text = path.read_text(encoding="utf-8").strip()
        if text == expected:
            matches.append(path)
    if not matches:
        raise ReconstructError(
            f"{name} reconstructed tree is missing unsigned digest {expected}"
        )
    return str(matches[0])


def reconstruct(lock_path: str, out_dir: str, run=_run) -> dict:
    lock = json.loads(Path(lock_path).read_text(encoding="utf-8"))
    buildset = lock.get("buildset")
    components = lock.get("components") or {}
    if not buildset or not components:
        raise ReconstructError("empty lock cannot reconstruct a buildset")
    if not out_dir:
        raise ReconstructError("reconstruct out_dir is required")
    if shutil.which("oras") is None or shutil.which("cosign") is None:
        raise ReconstructError("oras and cosign are required to reconstruct")
    try:
        lock = load_locked_buildset(lock_path, required=tuple(components))
    except (KeyError, ValueError) as error:
        raise ReconstructError(str(error)) from error
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
            try:
                verify_component({**component, "component": name, "signed": True}, run=run)
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
            digest_path = _require_unsigned_digest(dest, unsigned, name)
            pulled[name] = {
                "oci_digest": digest,
                "oci_reference": reference,
                "unsigned_digest": unsigned,
                "tree": str(root / name),
                "unsigned_digest_path": str(root / Path(digest_path).relative_to(staged)),
            }
        if root.exists():
            if tree_digest(root) != tree_digest(staged):
                raise ReconstructError("existing reconstructed contents differ from signed artifacts")
        else:
            staged.rename(root)
    return {"buildset": buildset, "components": pulled}


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--lock", default="artifacts.lock.json")
    parser.add_argument("--out-dir", required=True)
    args = parser.parse_args(argv)
    payload = reconstruct(args.lock, args.out_dir)
    print(payload["buildset"])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
