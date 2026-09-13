from __future__ import annotations

import json
import re
import subprocess
import sys
from pathlib import Path

import wiki_link_check


OWNER = "rudironsoni"
REPOSITORY = "Orlix"
EXTERNAL_ID = re.compile(rf"github:{OWNER}/{REPOSITORY}#([1-9][0-9]*)")
RELATIONSHIP_HEADING = re.compile(r"^## (Parent|Blocked by)$", re.MULTILINE)
BLOCKS_SECTION = re.compile(r"^## Blocks\s*\n(.*?)(?=^## |\Z)", re.MULTILINE | re.DOTALL)
WORK_KIND = {"Epic": "epic", "Story": "story", "Task": "task"}
QUERY = """
query($endCursor: String) {
  repository(owner: "rudironsoni", name: "Orlix") {
    issues(first: 100, states: [OPEN, CLOSED], after: $endCursor,
           orderBy: {field: CREATED_AT, direction: ASC}) {
      nodes {
        number
        title
        state
        body
        parent { number }
        subIssues(first: 100) { nodes { number state } }
        blockedBy(first: 100) { nodes { number state } }
        labels(first: 100) { nodes { name } }
      }
      pageInfo { hasNextPage endCursor }
    }
  }
}
"""


def github_issues() -> list[dict]:
    try:
        result = subprocess.run(
            [
                "gh",
                "api",
                "graphql",
                "--paginate",
                "-f",
                f"query={QUERY}",
                "--jq",
                ".data.repository.issues.nodes[]",
            ],
            check=True,
            capture_output=True,
            text=True,
        )
    except subprocess.CalledProcessError as error:
        print(error.stderr.rstrip(), file=sys.stderr)
        raise SystemExit(error.returncode) from None
    return [json.loads(line) for line in result.stdout.splitlines() if line]


def agent_frontier(issues: list[dict]) -> list[int]:
    return sorted(
        issue["number"]
        for issue in issues
        if issue["state"] == "OPEN"
        and "ready-for-agent" in {label["name"] for label in issue["labels"]["nodes"]}
        and not any(child["state"] == "OPEN" for child in issue["subIssues"]["nodes"])
        and not any(blocker["state"] == "OPEN" for blocker in issue["blockedBy"]["nodes"])
    )


def has_duplicate_relationship_metadata(body: str) -> bool:
    if RELATIONSHIP_HEADING.search(body):
        return True
    blocks = BLOCKS_SECTION.search(body)
    return bool(blocks and re.search(r"^\s*-\s+#[1-9][0-9]*", blocks.group(1), re.MULTILINE))


def work_objects(root: Path) -> dict[Path, dict]:
    objects: dict[Path, dict] = {}
    for kind in ("epic", "story", "task"):
        for path in (root / "objects" / kind).rglob("*.md"):
            resolved = path.resolve()
            fm = wiki_link_check.frontmatter(path.read_text(encoding="utf-8")) or ""
            external_id = wiki_link_check.scalar(fm, "external_id")
            match = EXTERNAL_ID.fullmatch(external_id)
            objects[resolved] = {
                "kind": kind,
                "status": wiki_link_check.scalar(fm, "status"),
                "external_id": external_id,
                "number": int(match.group(1)) if match else None,
                "relations": wiki_link_check.relations(fm, resolved),
            }
    return objects


