#!/usr/bin/env python3
"""Digest a filesystem tree by semantic path, mode, and content."""

from __future__ import annotations

import argparse
import hashlib
import json
import stat
from pathlib import Path


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


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("root", type=Path)
    args = parser.parse_args(argv)
    print(tree_digest(args.root))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
