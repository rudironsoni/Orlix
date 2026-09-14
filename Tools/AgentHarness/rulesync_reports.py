from __future__ import annotations

import argparse
import io
import json
import re
import subprocess
import tarfile
import tempfile
from pathlib import Path

from .context import repository_root
from .state import write_json


FEATURES = ("rules", "mcp", "subagents", "skills", "hooks", "permissions")
PROTECTED_GITHUB_FEATURES = {"skills", "hooks", "subagents", "permissions", "mcp"}
COMPLETE_DESTINATIONS = (".agents/", ".claude/", ".codex/", ".cursor/")
AUTHORIZED_GITHUB_ROOTS = {
    ".github/agents/": "subagents",
    ".github/hooks/": "hooks",
    ".github/mcp/": "mcp",
    ".github/permissions/": "permissions",
    ".github/skills/": "skills",
}
AUTHORIZED_FILES = {
    ".github/copilot-instructions.md": "rules",
    ".mcp.json": "mcp",
    ".vscode/mcp.json": "mcp",
    ".vscode/settings.json": "permissions",
    "AGENTS.md": "rules",
    "CLAUDE.md": "rules",
}
LIFECYCLE = {
    "preToolUse": "Tools.AgentHarness.hooks.pre_tool_use",
    "permissionRequest": "Tools.AgentHarness.hooks.permission_request",
    "postToolUse": "Tools.AgentHarness.hooks.post_tool_use",
    "subagentStart": "Tools.AgentHarness.hooks.subagent_start",
    "subagentStop": "Tools.AgentHarness.hooks.subagent_stop",
    "preCompact": "Tools.AgentHarness.hooks.pre_compact",
    "stop": "Tools.AgentHarness.hooks.stop",
}


def jsonc(text: str) -> dict:
    output = []
    index = 0
    quoted = False
    escaped = False
    while index < len(text):
        char = text[index]
        if quoted:
            output.append(char)
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == '"':
                quoted = False
            index += 1
            continue
        if char == '"':
            quoted = True
            output.append(char)
            index += 1
            continue
        if text[index:index + 2] == "//":
            index = text.find("\n", index)
            if index < 0:
                break
            output.append("\n")
            index += 1
            continue
        if text[index:index + 2] == "/*":
            end = text.find("*/", index + 2)
            if end < 0:
                raise ValueError("unterminated JSONC comment")
            index = end + 2
            continue
        output.append(char)
        index += 1
    cleaned = "".join(output)
    while True:
        replaced = re.sub(r",\s*([}\]])", r"\1", cleaned)
        if replaced == cleaned:
            break
        cleaned = replaced
    value = json.loads(cleaned)
    if not isinstance(value, dict):
        raise ValueError("RuleSync configuration must be an object")
    return value


def run(command: list[str], cwd: Path, check: bool = True) -> subprocess.CompletedProcess:
    result = subprocess.run(command, cwd=cwd, text=True, capture_output=True)
    if check and result.returncode:
        raise ValueError((result.stderr or result.stdout).strip() or f"command failed: {command[0]}")
    return result


def pinned_version(root: Path) -> str:
    path = root / ".rulesync" / "VERSION"
    try:
        return path.read_text(encoding="utf-8").strip()
    except FileNotFoundError:
        raise ValueError(f"missing RuleSync version pin: {path}") from None


def verify_version(root: Path) -> str:
    expected = pinned_version(root)
    actual = run(["rulesync", "--version"], root).stdout.strip()
    if actual != expected:
        raise ValueError(f"rulesync version mismatch: expected {expected}, got {actual}")
    return actual


def config(root: Path) -> dict:
    return jsonc((root / "rulesync.jsonc").read_text(encoding="utf-8"))


