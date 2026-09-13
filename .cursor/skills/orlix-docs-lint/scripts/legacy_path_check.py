#!/usr/bin/env python3
"""Reject references to documentation locations retired by the ontology migration."""

from __future__ import annotations

import os
from pathlib import Path


ROOT = Path(__file__).resolve().parents[4]
PATTERNS = [
    "docs/" + name + "/"
    for name in (
        "adr", "architecture", "plans", "reference", "harness", "release",
        "goals", "investigations", "codex-handoffs",
    )
] + ["Orlix/App/docs/" + "specs"]
SKIPPED_PARTS = {
    (".codex", "skills", "rulesync"),
    (".rulesync", "skills", "rulesync"),
    (".opencode", "skills", "rulesync"),
    ("tools", "docs"),
    ("Build",),
    ("third_party", "swift", ".build"),
    (".git",),
}


def skipped(path: Path) -> bool:
    parts = path.relative_to(ROOT).parts
    return (
        path.name in {".env", "secrets.env"}
        or path.name.startswith(".env.")
        or "node_modules" in parts
        or any(parts[: len(prefix)] == prefix for prefix in SKIPPED_PARTS)
    )


def main() -> int:
    problems = []
    for directory, directories, filenames in os.walk(ROOT):
        parent = Path(directory)
        directories[:] = [name for name in directories if not skipped(parent / name)]
        for name in filenames:
            path = parent / name
            if skipped(path) or not path.is_file():
                continue
            if path == ROOT / "docs" / "sources" / "tcti" / "reference-review.md":
                continue
            try:
                text = path.read_text(encoding="utf-8")
            except (UnicodeDecodeError, OSError):
                continue
            for line_number, line in enumerate(text.splitlines(), 1):
                for pattern in PATTERNS:
                    if pattern in line:
                        problems.append(f"{path.relative_to(ROOT)}:{line_number}: {pattern}")
    if problems:
        print("\n".join(problems))
        return 1
    print("legacy documentation paths: 0")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
