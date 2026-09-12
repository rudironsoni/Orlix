"""Checked worktree-local foreign-build state and content-preserving materialization."""

from __future__ import annotations

from contextlib import contextmanager
import fcntl
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import stat
import tempfile
import time


def _files(root: Path) -> dict[str, Path]:
    return {
        path.relative_to(root).as_posix(): path
        for path in root.rglob("*")
        if path.is_symlink() or path.is_file()
    }


def _entry(source: Path | bytes) -> tuple[int, bytes]:
    if isinstance(source, bytes):
        return stat.S_IFREG | 0o644, source
    if source.is_symlink():
        return stat.S_IFLNK | 0o777, os.fsencode(source.readlink())
    mode = 0o755 if source.stat().st_mode & 0o111 else 0o644
    return stat.S_IFREG | mode, source.read_bytes()


def _remove(path: Path) -> None:
    if path.is_dir() and not path.is_symlink():
        shutil.rmtree(path)
    else:
        path.unlink(missing_ok=True)


def directory(path: Path, boundary: Path) -> None:
    current = boundary
    for part in ("", *path.relative_to(boundary).parts):
        current = current / part
        if current.is_symlink() or (current.exists() and not current.is_dir()):
            raise ValueError(f"state ancestor is not a real directory: {current}")
        current.mkdir(exist_ok=True)


def sync(files: dict[str, Path | bytes], destination: Path, boundary: Path, *, remove_stale: bool = True) -> dict:
    directory(destination, boundary)
    directories = {destination}
    changed = 0
    digest = hashlib.sha256()
    for name, source in sorted(files.items()):
        relative = PurePosixPath(name)
        if relative.is_absolute() or ".." in relative.parts or relative.as_posix() != name:
            raise ValueError(f"invalid source path: {name}")
        mode, data = _entry(source)
        digest.update(json.dumps([name, mode, hashlib.sha256(data).hexdigest()]).encode() + b"\n")
        target = destination / name
        for parent in reversed(target.parents):
            if destination in parent.parents and parent not in directories:
                if parent.is_symlink() or (parent.exists() and not parent.is_dir()):
                    _remove(parent)
                parent.mkdir(exist_ok=True)
                directories.add(parent)
        try:
            current = target.lstat().st_mode
        except FileNotFoundError:
            current = 0
        if stat.S_ISLNK(mode):
            if stat.S_ISLNK(current) and os.fsencode(target.readlink()) == data:
                continue
            _remove(target)
            target.symlink_to(os.fsdecode(data))
        else:
            if current == mode and target.read_bytes() == data:
                continue
            _remove(target)
            target.write_bytes(data)
            target.chmod(stat.S_IMODE(mode))
        changed += 1
    if remove_stale:
        for name, path in _files(destination).items():
            if name not in files:
                _remove(path)
                changed += 1
    return {"sha256": digest.hexdigest(), "files": len(files), "changed": changed}



def _build_content(root: Path, trees: tuple[str, ...]) -> dict:
    result = {}
    for name in trees:
        tree = root / name
        if tree.is_symlink():
            raise ValueError(f"build tree is a symlink: {tree}")
        for relative, path in _files(tree).items():
            mode, data = _entry(path)
            result[name + "/" + relative] = [mode, hashlib.sha256(data).hexdigest(), path.lstat().st_mtime_ns]
    return result


def resume(root: Path, identity_inputs: list[Path], trees: tuple[str, ...], *, compatibility: dict | None = None) -> bool:
    identity = hashlib.sha256()
    for path in identity_inputs:
        identity.update(path.name.encode() + b"\0" + hashlib.sha256(path.read_bytes()).digest())
    expected = identity.hexdigest()
    record = root / "build-state.json"
    try:
        if record.is_symlink():
            raise ValueError("build state record is a symlink")
        previous = json.loads(record.read_text())
        if compatibility is not None and isinstance(previous, dict):
            stored = previous.get("compatibility")
            owner = stored.get("package") if isinstance(stored, dict) else None
            if owner is not None and owner != compatibility.get("package"):
                raise RuntimeError(f"package state mismatch: {owner} != {compatibility.get('package')}")
        current = _build_content(root, trees)
        checked_at = time.time_ns()
        valid = isinstance(previous, dict) and previous["identity"] == expected and previous["files"] == current and all(entry[2] <= checked_at for entry in current.values())
        if compatibility is not None:
            valid = valid and previous.get("compatibility") == compatibility
    except (OSError, ValueError, KeyError, TypeError):
        valid = False
    if not valid:
        for name in trees:
            _remove(root / name)
    _remove(record)
    identity_file = root / "build-identity"
    _remove(identity_file)
    identity_file.write_text(expected)
    return valid


def record(root: Path, trees: tuple[str, ...], *, compatibility: dict | None = None) -> None:
    payload = {"identity": (root / "build-identity").read_text(), "files": _build_content(root, trees)}
    if compatibility is not None:
        payload["compatibility"] = compatibility
    with tempfile.NamedTemporaryFile(mode="w", dir=root, delete=False) as stream:
        stream.write(json.dumps(payload, sort_keys=True) + "\n")
    Path(stream.name).replace(root / "build-state.json")


@contextmanager
def locked(root: Path, boundary: Path):
    directory(root, boundary)
    descriptor = os.open(root / "build.lock", os.O_CREAT | os.O_RDWR | os.O_NOFOLLOW, 0o600)
    with os.fdopen(descriptor, "w") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        yield