def generate(root: Path, target: str) -> tuple[dict[str, dict[str, str]], list[str]]:
    files: dict[str, dict[str, str]] = {}
    warnings = []
    for feature in FEATURES:
        with tempfile.TemporaryDirectory(prefix="orlix-rulesync-output-") as directory:
            output = Path(directory)
            result = run(
                [
                    "rulesync",
                    "generate",
                    "--config",
                    str(root / "rulesync.jsonc"),
                    "--input-roots",
                    str(root / ".rulesync"),
                    "--output-roots",
                    str(output),
                    "--targets",
                    target,
                    "--features",
                    feature,
                ],
                root,
            )
            files[feature] = {
                path.relative_to(output).as_posix(): path.read_text(encoding="utf-8", errors="replace")
                for path in output.rglob("*")
                if path.is_file()
            }
            warnings.extend(
                line.strip()
                for line in result.stderr.splitlines()
                if "warn" in line.lower() or "skip" in line.lower()
            )
    return files, warnings


def inventory(root: Path) -> tuple[list[dict], dict[tuple[str, str], dict[str, str]], list[str]]:
    symlinks = [path.relative_to(root).as_posix() for path in (root / ".rulesync").rglob("*") if path.is_symlink()]
    if symlinks:
        raise ValueError("RuleSync source symlink is not allowed:\n" + "\n".join(sorted(symlinks)))
    settings = config(root)
    targets = settings.get("targets", [])
    if not isinstance(targets, list) or not all(isinstance(target, str) for target in targets):
        raise ValueError("rulesync targets must be a list")
    records = []
    outputs: dict[tuple[str, str], dict[str, str]] = {}
    warnings = []
    for target in targets:
        generated, target_warnings = generate(root, target)
        warnings.extend(f"{target}: {warning}" for warning in target_warnings)
        for feature, files in generated.items():
            outputs[target, feature] = files
            for path in sorted(files):
                parts = Path(path).parts
                destination = path
                if len(parts) > 1:
                    destination = "/".join(parts[:2]) if parts[0] == ".github" else parts[0]
                records.append(
                    {
                        "target": target,
                        "feature": feature,
                        "generated_path": path,
                        "generated_destination_root": destination,
                    }
                )
    validate_inventory(records)
    return records, outputs, sorted(set(warnings))


def validate_inventory(records: list[dict]) -> None:
    invalid = sorted(
        record["generated_path"]
        for record in records
        if AUTHORIZED_FILES.get(record["generated_path"]) != record["feature"]
        and not any(record["generated_path"].startswith(root) for root in COMPLETE_DESTINATIONS)
        and not any(
            record["generated_path"].startswith(root) and record["feature"] == feature
            for root, feature in AUTHORIZED_GITHUB_ROOTS.items()
        )
    )
    if invalid:
        raise ValueError("RuleSync generated outside authorized destinations:\n" + "\n".join(invalid))


def inventory_report(root: Path) -> dict:
    records, _, warnings = inventory(root)
    report = {"rulesync_version": pinned_version(root), "outputs": records, "warnings": warnings}
    write_json(root / "Build" / "AgentHarness" / "rulesync" / "output-inventory.json", report)
    return report


