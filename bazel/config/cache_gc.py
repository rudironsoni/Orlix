#!/usr/bin/env python3
"""Bound Bazel disk-cache growth. A cache hit is not proof."""

from __future__ import annotations

import argparse
import time
from pathlib import Path

MAX_BYTES = 30 * 1024 * 1024 * 1024
MAX_AGE_SECONDS = 30 * 24 * 60 * 60


class CacheGcError(RuntimeError):
    pass


def require_namespace(root: Path, namespace: str) -> None:
    if not namespace:
        raise CacheGcError("disk-cache namespace is required")
    if namespace not in str(root):
        raise CacheGcError(f"disk cache {root} is not namespaced as {namespace}")


def iter_files(root: Path) -> list[tuple[float, int, Path]]:
    entries: list[tuple[float, int, Path]] = []
    if not root.exists():
        return entries
    for path in root.rglob("*"):
        if not path.is_file() or path.is_symlink():
            continue
        stat = path.stat()
        entries.append((stat.st_atime, stat.st_size, path))
    return entries


def gc(
    root: Path,
    *,
    namespace: str,
    now: float | None = None,
    max_bytes: int = MAX_BYTES,
    max_age_seconds: int = MAX_AGE_SECONDS,
) -> dict:
    require_namespace(root, namespace)
    clock = time.time() if now is None else now
    removed = 0
    retained = 0
    bytes_removed = 0
    files = iter_files(root)
    keep: list[tuple[float, int, Path]] = []
    for atime, size, path in files:
        if clock - atime > max_age_seconds:
            path.unlink()
            removed += 1
            bytes_removed += size
            continue
        keep.append((atime, size, path))
        retained += size
    keep.sort()
    for atime, size, path in keep:
        if retained <= max_bytes:
            break
        path.unlink()
        removed += 1
        bytes_removed += size
        retained -= size
    return {
        "namespace": namespace,
        "root": str(root),
        "removed": removed,
        "bytes_removed": bytes_removed,
        "bytes_retained": max(retained, 0),
        "max_bytes": max_bytes,
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", required=True)
    parser.add_argument("--namespace", required=True)
    args = parser.parse_args(argv)
    payload = gc(Path(args.root), namespace=args.namespace)
    print(
        "cache-gc namespace=%s removed=%s bytes_removed=%s bytes_retained=%s"
        % (
            payload["namespace"],
            payload["removed"],
            payload["bytes_removed"],
            payload["bytes_retained"],
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
