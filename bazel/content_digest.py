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


def _tree_entries(root: Path, *, files_only: bool = False) -> list[dict]:
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
    if files_only:
        entries = [entry for entry in entries if entry["type"] != "directory"]
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


def _selected_source(path: Path) -> Path:
    """Hash the product file when Bazel stages it through an absolute symlink."""
    try:
        if not stat.S_ISLNK(path.lstat().st_mode):
            return path
    except OSError:
        return path
    target = os.readlink(path)
    if not target.startswith("/"):
        return path
    resolved = Path(target)
    try:
        if stat.S_ISREG(resolved.lstat().st_mode):
            return resolved
    except OSError:
        return path
    return path


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
        entries.append(_entry(_selected_source(Path(source)), relative, allow_directory=False))
    return sorted(entries, key=lambda entry: entry["path"])


def artifact_manifest_v2(
    root: Optional[Path] = None,
    *,
    artifacts: Optional[ArtifactSelection] = None,
    format: str = ARTIFACT_IDENTITY_V2_FORMAT,
    files_only: bool = False,
) -> bytes:
    """Serialize one explicitly selected product boundary canonically."""
    if format != ARTIFACT_IDENTITY_V2_FORMAT:
        raise ValueError(f"unknown artifact identity format: {format!r}")
    if (root is None) == (artifacts is None):
        raise ValueError("provide exactly one of root or artifacts")
    if root is not None:
        entries = _tree_entries(Path(root), files_only=files_only)
    else:
        entries = _selected_entries(artifacts)
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


def _owner_write_only(recorded: int, observed: int) -> bool:
    if type(recorded) is not int or type(observed) is not int:
        return False
    return observed == (recorded | 0o200) and (recorded & 0o200) == 0


def assert_staged_product_match(imported: bytes, computed: bytes) -> None:
    """Accept a staged manifest whose only mode change is Bazel's owner-write bit."""
    try:
        left = json.loads(imported)
        right = json.loads(computed)
    except json.JSONDecodeError as error:
        raise ValueError("artifact identity manifest is not JSON") from error
    if not isinstance(left, dict) or not isinstance(right, dict):
        raise ValueError("artifact identity manifest is not an object")
    for key in ("domain", "format", "version"):
        if left.get(key) != right.get(key):
            raise ValueError(f"artifact identity {key} differs")
    left_entries = left.get("entries")
    right_entries = right.get("entries")
    if not isinstance(left_entries, list) or not isinstance(right_entries, list):
        raise ValueError("artifact identity entries are missing")
    if len(left_entries) != len(right_entries):
        raise ValueError("artifact identity entry count differs")
    for recorded, observed in zip(left_entries, right_entries):
        if not isinstance(recorded, dict) or not isinstance(observed, dict):
            raise ValueError("artifact identity entry is not an object")
        if recorded.get("path") != observed.get("path") or recorded.get("type") != observed.get("type"):
            raise ValueError("artifact identity path or type differs")
        if recorded.get("content_sha256") != observed.get("content_sha256"):
            raise ValueError("artifact identity content differs")
        if recorded.get("target") != observed.get("target"):
            raise ValueError("artifact identity symlink target differs")
        recorded_mode = recorded.get("mode")
        observed_mode = observed.get("mode")
        if recorded_mode != observed_mode and not _owner_write_only(recorded_mode, observed_mode):
            raise ValueError(
                f"artifact identity mode differs for {recorded.get('path')}: {recorded_mode} != {observed_mode}"
            )


def restore_recorded_modes(imported: bytes, root: Path) -> None:
    payload = json.loads(imported)
    for entry in payload["entries"]:
        if entry.get("type") != "file":
            continue
        os.chmod(root / entry["path"], entry["mode"])


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


def consumer_tree_identity(root: Path) -> str:
    """Identity of the files a compiler reads. Directory modes stay out."""
    return hashlib.sha256(artifact_manifest_v2(root, files_only=True)).hexdigest()


def consumer_file_identity(path: Path) -> str:
    """Identity of one linked file. The absolute path stays out."""
    return artifact_identity_v2(artifacts={"file": path})


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--consumer", action="store_true")
    parser.add_argument("--consumer-file", action="store_true")
    parser.add_argument("root", type=Path)
    args = parser.parse_args(argv)
    if args.consumer and args.consumer_file:
        raise SystemExit("choose one of --consumer or --consumer-file")
    if args.consumer_file:
        print(consumer_file_identity(args.root))
    elif args.consumer:
        print(consumer_tree_identity(args.root))
    else:
        print(tree_digest(args.root))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