def capability_report(root: Path) -> dict:
    records, outputs, warnings = inventory(root)
    settings = config(root)
    targets = {}
    for target in settings["targets"]:
        generated = {record["feature"] for record in records if record["target"] == target}
        subagents = outputs.get((target, "subagents"), {})
        subagent_text = "\n".join(subagents.values())
        restricted = [
            text
            for path, text in subagents.items()
            if any(role in path for role in ("bazel-inspector", "orlix-planner", "orlix-reviewer", "tcti-llvm-inspector"))
        ]
        restriction_markers = {
            "codexcli": ('sandbox_mode = "read-only"', 'approval_policy = "never"'),
            "claudecode": ("permissionMode: plan", "tools:"),
            "cursor": ("readonly: true",),
            "copilot": ("tools:",),
        }.get(target, ())
        native_restriction = len(restricted) == 4 and all(
            all(marker in text for marker in restriction_markers) for text in restricted
        )
        hook_text = "\n".join(outputs.get((target, "hooks"), {}).values())
        targets[target] = {
            "rules": "native" if "rules" in generated else "unsupported",
            "skills": "native" if "skills" in generated else "unsupported",
            "subagents": "native" if "subagents" in generated else "unsupported",
            "hooks": "native" if "hooks" in generated else "unsupported",
            "permissions": "native" if "permissions" in generated else "unsupported",
            "MCP": "native" if "mcp" in generated else "unsupported",
            "role_specific_tool_restrictions": "native" if native_restriction else "instruction-only" if "subagents" in generated else "unsupported",
            "role_specific_write_restrictions": "native" if native_restriction else "instruction-only" if "subagents" in generated else "unsupported",
            "role_specific_MCP_restrictions": "instruction-only" if "subagents" in generated else "unsupported",
            "subagent_concurrency": "instruction-only",
            "approval_policy": "native" if "approval_policy" in subagent_text or "permissionMode:" in subagent_text else "instruction-only",
            "sandbox_restrictions": "native" if "sandbox_mode" in subagent_text or "readonly: true" in subagent_text else "instruction-only",
            "lifecycle_events": {
                event: "hook-enforced" if command_name in hook_text else "unsupported"
                for event, command_name in LIFECYCLE.items()
            },
        }
    report = {
        "rulesync_version": pinned_version(root),
        "source": "RuleSync isolated generation from canonical configuration",
        "project_subagent_concurrency": {
            "classification": "instruction-only",
            "maximum": 4,
            "reason": "RuleSync 16.26.1 skips the Codex agents table",
        },
        "targets": targets,
        "warnings": warnings,
    }
    write_json(root / "Build" / "AgentHarness" / "rulesync" / "capabilities.json", report)
    return report


def extract_revision(repo: Path, revision: str, destination: Path) -> None:
    archive = subprocess.run(
        ["git", "archive", "--format=tar", revision], cwd=repo, check=True, stdout=subprocess.PIPE
    ).stdout
    with tarfile.open(fileobj=io.BytesIO(archive), mode="r:") as bundle:
        for member in bundle.getmembers():
            if not (member.name == "rulesync.jsonc" or member.name.startswith(".rulesync/")):
                continue
            if not member.isfile():
                continue
            target = (destination / member.name).resolve()
            try:
                target.relative_to(destination.resolve())
            except ValueError as error:
                raise ValueError(f"unsafe archive path: {member.name}") from error
            target.parent.mkdir(parents=True, exist_ok=True)
            source = bundle.extractfile(member)
            if source is None:
                raise ValueError(f"cannot extract RuleSync source: {member.name}")
            target.write_bytes(source.read())


def revision_inventory(repo: Path, revision: str) -> list[dict]:
    with tempfile.TemporaryDirectory(prefix="orlix-rulesync-source-") as directory:
        source = Path(directory)
        extract_revision(repo, revision, source)
        if not (source / "rulesync.jsonc").is_file():
            return []
        expected = pinned_version(source)
        actual = run(["rulesync", "--version"], source).stdout.strip()
        if actual != expected:
            raise ValueError(f"rulesync version mismatch for {revision}: expected {expected}, got {actual}")
        return inventory(source)[0]


def revision_pin(repo: Path, revision: str) -> str | None:
    result = run(["git", "show", f"{revision}:.rulesync/VERSION"], repo, check=False)
    return result.stdout.strip() if result.returncode == 0 else None


def paths_from_name_status(data: bytes) -> set[str]:
    fields = data.decode("utf-8", "surrogateescape").split("\0")
    result = set()
    index = 0
    while index < len(fields) and fields[index]:
        status = fields[index]
        index += 1
        count = 2 if status[:1] in {"R", "C"} else 1
        for _ in range(count):
            if index < len(fields) and fields[index]:
                result.add(fields[index])
            index += 1
    return result


def changed_paths(repo: Path, base: str, head: str) -> set[str]:
    result = subprocess.run(
        ["git", "diff", "--name-status", "--find-renames", "--find-copies-harder", "-z", f"{base}...{head}"],
        cwd=repo,
        check=True,
        stdout=subprocess.PIPE,
    )
    return paths_from_name_status(result.stdout)


