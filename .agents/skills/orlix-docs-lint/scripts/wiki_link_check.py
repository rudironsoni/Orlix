#!/usr/bin/env python3
"""Validate the Orlix ontology brain and all local Markdown links."""

from __future__ import annotations

import importlib.util
import re
import sys
from pathlib import Path


META = {"AGENTS.md", "README.md", "index.md", "log.md", "ontology.md"}
LINK_KEYS = {
    "has_story",
    "story_of",
    "has_task",
    "task_of",
    "part_of", "has_part", "depends_on", "blocks", "targets", "owned_by", "owns",
    "applies", "derived_from", "supersedes", "superseded_by", "amends", "amended_by", "relates_to",
}
DATA_KEYS = {"type", "tags", "aliases", "updated", "status", "external_id", "sources", "summary"}
STATUSES = {
    "product": {"active", "retired"},
    "software-component": {"active", "retired"},
    "architecture-decision": {"accepted", "superseded"},
    "epic": {"todo", "doing", "done"},
    "story": {"todo", "doing", "done"},
    "task": {"todo", "doing", "done"},
    "product-capability": {"implemented", "partial", "proposed", "retired"},
    "source": {"current", "superseded"},
}
INVERSES = {
    "has_story": "story_of",
    "story_of": "has_story",
    "has_task": "task_of",
    "task_of": "has_task",
    "supersedes": "superseded_by",
    "superseded_by": "supersedes",
    "amends": "amended_by",
    "amended_by": "amends",
}
HIERARCHY_LINK_TYPES = {
    "has_story": ("epic", "story"),
    "story_of": ("story", "epic"),
    "has_task": ("story", "task"),
    "task_of": ("task", "story"),
}


def root_from_args() -> Path:
    root = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else Path(__file__).resolve().parents[4] / "docs"
    if not (root / "ontology.md").is_file():
        raise SystemExit(f"error: {root} is not an ontology brain")
    return root


def frontmatter(text: str) -> str | None:
    match = re.match(r"---\n(.*?)\n---\n", text, re.DOTALL)
    return match.group(1) if match else None


def scalar(fm: str, key: str) -> str:
    match = re.search(rf"^{re.escape(key)}:\s*[\"']?([^\n\"']+)[\"']?\s*$", fm, re.MULTILINE)
    return match.group(1).strip() if match else ""


def keys(fm: str) -> set[str]:
    return {match.group(1) for match in re.finditer(r"^([a-z_][a-z0-9_]*):", fm, re.MULTILINE)}


def relations(fm: str, page: Path) -> dict[str, set[Path]]:
    result: dict[str, set[Path]] = {}
    current = ""
    for line in fm.splitlines():
        key_match = re.match(r"^([a-z_][a-z0-9_]*):\s*$", line)
        if key_match:
            current = key_match.group(1) if key_match.group(1) in LINK_KEYS else ""
            continue
        if line and not line[0].isspace():
            current = ""
        if current:
            link = re.match(r'^\s+-\s+"\[[^]]+\]\(([^)#]+)(?:#[^)]+)?\)"\s*$', line)
            if link:
                result.setdefault(current, set()).add((page.parent / link.group(1)).resolve())
    return result


