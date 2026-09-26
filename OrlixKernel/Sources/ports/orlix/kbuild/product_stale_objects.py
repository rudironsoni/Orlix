#!/usr/bin/env python3
"""List product objects whose Make dependencies are newer than the object."""

from __future__ import annotations

import argparse
from pathlib import Path


def dep_paths(text: str) -> list[str]:
    body = text.replace("\\\n", " ")
    colon = body.find(":")
    if colon < 0:
        raise ValueError("dependency file has no target")
    rest = body[colon + 1 :]
    paths: list[str] = []
    index = 0
    limit = len(rest)
    while index < limit:
        while index < limit and rest[index].isspace():
            index += 1
        if index >= limit:
            break
        chars: list[str] = []
        while index < limit and not rest[index].isspace():
            if rest[index] == "\\" and index + 1 < limit:
                chars.append(rest[index + 1])
                index += 2
            else:
                chars.append(rest[index])
                index += 1
        paths.append("".join(chars))
    return paths


def stale_objects(objects: Path, cwd: Path) -> list[str]:
    stale: list[str] = []
    for obj in sorted(path for path in objects.glob("*.o") if path.is_file()):
        dep = obj.with_suffix(".d")
        try:
            text = dep.read_text(errors="surrogateescape")
        except OSError:
            stale.append(str(obj))
            continue
        if not text.strip():
            stale.append(str(obj))
            continue
        obj_mtime = obj.stat().st_mtime
        for token in dep_paths(text):
            path = Path(token)
            if not path.is_absolute():
                path = cwd / path
            try:
                dependency_mtime = path.stat().st_mtime
            except OSError:
                stale.append(str(obj))
                break
            if dependency_mtime >= obj_mtime:
                stale.append(str(obj))
                break
    return stale


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--objects", required=True, type=Path)
    parser.add_argument("--cwd", required=True, type=Path)
    args = parser.parse_args(argv)
    for path in stale_objects(args.objects, args.cwd):
        print(path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