def protected(records: list[dict]) -> tuple[set[str], set[str]]:
    exact = {
        record["generated_path"]
        for record in records
        if not record["generated_path"].startswith(".github/")
        or record["feature"] in PROTECTED_GITHUB_FEATURES
    }
    roots = {
        root
        for root in COMPLETE_DESTINATIONS
        if any(path.startswith(root) for path in exact)
    }
    return exact, roots


def authorized(records: list[dict]) -> tuple[set[str], set[str]]:
    validate_inventory(records)
    exact = {record["generated_path"] for record in records}
    roots = {root for root in COMPLETE_DESTINATIONS if any(path.startswith(root) for path in exact)}
    return exact, roots


def reject(paths: set[str], records: list[dict]) -> None:
    exact, roots = protected(records)
    rejected = sorted(path for path in paths if path in exact or any(path.startswith(root) for root in roots))
    if rejected:
        raise ValueError("generated RuleSync output changed:\n" + "\n".join(rejected))


def pr_guard(root: Path, base: str, head: str) -> None:
    base_pin = revision_pin(root, base)
    head_pin = revision_pin(root, head)
    if base_pin and head_pin and base_pin != head_pin:
        raise ValueError(
            f"RuleSync-version migration requires a dedicated upgrade path: base pins {base_pin}, head pins {head_pin}"
        )
    records = revision_inventory(root, base) + revision_inventory(root, head)
    reject(changed_paths(root, base, head), records)


def generated_pr_guard(root: Path, pr_number: str, source_revision: str, head_revision: str) -> None:
    if not re.fullmatch(r"[0-9a-f]{40}", source_revision):
        raise ValueError("source revision must be a complete lowercase SHA")
    if not re.fullmatch(r"[0-9a-f]{40}", head_revision):
        raise ValueError("head revision must be a complete lowercase SHA")
    if not pr_number.isdigit() or int(pr_number) < 1:
        raise ValueError("pull request number must be positive")
    details = json.loads(
        run(
            [
                "gh",
                "pr",
                "view",
                pr_number,
                "--repo",
                "rudironsoni/Orlix",
                "--json",
                "baseRefName,body,headRefName,headRefOid,isCrossRepository,state",
            ],
            root,
        ).stdout
    )
    branch = f"automation/rulesync-{source_revision}"
    if details["state"] != "OPEN" or details["baseRefName"] != "main":
        raise ValueError("generated pull request must be open against main")
    if details["isCrossRepository"] or details["headRefName"] != branch:
        raise ValueError(f"generated pull request head must be {branch}")
    required_body = [
        "rulesync-generated: true",
        f"source-revision: {source_revision}",
        f"rulesync-version: {pinned_version(root)}",
    ]
    if details["body"].splitlines() != required_body:
        raise ValueError("generated pull request body metadata is invalid")
    head = details["headRefOid"]
    if head != head_revision:
        raise ValueError("generated pull request head changed before verification")
    run(
        [
            "git",
            "fetch",
            "--no-tags",
            "origin",
            "+refs/heads/main:refs/remotes/origin/main",
            f"+refs/heads/{branch}:refs/remotes/origin/{branch}",
        ],
        root,
    )
    fetched_head = run(["git", "rev-parse", f"refs/remotes/origin/{branch}"], root).stdout.strip()
    if fetched_head != head:
        raise ValueError("generated pull request head changed before verification")
    ancestor = run(
        ["git", "merge-base", "--is-ancestor", source_revision, "refs/remotes/origin/main"], root, check=False
    )
    if ancestor.returncode != 0:
        raise ValueError("source revision is not an ancestor of origin/main")
    canonical = run(
        ["git", "diff", "--quiet", source_revision, "refs/remotes/origin/main", "--", ".rulesync", "rulesync.jsonc"],
        root,
        check=False,
    )
    if canonical.returncode != 0:
        raise ValueError("generated pull request source is stale: canonical RuleSync inputs changed on main")
    parents = run(["git", "rev-list", "--parents", "-n", "1", head], root).stdout.split()
    if len(parents) != 2 or parents[1] != source_revision:
        raise ValueError("generated commit must have the source revision as its only parent")
    message = [line for line in run(["git", "log", "-1", "--format=%s%n%b", head], root).stdout.splitlines() if line]
    required_message = [
        "chore: generate agent files",
        f"source-revision: {source_revision}",
        f"rulesync-version: {pinned_version(root)}",
    ]
    if message != required_message:
        raise ValueError("generated commit metadata is invalid")
    with tempfile.TemporaryDirectory(prefix="orlix-rulesync-verify-") as directory:
        expected = Path(directory) / "source"
        run(["git", "worktree", "add", "--detach", str(expected), source_revision], root)
        try:
            run(["rulesync", "generate"], expected)
            validate_write_set(expected)
            run(["git", "add", "-A"], expected)
            expected_tree = run(["git", "write-tree"], expected).stdout.strip()
        finally:
            run(["git", "worktree", "remove", "--force", str(expected)], root, check=False)
    actual_tree = run(["git", "rev-parse", f"{head}^{{tree}}"], root).stdout.strip()
    if expected_tree != actual_tree:
        raise ValueError("generated pull request does not exactly match independent RuleSync generation")


