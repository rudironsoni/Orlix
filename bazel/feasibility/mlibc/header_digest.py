"""Content digest of an installed header tree after following symlinks."""

from __future__ import annotations

import hashlib
import json
import sys
from pathlib import Path


def followed_tree_digest(root: Path) -> str:
    """Hash header bytes at each logical path, following file and directory symlinks.

    The symlink text is not part of the digest, so an absolute temporary target and a
    relative target with the same bytes produce the same digest.
    """
    if not root.is_dir():
        raise ValueError(f"missing header tree: {root}")
    records: list[tuple[str, str]] = []

    def walk(current: Path, logical_prefix: str, stack: tuple[Path, ...]) -> None:
        try:
            resolved = current.resolve()
        except OSError as error:
            raise ValueError(f"unreadable header path: {current}") from error
        if resolved in stack:
            raise ValueError(f"header symlink cycle: {current}")
        children = sorted(current.iterdir(), key=lambda child: child.name)
        nested = stack + (resolved,)
        for child in children:
            logical = child.name if not logical_prefix else logical_prefix + "/" + child.name
            if child.is_symlink() or child.is_dir():
                try:
                    target = child.resolve(strict=True)
                except OSError as error:
                    raise ValueError(f"broken header symlink: {child}") from error
                if target.is_dir():
                    walk(target, logical, nested)
                    continue
                if target.is_file():
                    records.append((logical, _file_digest(target)))
                    continue
                raise ValueError(f"unsupported header entry: {child}")
            if child.is_file():
                records.append((logical, _file_digest(child)))
                continue
            raise ValueError(f"unsupported header entry: {child}")

    walk(root, "", ())
    digest = hashlib.sha256()
    for logical, file_digest in sorted(records):
        digest.update(json.dumps([logical, file_digest], separators=(",", ":")).encode() + b"\n")
    return digest.hexdigest()


def _file_digest(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def main(argv: list[str] | None = None) -> int:
    arguments = list(sys.argv[1:] if argv is None else argv)
    if len(arguments) != 1:
        raise SystemExit("usage: header_digest.py HEADER_ROOT")
    print(followed_tree_digest(Path(arguments[0])))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
