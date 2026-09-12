#!/usr/bin/env python3
"""Unused. Ghostty now comes from libghostty-spm."""

from __future__ import annotations

import argparse
import os
import re
import shutil
import stat
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
    return os.path.relpath(target, start=start)


def atomic_write(path: Path, text: str) -> None:
    tmp = path.with_name(path.name + ".orlix-new")
    tmp.write_text(text, encoding="utf-8")
    if path.exists():
        os.chmod(path, stat.S_IRUSR | stat.S_IWUSR)
        path.unlink()
    tmp.replace(path)


def rewrite_existing_tree(ghostty_root: Path) -> None:
    packages = package_dirs(ghostty_root / "vendor-zig")
    for zon in ghostty_root.rglob("build.zig.zon"):
        original = zon.read_text(encoding="utf-8")
        updated = rewrite_text(original, zon.parent, packages)
        if updated != original:
            atomic_write(zon, updated)


def materialize(ghostty_root: Path, packages_root: Path) -> None:
    vendor = ghostty_root / "vendor-zig"
    vendor.mkdir(parents=True, exist_ok=True)
    for name, source in package_dirs(packages_root).items():
        shutil.copytree(source, vendor / name)
    rewrite_existing_tree(ghostty_root)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("ghostty_root")
    parser.add_argument("packages_root", nargs="?")
    parser.add_argument("--rewrite-only", action="store_true")
    args = parser.parse_args(argv)
    root = Path(args.ghostty_root)
    if args.rewrite_only:
        rewrite_existing_tree(root)
        return 0
    if not args.packages_root:
        raise SystemExit("packages_root is required unless --rewrite-only")
    materialize(root, Path(args.packages_root))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