def load_index_generator():
    path = Path(__file__).with_name("build_index.py")
    spec = importlib.util.spec_from_file_location("orlix_docs_build_index", path)
    assert spec and spec.loader
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def main() -> int:
    root = root_from_args()
    repository_root = root.parent
    pages = {path.resolve(): path.read_text(encoding="utf-8") for path in root.rglob("*.md")}
    problems: list[str] = []
    page_relations: dict[Path, dict[str, set[Path]]] = {}
    inbound: set[Path] = set()
    slugs: dict[str, Path] = {}

    for path, text in pages.items():
        rel = path.relative_to(root)
        if path.name not in META:
            fm = frontmatter(text)
            if fm is None:
                problems.append(f"missing-frontmatter {rel}")
                continue
            kind = scalar(fm, "type")
            status = scalar(fm, "status")
            if not kind or not scalar(fm, "updated") or "tags" not in keys(fm):
                problems.append(f"missing-required-property {rel}")
            if kind in STATUSES and status not in STATUSES[kind]:
                problems.append(f"bad-status {rel}: {status!r}")
            unknown = keys(fm) - DATA_KEYS - LINK_KEYS
            for key in sorted(unknown):
                problems.append(f"bad-key {rel}: {key}")
            expected_kind = rel.parts[1] if rel.parts[0] == "objects" and len(rel.parts) > 2 else rel.parts[0].removesuffix("s")
            if rel.parts[0] == "concepts":
                expected_kind = "concept"
            elif rel.parts[0] == "sources":
                expected_kind = "source"
            if kind != expected_kind:
                problems.append(f"type-path-mismatch {rel}: {kind!r}, expected {expected_kind!r}")
            page_relations[path] = relations(fm, path)
            if kind in {"epic", "story", "task"}:
                expected_status = rel.parts[2] if len(rel.parts) == 4 else ""
                if expected_status not in {"todo", "doing", "done"}:
                    problems.append(f"missing-status-folder {rel}")
                elif status != expected_status:
                    problems.append(f"status-folder-mismatch {rel}: {status!r}, expected {expected_status!r}")
            if kind == "epic" and not page_relations[path].get("has_story"):
                problems.append(f"epic-without-story {rel}")
            if kind == "story":
                if len(page_relations[path].get("story_of", set())) != 1:
                    problems.append(f"story-parent-count {rel}")
                if not page_relations[path].get("has_task"):
                    problems.append(f"story-without-task {rel}")
            if kind == "task" and len(page_relations[path].get("task_of", set())) != 1:
                problems.append(f"task-parent-count {rel}")
            for targets in page_relations[path].values():
                inbound.update(targets)
        slug = path.stem
        if slug in slugs and path.name not in META:
            problems.append(f"duplicate-slug {rel} and {slugs[slug].relative_to(root)}")
        elif path.name not in META:
            slugs[slug] = path

        clean = re.sub(r"```.*?```", "", text, flags=re.DOTALL)
        if re.search(r"\[\[[^]]+\]\]", clean):
            problems.append(f"wikilink {rel}")
        for match in re.finditer(r"!?\[[^]]*\]\(([^)\s]+)(?:\s+[^)]*)?\)", clean):
            target = match.group(1).split("#", 1)[0]
            if not target or target.startswith(("https://", "http://", "mailto:")):
                continue
            if target.startswith(("/", "file:")):
                problems.append(f"absolute-link {rel}: {target}")
                continue
            resolved = (path.parent / target).resolve()
            if repository_root not in resolved.parents and resolved != repository_root:
                problems.append(f"escaping-link {rel}: {target}")
            elif not resolved.exists():
                problems.append(f"broken-link {rel}: {target}")
            elif resolved in pages:
                inbound.add(resolved)

    for path, rels in page_relations.items():
        source_fm = frontmatter(pages[path]) or ""
        source_kind = scalar(source_fm, "type")
        for relation, (expected_source, expected_target) in HIERARCHY_LINK_TYPES.items():
            for target in rels.get(relation, set()):
                target_fm = frontmatter(pages.get(target, "")) or ""
                target_kind = scalar(target_fm, "type")
                if source_kind != expected_source or target_kind != expected_target:
                    target_label = target.relative_to(root) if target in pages else target
                    problems.append(
                        f"bad-hierarchy-target {path.relative_to(root)}: "
                        f"{relation} -> {target_label}"
                    )
        for relation, inverse in INVERSES.items():
            for target in rels.get(relation, set()):
                if path not in page_relations.get(target, {}).get(inverse, set()):
                    problems.append(f"missing-inverse {path.relative_to(root)}: {relation} -> {target.relative_to(root)}")
    for path in page_relations:
        if path not in inbound:
            problems.append(f"orphan {path.relative_to(root)}")

    generator = load_index_generator()
    index = root / "index.md"
    if not index.is_file() or index.read_text(encoding="utf-8") != generator.generate(root):
        problems.append("index-drift docs/index.md")

    if problems:
        for problem in sorted(set(problems)):
            print(problem)
        print(f"{len(set(problems))} problems")
        return 1
    print("0 problems")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
