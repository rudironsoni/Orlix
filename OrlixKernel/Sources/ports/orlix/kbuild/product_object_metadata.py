#!/usr/bin/env python3
"""Collect cached Mach-O section and undefined-symbol metadata for one archive."""

from __future__ import annotations

import argparse
import subprocess
from pathlib import Path


def section_lines(text: str) -> list[str]:
    section = ""
    rows: list[str] = []
    for line in text.splitlines():
        parts = line.split()
        if len(parts) < 2:
            continue
        if parts[0] == "sectname":
            section = parts[1]
        elif parts[0] == "segname" and section:
            rows.append(parts[1] + "," + section)
            section = ""
    return rows


def undefined_lines(text: str) -> list[str]:
    rows: list[str] = []
    for line in text.splitlines():
        parts = line.split()
        if parts:
            rows.append(parts[-1])
    return rows


def _cached(cache: Path, obj: Path, command: list[str], parse) -> list[str]:
    if (not cache.is_file()) or cache.stat().st_size == 0 or cache.stat().st_mtime < obj.stat().st_mtime:
        text = subprocess.check_output(command, text=True, errors="replace")
        rows = parse(text)
        cache.write_text("".join(row + "\n" for row in rows))
        return rows
    return [line for line in cache.read_text(errors="replace").splitlines() if line]


def collect(metadata: Path, nm: str, otool: str, objects: list[str]) -> tuple[list[str], list[str]]:
    metadata.mkdir(parents=True, exist_ok=True)
    sections: set[str] = set()
    undefined: set[str] = set()
    for obj in objects:
        path = Path(obj)
        key = path.name
        sections.update(_cached(metadata / (key + ".sections"), path, [otool, "-l", obj], section_lines))
        undefined.update(_cached(metadata / (key + ".undefined"), path, [nm, "-u", obj], undefined_lines))
    return sorted(sections), sorted(undefined)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--metadata", required=True, type=Path)
    parser.add_argument("--nm", required=True)
    parser.add_argument("--otool", required=True)
    parser.add_argument("--objects", required=True, type=Path)
    parser.add_argument("--sections-out", required=True, type=Path)
    parser.add_argument("--undefined-out", required=True, type=Path)
    args = parser.parse_args(argv)
    objects = [line for line in args.objects.read_text().splitlines() if line]
    sections, undefined = collect(args.metadata, args.nm, args.otool, objects)
    args.sections_out.write_text("".join(line + "\n" for line in sections))
    args.undefined_out.write_text("".join(line + "\n" for line in undefined))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
