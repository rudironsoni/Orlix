"""Declared-byte keys for guest package actions.

Bazel 9.2.0 can hide a source edit inside a directory input and return the
previous output from the output-base action cache. These helpers turn the
files a package actually compiles into a tar and a content stamp. Those two
files are ordinary action outputs, so a later action keys on their bytes.
"""

from __future__ import annotations

import hashlib
import io
import os
from pathlib import Path
import sys
import tarfile
from typing import Iterable


def _file_mode(path: Path) -> int:
    return 0o755 if path.stat().st_mode & 0o111 else 0o644


def _add_bytes(tar: tarfile.TarFile, name: str, data: bytes, mode: int) -> None:
    info = tarfile.TarInfo(name)
    info.size = len(data)
    info.mtime = 0
    info.mode = mode
    info.uid = 0
    info.gid = 0
    info.uname = ""
    info.gname = ""
    info.type = tarfile.REGTYPE
    tar.addfile(info, io.BytesIO(data))


def write_file_key(archive: Path, stamp: Path, pairs: Iterable[tuple[str, Path]]) -> None:
    """Write a deterministic tar and a sorted content stamp of declared files."""
    ordered = sorted((name, path) for name, path in pairs)
    lines: list[str] = []
    with tarfile.open(archive, "w") as tar:
        for name, path in ordered:
            data = path.read_bytes()
            lines.append(f"{hashlib.sha256(data).hexdigest()}  {name}\n")
            _add_bytes(tar, name, data, _file_mode(path))
    stamp.write_text("".join(lines))


def write_tree_tar(archive: Path, tree: Path) -> None:
    """Write a deterministic tar of one dependency tree, following file symlinks."""
    files: list[tuple[str, Path]] = []
    for dirpath, dirnames, filenames in os.walk(tree):
        dirnames.sort()
        for filename in sorted(filenames):
            path = Path(dirpath) / filename
            if path.is_symlink() and not path.is_file():
                continue
            if not path.is_file():
                continue
            relative = path.relative_to(tree).as_posix()
            files.append((relative, path))
    with tarfile.open(archive, "w") as tar:
        for name, path in files:
            _add_bytes(tar, name, path.read_bytes(), _file_mode(path))


def main(argv: list[str] | None = None) -> int:
    args = list(sys.argv[1:] if argv is None else argv)
    if not args or args[0] not in {"files", "tree"}:
        raise SystemExit("usage: source_key.py files|tree ...")
    if args[0] == "tree":
        if len(args) != 3:
            raise SystemExit("usage: source_key.py tree ARCHIVE TREE")
        write_tree_tar(Path(args[1]), Path(args[2]))
        return 0
    if len(args) < 3 or (len(args) - 3) % 2:
        raise SystemExit("usage: source_key.py files ARCHIVE STAMP NAME PATH...")
    names = args[3::2]
    paths = args[4::2]
    write_file_key(Path(args[1]), Path(args[2]), zip(names, (Path(path) for path in paths)))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
