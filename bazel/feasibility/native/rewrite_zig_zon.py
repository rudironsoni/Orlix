#!/usr/bin/env python3
"""Replace Zig .url/.hash deps with local .path deps from a digest-pinned package tree."""

from __future__ import annotations

import argparse
import re
import shutil
from pathlib import Path

URL_HASH = re.compile(
    r'\.url\s*=\s*"[^"]+"\s*,\s*(\n[ \t]*)\.hash\s*=\s*"([^"]+)"',
    re.MULTILINE,
)


def package_dirs(packages_root: Path) -> dict[str, Path]:
    mapping: dict[str, Path] = {}
    for entry in packages_root.iterdir():
        if not entry.is_dir() or entry.name.endswith(".work"):
            continue
        mapping[entry.name] = entry
    if not mapping:
        raise SystemExit(f"no zig packages under {packages_root}")
    return mapping


def rewrite_text(text: str, zon_dir: Path, packages: dict[str, Path]) -> str:
    def replace(match: re.Match[str]) -> str:
        indent = match.group(1)
        digest = match.group(2)
        source = packages.get(digest)
        if source is None:
            return match.group(0)
        rel = Path(os_path_rel(source, zon_dir))
        return f".path = \"{rel.as_posix()}\","

    return URL_HASH.sub(replace, text)


def os_path_rel(target: Path, start: Path) -> str:
    return os_relpath(str(target), str(start))


def os_relpath(target: str, start: str) -> str:
    import os

    return os.path.relpath(target, start=start)


def materialize(ghostty_root: Path, packages_root: Path) -> None:
    vendor = ghostty_root / "vendor-zig"
    vendor.mkdir(parents=True, exist_ok=True)
    packages = {}
    for name, source in package_dirs(packages_root).items():
        dest = vendor / name
        shutil.copytree(source, dest)
        packages[name] = dest
    for zon in ghostty_root.rglob("build.zig.zon"):
        original = zon.read_text(encoding="utf-8")
        updated = rewrite_text(original, zon.parent, packages)
        if updated != original:
            zon.write_text(updated, encoding="utf-8")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("ghostty_root")
    parser.add_argument("packages_root")
    args = parser.parse_args(argv)
    materialize(Path(args.ghostty_root), Path(args.packages_root))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
