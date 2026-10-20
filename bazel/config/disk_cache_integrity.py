"""Drop Bazel disk-cache blobs whose bytes do not match their names.

Bazel 9.2 stores a CAS object at ``cas/<hash[:2]>/<hash>`` and copies that
file on a hit without hashing it. A blob that is missing is already a miss.
A blob that is present with different bytes would otherwise become the action
output, so this check deletes it and the action runs again. A symlink used as
``cas``, as a digest directory, or as a nested ``cas`` is removed and not
followed. The cache root itself is never a symlink.

Within one ``ORLIX_BAZEL_RUN_ID``, a blob whose size and mtime are unchanged
since that run's hash is not read again. A new run hashes every blob.
"""

from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path


class DiskCacheIntegrityError(RuntimeError):
    pass


_HEX = frozenset("0123456789abcdef")
_STAMP = ".orlix-disk-cache-integrity.json"


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    try:
        with path.open("rb") as stream:
            for chunk in iter(lambda: stream.read(1024 * 1024), b""):
                digest.update(chunk)
    except OSError as error:
        raise DiskCacheIntegrityError(f"cannot read disk cache object {path}") from error
    return digest.hexdigest()


def _remove(path: Path) -> None:
    try:
        path.unlink()
    except OSError as error:
        raise DiskCacheIntegrityError(f"cannot remove disk cache object {path}") from error


def _cas_roots(root: Path) -> tuple[list[Path], int]:
    """Real CAS directories. Symlinks in those positions are removed."""
    found: list[Path] = []
    removed = 0
    direct = root / "cas"
    if direct.is_symlink():
        _remove(direct)
        removed += 1
    elif direct.is_dir():
        found.append(direct)
    try:
        children = list(root.iterdir())
    except OSError as error:
        raise DiskCacheIntegrityError(f"cannot read disk cache {root}") from error
    for child in children:
        if child.name in {"ac", "cas", "tmp"} or child.name.startswith("."):
            continue
        if child.is_symlink():
            _remove(child)
            removed += 1
            continue
        if not child.is_dir():
            continue
        nested = child / "cas"
        if nested.is_symlink():
            _remove(nested)
            removed += 1
            continue
        if nested.is_dir():
            found.append(nested)
    return found, removed


def _load_trusted(root: Path, run_id: str) -> dict[str, tuple[int, int]]:
    if not run_id:
        return {}
    path = root / _STAMP
    if path.is_symlink() or not path.is_file():
        return {}
    try:
        payload = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return {}
    if not isinstance(payload, dict) or payload.get("schema") != 1 or payload.get("run_id") != run_id:
        return {}
    files = payload.get("files")
    if not isinstance(files, dict):
        return {}
    trusted: dict[str, tuple[int, int]] = {}
    for name, item in files.items():
        if (
            isinstance(name, str)
            and isinstance(item, list)
            and len(item) == 2
            and isinstance(item[0], int)
            and not isinstance(item[0], bool)
            and isinstance(item[1], int)
            and not isinstance(item[1], bool)
        ):
            trusted[name] = (item[0], item[1])
    return trusted


def _write_stamp(root: Path, run_id: str, files: dict[str, tuple[int, int]]) -> None:
    if not run_id:
        return
    path = root / _STAMP
    if path.is_symlink():
        _remove(path)
    temporary = path.with_name(f".{path.name}.{os.getpid()}.tmp")
    payload = {
        "schema": 1,
        "run_id": run_id,
        "files": {name: [mtime, size] for name, (mtime, size) in sorted(files.items())},
    }
    try:
        temporary.write_text(json.dumps(payload, separators=(",", ":")) + "\n", encoding="utf-8")
        os.replace(temporary, path)
    except OSError:
        temporary.unlink(missing_ok=True)


def _discard_tree(
    root: Path,
    cas: Path,
    trusted: dict[str, tuple[int, int]],
    survivors: dict[str, tuple[int, int]],
) -> tuple[int, int]:
    checked = 0
    removed = 0
    for dirpath, dirnames, filenames in os.walk(cas, followlinks=False):
        current = Path(dirpath)
        kept: list[str] = []
        for name in dirnames:
            child = current / name
            if child.is_symlink():
                _remove(child)
                removed += 1
            else:
                kept.append(name)
        dirnames[:] = kept
        for name in filenames:
            path = current / name
            if path.is_symlink() or not path.is_file():
                _remove(path)
                removed += 1
                continue
            plausible = (
                len(name) == 64
                and path.parent.name == name[:2]
                and all(char in _HEX for char in name)
            )
            if not plausible:
                _remove(path)
                removed += 1
                continue
            checked += 1
            try:
                stat = path.stat()
            except OSError as error:
                raise DiskCacheIntegrityError(f"cannot stat disk cache object {path}") from error
            key = path.relative_to(root).as_posix()
            identity = (stat.st_mtime_ns, stat.st_size)
            if trusted.get(key) == identity:
                survivors[key] = identity
                continue
            if _sha256(path) != name:
                _remove(path)
                removed += 1
                continue
            try:
                stat = path.stat()
            except OSError as error:
                raise DiskCacheIntegrityError(f"cannot stat disk cache object {path}") from error
            survivors[key] = (stat.st_mtime_ns, stat.st_size)
    return checked, removed


def discard_corrupt_entries(root: Path) -> dict[str, int | bool]:
    """Delete corrupt CAS objects. A missing root stays missing."""
    if root.is_symlink():
        raise DiskCacheIntegrityError(f"disk cache root is a symlink: {root}")
    if not root.exists():
        return {"checked": 0, "removed": 0, "missing": True}
    if not root.is_dir():
        raise DiskCacheIntegrityError(f"disk cache root is not a directory: {root}")
    run_id = os.environ.get("ORLIX_BAZEL_RUN_ID", "")
    trusted = _load_trusted(root, run_id)
    survivors: dict[str, tuple[int, int]] = {}
    cas_roots, removed = _cas_roots(root)
    checked = 0
    for cas in cas_roots:
        tree_checked, tree_removed = _discard_tree(root, cas, trusted, survivors)
        checked += tree_checked
        removed += tree_removed
    _write_stamp(root, run_id, survivors)
    return {"checked": checked, "removed": removed, "missing": False}
