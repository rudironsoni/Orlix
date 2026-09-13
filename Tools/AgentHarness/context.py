from __future__ import annotations

import argparse
import json
import re
from dataclasses import dataclass
from pathlib import Path


RELATIONS = {
    "has_story",
    "story_of",
    "has_task",
    "task_of",
    "part_of",
    "has_part",
    "depends_on",
    "blocks",
    "targets",
    "owned_by",
    "owns",
    "applies",
    "derived_from",
    "supersedes",
    "superseded_by",
    "amends",
    "amended_by",
    "relates_to",
}
CONTEXT_TYPES = {
    "architecture-decision",
    "concept",
    "product",
    "product-capability",
    "software-component",
    "source",
}
LINK = re.compile(r"\[[^]]+\]\(([^)]+)\)")


def repository_root() -> Path:
    return Path(__file__).resolve().parents[2]


def frontmatter(text: str) -> str:
    if not text.startswith("---\n"):
        raise ValueError("missing frontmatter")
    end = text.find("\n---\n", 4)
    if end < 0:
        raise ValueError("unterminated frontmatter")
    return text[4:end]


def values(block: str, key: str) -> list[str]:
    result: list[str] = []
    active = False
    for line in block.splitlines():
        if re.fullmatch(rf"{re.escape(key)}:\s*", line):
            active = True
            continue
        if active and line and not line[0].isspace():
            break
        if active:
            match = re.match(r"^\s+-\s+(.+?)\s*$", line)
            if match:
                result.append(match.group(1).strip("\"'"))
    return result


def scalar(block: str, key: str) -> str:
    match = re.search(rf"^{re.escape(key)}:\s*(.*?)\s*$", block, re.MULTILINE)
    return match.group(1).strip("\"'") if match else ""


@dataclass(frozen=True)
class Page:
    path: Path
    kind: str
    status: str
    external_id: str
    metadata: str

    def links(self, relation: str) -> tuple[Path, ...]:
        resolved = []
        for value in values(self.metadata, relation):
            match = LINK.search(value)
            if not match:
                raise ValueError(f"invalid {relation} link in {self.path}: {value}")
            target = (self.path.parent / match.group(1)).resolve()
            resolved.append(target)
        return tuple(resolved)


class Ontology:
    def __init__(self, root: Path):
        self.root = root.resolve()
        self.docs = self.root / "docs"

    def relative(self, path: Path) -> str:
        try:
            return path.resolve().relative_to(self.root).as_posix()
        except ValueError as error:
            raise ValueError(f"ontology link leaves repository: {path}") from error

    def page(self, path: Path) -> Page:
        path = path.resolve()
        self.relative(path)
        if not path.is_file():
            raise ValueError(f"ontology page does not exist: {self.relative(path)}")
        metadata = frontmatter(path.read_text(encoding="utf-8"))
        return Page(
            path=path,
            kind=scalar(metadata, "type"),
            status=scalar(metadata, "status"),
            external_id=scalar(metadata, "external_id"),
            metadata=metadata,
        )

    def task(self, selector: str) -> Page:
        task_root = self.docs / "objects" / "task"
        if selector.startswith("github:"):
            matches = [
                path
                for path in task_root.glob("*/*.md")
                if self.page(path).external_id == selector
            ]
        else:
            slug = selector.removesuffix(".md")
            matches = list(task_root.glob(f"*/{slug}.md"))
        if len(matches) != 1:
            raise ValueError(f"expected one ontology task for {selector}, found {len(matches)}")
        page = self.page(matches[0])
        if page.kind != "task":
            raise ValueError(f"selected object is not a task: {self.relative(page.path)}")
        return page

    def one_parent(self, page: Page, relation: str, kind: str) -> Page:
        links = page.links(relation)
        if len(links) != 1:
            raise ValueError(f"{self.relative(page.path)} must have one {relation}")
        parent = self.page(links[0])
        if parent.kind != kind:
            raise ValueError(f"{relation} target must be {kind}: {self.relative(parent.path)}")
        return parent

    def resolve(self, selector: str) -> dict:
        task = self.task(selector)
        story = self.one_parent(task, "task_of", "story")
        epic = self.one_parent(story, "story_of", "epic")
        selected = (task, story, epic)
        context: dict[Path, set[str]] = {
            task.path: {"selected-task"},
            story.path: {"task_of"},
            epic.path: {"story_of"},
        }
        for owner in selected:
            for relation in sorted(RELATIONS):
                for target_path in owner.links(relation):
                    target = self.page(target_path)
                    if target.kind not in CONTEXT_TYPES:
                        continue
                    if target.kind == "architecture-decision" and target.status != "accepted":
                        continue
                    context.setdefault(target.path, set()).add(
                        f"{self.relative(owner.path)}:{relation}"
                    )
        for path, source in (
            (self.docs / "index.md", "ontology-index"),
            (self.docs / "ontology.md", "ontology-schema"),
            (self.root / ".rulesync" / "rules" / "overview.md", "root-rule"),
        ):
            if not path.is_file():
                raise ValueError(f"required context path does not exist: {self.relative(path)}")
            context.setdefault(path.resolve(), set()).add(source)
        for relative in values(task.metadata, "context_paths"):
            path = (self.root / relative).resolve()
            self.relative(path)
            if not path.is_file():
                raise ValueError(f"task context path does not exist: {relative}")
            context.setdefault(path, set()).add("task:context_paths")
        objects = []
        for path in sorted(context, key=self.relative):
            if path.suffix != ".md" or not path.read_text(encoding="utf-8").startswith("---\n"):
                continue
            page = self.page(path)
            if not page.kind:
                continue
            objects.append(
                {
                    "identity": page.external_id or self.relative(page.path),
                    "path": self.relative(path),
                    "type": page.kind,
                    "status": page.status or None,
                }
            )
        return {
            "task": {
                "identity": task.external_id or self.relative(task.path),
                "path": self.relative(task.path),
                "slug": task.path.stem,
                "status": task.status,
            },
            "hierarchy": [self.relative(epic.path), self.relative(story.path), self.relative(task.path)],
            "context_paths": sorted(self.relative(path) for path in context),
            "context_sources": {
                self.relative(path): sorted(sources)
                for path, sources in sorted(context.items(), key=lambda item: self.relative(item[0]))
            },
            "objects": objects,
            "required_proof": values(task.metadata, "required_proof"),
        }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("task")
    args = parser.parse_args()
    try:
        result = Ontology(repository_root()).resolve(args.task)
    except ValueError as error:
        raise SystemExit(str(error)) from None
    print(json.dumps(result, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
