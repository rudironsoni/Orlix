#!/usr/bin/env python3
"""Digest legacy trees and canonical selected artifacts."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import stat
from collections.abc import Iterable, Mapping
from pathlib import Path
from typing import Optional, Tuple, Union


ARTIFACT_IDENTITY_V2_DOMAIN = "orlix.artifact.identity"
ARTIFACT_IDENTITY_V2_VERSION = 2
ARTIFACT_IDENTITY_V2_FORMAT = "artifact-identity-v2"

ArtifactInput = Union[str, Path]
ArtifactSelection = Union[
    Mapping[str, ArtifactInput],
    Iterable[Tuple[str, ArtifactInput]],
]


def tree_digest(root: Path) -> str:
    if not root.is_dir():
        raise ValueError(f"missing component tree: {root}")
    digest = hashlib.sha256()
    for path in sorted(root.rglob("*")):
        mode = path.lstat().st_mode
        if stat.S_ISLNK(mode):
            content = str(path.readlink())
        elif stat.S_ISREG(mode):
            file_digest = hashlib.sha256()
            with path.open("rb") as stream:
                for chunk in iter(lambda: stream.read(1024 * 1024), b""):
                    file_digest.update(chunk)
            content = file_digest.hexdigest()
        elif stat.S_ISDIR(mode):
            content = ""
        else:
            raise ValueError(f"unsupported component entry: {path}")
        record = [path.relative_to(root).as_posix(), mode, content]
        digest.update(json.dumps(record, separators=(",", ":")).encode() + b"\n")
    return digest.hexdigest()


def _file_digest(path: Path) -> str:
    flags = os.O_RDONLY | getattr(os, "O_NOFOLLOW", 0)
    descriptor = os.open(path, flags)
    with os.fdopen(descriptor, "rb") as stream:
        digest = hashlib.sha256()
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
        return digest.hexdigest()


def _entry(path: Path, relative: str, *, allow_directory: bool) -> dict:
    metadata = path.lstat()
    mode = metadata.st_mode
    entry = {
        "mode": stat.S_IMODE(mode),
        "path": relative,
    }
    if stat.S_ISREG(mode):
        entry["content_sha256"] = _file_digest(path)
        entry["type"] = "file"
        return entry
    if stat.S_ISLNK(mode):
        target = os.readlink(path)
        entry["target"] = target
        entry["type"] = "symlink"
        return entry
    if stat.S_ISDIR(mode) and allow_directory:
        entry["type"] = "directory"
        return entry
    raise ValueError(f"unsupported artifact entry: {path}")


def _tree_entries(root: Path) -> list[dict]:
    try:
        root_metadata = root.lstat()
    except OSError as error:
        raise ValueError(f"missing artifact root: {root}") from error
    if not stat.S_ISDIR(root_metadata.st_mode):
        raise ValueError(f"artifact root is not a real directory: {root}")

    entries = []
    pending = [root]
    while pending:
        current = pending.pop()
        children = sorted(
            current.iterdir(),
            key=lambda child: child.relative_to(root).as_posix(),
            reverse=True,
        )
        for child in children:
            relative = child.relative_to(root).as_posix()
            entry = _entry(child, relative, allow_directory=True)
            entries.append(entry)
            if entry["type"] == "directory":
                pending.append(child)
    return sorted(entries, key=lambda entry: entry["path"])


def _relative_artifact_path(name: str) -> str:
    if not isinstance(name, str) or not name:
        raise ValueError(f"invalid artifact path: {name!r}")
    if name.startswith("/") or "\x00" in name:
        raise ValueError(f"invalid artifact path: {name!r}")
    parts = name.split("/")
    if any(part in ("", ".", "..") for part in parts):
        raise ValueError(f"invalid artifact path: {name!r}")
    return name


def _selected_entries(artifacts: ArtifactSelection) -> list[dict]:
    items = artifacts.items() if isinstance(artifacts, Mapping) else artifacts
    entries = []
    names = set()
    try:
        iterator = iter(items)
    except TypeError as error:
        raise ValueError("named artifact inputs are required") from error
    for item in iterator:
        try:
            name, source = item
        except (TypeError, ValueError) as error:
            raise ValueError("named artifact inputs require (path, source) pairs") from error
        relative = _relative_artifact_path(name)
        if relative in names:
            raise ValueError(f"duplicate artifact path: {relative}")
        names.add(relative)
        entries.append(_entry(Path(source), relative, allow_directory=False))
    return sorted(entries, key=lambda entry: entry["path"])


def artifact_manifest_v2(
    root: Optional[Path] = None,
    *,
    artifacts: Optional[ArtifactSelection] = None,
    format: str = ARTIFACT_IDENTITY_V2_FORMAT,
) -> bytes:
    """Serialize one explicitly selected product boundary canonically."""
    if format != ARTIFACT_IDENTITY_V2_FORMAT:
        raise ValueError(f"unknown artifact identity format: {format!r}")
    if (root is None) == (artifacts is None):
        raise ValueError("provide exactly one of root or artifacts")
    entries = _tree_entries(Path(root)) if root is not None else _selected_entries(artifacts)
    payload = {
        "domain": ARTIFACT_IDENTITY_V2_DOMAIN,
        "entries": entries,
        "format": format,
        "version": ARTIFACT_IDENTITY_V2_VERSION,
    }
    return (
        json.dumps(payload, ensure_ascii=True, sort_keys=True, separators=(",", ":"))
        + "\n"
    ).encode("ascii")


def artifact_identity_v2(
    root: Optional[Path] = None,
    *,
    artifacts: Optional[ArtifactSelection] = None,
    format: str = ARTIFACT_IDENTITY_V2_FORMAT,
) -> str:
    """Return the SHA-256 of the canonical artifact-identity-v2 manifest."""
    return hashlib.sha256(
        artifact_manifest_v2(root, artifacts=artifacts, format=format)
    ).hexdigest()


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("root", type=Path)
    args = parser.parse_args(argv)
    print(tree_digest(args.root))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