def worktree_paths(root: Path) -> set[str]:
    result = subprocess.run(
        ["git", "status", "--porcelain=v1", "-z", "--untracked-files=all"],
        cwd=root,
        check=True,
        stdout=subprocess.PIPE,
    )
    fields = result.stdout.decode("utf-8", "surrogateescape").split("\0")
    paths = set()
    index = 0
    while index < len(fields) and fields[index]:
        record = fields[index]
        index += 1
        paths.add(record[3:])
        if ({"R", "C"} & set(record[:2])) and index < len(fields) and fields[index]:
            paths.add(fields[index])
            index += 1
    return paths


def validate_write_set(root: Path) -> None:
    records = revision_inventory(root, "HEAD")
    parent = run(["git", "rev-parse", "--verify", "HEAD^"], root, check=False)
    if parent.returncode == 0:
        records += revision_inventory(root, "HEAD^")
    validate_generated_write_paths(worktree_paths(root), records)


def validate_generated_write_paths(paths: set[str], records: list[dict]) -> None:
    exact, roots = authorized(records)
    invalid = sorted(path for path in paths if path not in exact and not any(path.startswith(prefix) for prefix in roots))
    if invalid:
        raise ValueError("RuleSync generation wrote outside the authorized set:\n" + "\n".join(invalid))


def main() -> int:
    parser = argparse.ArgumentParser()
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("inventory")
    commands.add_parser("capabilities")
    guard = commands.add_parser("pr-guard")
    guard.add_argument("base")
    guard.add_argument("head")
    generated_guard = commands.add_parser("generated-pr-guard")
    generated_guard.add_argument("pr_number")
    generated_guard.add_argument("source_revision")
    generated_guard.add_argument("head_revision")
    commands.add_parser("validate-write-set")
    args = parser.parse_args()
    root = repository_root()
    try:
        if args.command == "inventory":
            verify_version(root)
            report = inventory_report(root)
            print(f"RuleSync inventory written: {len(report['outputs'])} entries")
        elif args.command == "capabilities":
            verify_version(root)
            capability_report(root)
            print("RuleSync capability report written")
        elif args.command == "pr-guard":
            verify_version(root)
            pr_guard(root, args.base, args.head)
            print("RuleSync generated-output guard passed")
        elif args.command == "generated-pr-guard":
            verify_version(root)
            generated_pr_guard(root, args.pr_number, args.source_revision, args.head_revision)
            print("RuleSync generated pull request matches independent generation")
        else:
            verify_version(root)
            validate_write_set(root)
            print("RuleSync write set valid")
    except (OSError, ValueError) as error:
        raise SystemExit(str(error)) from None
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