def graph_problems(objects: dict[Path, dict], issues: list[dict], root: Path) -> list[str]:
    problems = wiki_link_check.external_id_problems(
        [
            (path, data["kind"], data["external_id"])
            for path, data in objects.items()
            if data["external_id"]
        ],
        root,
    )
    mapped = {data["number"]: (path, data) for path, data in objects.items() if data["number"]}
    live = {issue["number"]: issue for issue in issues}

    for number, (path, data) in mapped.items():
        issue = live.get(number)
        rel = path.relative_to(root)
        if issue is None:
            problems.append(f"missing-github-issue {rel}: #{number}")
            continue
        expected_state = "CLOSED" if data["status"] == "done" else "OPEN"
        if issue["state"] != expected_state:
            problems.append(
                f"status-drift {rel}: {data['status']} != #{number} {issue['state'].lower()}"
            )

    for issue in issues:
        if issue["state"] == "OPEN" and has_duplicate_relationship_metadata(issue["body"] or ""):
            problems.append(f"duplicate-relationship-metadata #{issue['number']}")
        kind_match = re.match(r"^(Epic|Story|Task):", issue["title"])
        if not kind_match:
            continue
        kind = WORK_KIND[kind_match.group(1)]
        links = re.findall(
            rf"https://github\.com/{OWNER}/{REPOSITORY}/blob/main/"
            rf"(docs/objects/{kind}/[^)\s]+\.md)",
            issue["body"] or "",
        )
        for link in links:
            path = (root.parent / link).resolve()
            data = objects.get(path)
            if data is None or data["number"] != issue["number"]:
                problems.append(f"missing-ontology-object #{issue['number']}: {link}")

    expected_hierarchy: set[tuple[int, int]] = set()
    expected_dependencies: set[tuple[int, int]] = set()
    for data in objects.values():
        number = data["number"]
        if not number:
            continue
        for relation in ("has_story", "has_task"):
            for target in data["relations"].get(relation, set()):
                target_number = objects.get(target, {}).get("number")
                if target_number:
                    expected_hierarchy.add((number, target_number))
        for relation in ("story_of", "task_of"):
            for target in data["relations"].get(relation, set()):
                target_number = objects.get(target, {}).get("number")
                if target_number:
                    expected_hierarchy.add((target_number, number))
        for target in data["relations"].get("depends_on", set()):
            target_number = objects.get(target, {}).get("number")
            if target_number:
                expected_dependencies.add((number, target_number))
        for target in data["relations"].get("blocks", set()):
            target_number = objects.get(target, {}).get("number")
            if target_number:
                expected_dependencies.add((target_number, number))

    native_hierarchy = {
        (issue["parent"]["number"], issue["number"])
        for issue in issues
        if issue["number"] in mapped
        and issue["parent"]
        and issue["parent"]["number"] in mapped
    }
    native_dependencies = {
        (issue["number"], blocker["number"])
        for issue in issues
        if issue["number"] in mapped
        for blocker in issue["blockedBy"]["nodes"]
        if blocker["number"] in mapped
    }

    for parent, child in sorted(expected_hierarchy - native_hierarchy):
        problems.append(f"missing-native-hierarchy #{parent} -> #{child}")
    for parent, child in sorted(native_hierarchy - expected_hierarchy):
        problems.append(f"unexpected-native-hierarchy #{parent} -> #{child}")
    for blocked, blocker in sorted(expected_dependencies - native_dependencies):
        problems.append(f"missing-native-dependency #{blocked} blocked by #{blocker}")
    for blocked, blocker in sorted(native_dependencies - expected_dependencies):
        problems.append(f"unexpected-native-dependency #{blocked} blocked by #{blocker}")
    return sorted(set(problems))


def main() -> int:
    action = sys.argv[1] if len(sys.argv) > 1 else ""
    if action == "frontier" and len(sys.argv) == 2:
        issues = github_issues()
        result = agent_frontier(issues)
        print(
            f"action=agent-frontier repository={OWNER}/{REPOSITORY} "
            f"issues={len(issues)} frontier={len(result)}",
            file=sys.stderr,
        )
        for number in result:
            print(f"#{number}")
        return 0
    if action == "check" and len(sys.argv) == 3:
        root = Path(sys.argv[2]).resolve()
        objects = work_objects(root)
        issues = github_issues()
        problems = graph_problems(objects, issues, root)
        print(
            f"action=agent-graph-check repository={OWNER}/{REPOSITORY} "
            f"mapped={sum(data['number'] is not None for data in objects.values())} "
            f"issues={len(issues)}",
            file=sys.stderr,
        )
        for problem in problems:
            print(problem)
        print(f"{len(problems)} problems")
        return 1 if problems else 0
    print("usage: github_issue_graph.py frontier | check <docs-root>", file=sys.stderr)
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
