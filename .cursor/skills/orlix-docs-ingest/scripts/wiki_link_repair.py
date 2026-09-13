#!/usr/bin/env python3
"""Repair broken relative Markdown links after page moves.

If a broken relative link's basename uniquely matches an existing page, rewrite
the link to that page relative to the linking file. Code fences and inline code
spans are ignored.

Usage: python3 .claude/skills/brain-ingest/scripts/wiki_link_repair.py [--dry-run] [brain-root]
"""
from __future__ import annotations

import os
import re
import sys

LINK_RE = re.compile(r"(!?\[(?:[^\[\]]|\[[^\]]*\])*\]\()([^)\s]+)((?:\s[^)]*)?\))")


def find_root(start: str) -> str:
    path = os.path.realpath(start)
    while True:
        if os.path.exists(os.path.join(path, "ontology.md")) and os.path.isdir(os.path.join(path, "objects")):
            return path
        parent = os.path.dirname(path)
        if parent == path:
            raise SystemExit("error: could not locate brain root above " + start)
        path = parent


def read_pages(root: str) -> tuple[dict[str, str], dict[str, list[str]]]:
    pages: dict[str, str] = {}
    by_name: dict[str, list[str]] = {}
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = [d for d in dirnames if not d.startswith(".")]
        for filename in filenames:
            if filename.endswith(".md"):
                path = os.path.join(dirpath, filename)
                with open(path, encoding="utf-8") as handle:
                    pages[path] = handle.read()
                by_name.setdefault(filename, []).append(path)
    return pages, by_name


def main() -> int:
    dry_run = "--dry-run" in sys.argv[1:]
    args = [arg for arg in sys.argv[1:] if arg != "--dry-run"]
    root = os.path.realpath(args[0]) if args and os.path.isdir(args[0]) else find_root(os.path.dirname(__file__))
    pages, by_name = read_pages(root)

    fixed = 0
    unresolved: list[tuple[str, str, str]] = []

    for path, text in pages.items():
        output: list[str] = []
        in_fence = False
        changed = False

        for line in text.split("\n"):
            if line.lstrip().startswith("```"):
                in_fence = not in_fence
                output.append(line)
                continue
            if in_fence:
                output.append(line)
                continue

            code_spans = [match.span() for match in re.finditer(r"`[^`\n]*`", line)]

            def replace(match: re.Match[str]) -> str:
                nonlocal changed, fixed
                if any(start <= match.start() and match.end() <= end for start, end in code_spans):
                    return match.group(0)

                prefix, target, suffix = match.group(1), match.group(2), match.group(3)
                base, sep, fragment = target.partition("#")
                if not base or base.startswith(("http://", "https://", "mailto:", "/", "file:")):
                    return match.group(0)

                resolved = os.path.realpath(os.path.join(os.path.dirname(path), base))
                if os.path.exists(resolved):
                    return match.group(0)

                hits = by_name.get(os.path.basename(base), [])
                if len(hits) == 1:
                    new_target = os.path.relpath(hits[0], os.path.dirname(path))
                    changed = True
                    fixed += 1
                    return prefix + new_target + (sep + fragment if sep else "") + suffix

                reason = "ambiguous" if len(hits) > 1 else "no match"
                unresolved.append((os.path.relpath(path, root), target, reason))
                return match.group(0)

            output.append(LINK_RE.sub(replace, line))

        if changed and not dry_run:
            with open(path, "w", encoding="utf-8") as handle:
                handle.write("\n".join(output))

    print(f"{'would fix' if dry_run else 'fixed'}: {fixed} links")
    for rel, target, reason in unresolved:
        print(f"unresolved ({reason}): {rel}: {target[:80]}")
    return 1 if unresolved else 0


if __name__ == "__main__":
    raise SystemExit(main())
