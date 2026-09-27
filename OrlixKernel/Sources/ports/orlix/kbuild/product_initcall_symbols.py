#!/usr/bin/env python3
"""Collect Linux initcall symbols from Mach-O nm -m output.

The archive recipe used to spawn awk once per object. This script refreshes a
per-object nm cache and prints "section symbol" lines for the product link.
"""

from __future__ import annotations

import argparse
import subprocess
from pathlib import Path

SECTIONS = (
    "__initcall_e",
    "__initcall0",
    "__initcall0s",
    "__initcall1",
    "__initcall1s",
    "__initcall2",
    "__initcall2s",
    "__initcall3",
    "__initcall3s",
    "__initcall4",
    "__initcall4s",
    "__initcall5",
    "__initcall5s",
    "__initcallrf",
    "__initcallrfs",
    "__initcall6",
    "__initcall6s",
    "__initcall7",
    "__initcall7s",
)


def collect(metadata: Path, nm: str, objects: list[str]) -> str:
    rows: list[str] = []
    metadata.mkdir(parents=True, exist_ok=True)
    for obj in objects:
        path = Path(obj)
        cache = metadata / (path.name + ".nm-m")
        if (not cache.is_file()) or cache.stat().st_mtime < path.stat().st_mtime:
            cache.write_bytes(subprocess.check_output([nm, "-m", obj]))
        for line in cache.read_text(errors="replace").splitlines():
            parts = line.split()
            if not parts or "___initcall____" not in parts[-1]:
                continue
            for section in SECTIONS:
                if "(__DATA," + section + ")" in line:
                    rows.append(section + " " + parts[-1])
                    break
    return "\n".join(rows)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--metadata", required=True, type=Path)
    parser.add_argument("--nm", required=True)
    parser.add_argument("--objects", required=True, type=Path)
    args = parser.parse_args(argv)
    objects = [line for line in args.objects.read_text().splitlines() if line]
    print(collect(args.metadata, args.nm, objects))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
