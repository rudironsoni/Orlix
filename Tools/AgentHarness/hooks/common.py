from __future__ import annotations

import json
import hashlib
import re
import shlex
import subprocess
import sys
from fnmatch import fnmatchcase
from pathlib import Path

from ..context import repository_root
from ..state import active_envelope, agent_root, read_json


DIRECT_WRITE_TOOLS = {"apply_patch", "edit", "multiedit", "write"}
SHELL_TOOLS = {"bash", "exec_command", "functions.exec_command", "shell"}
PATH_KEYS = {"file", "file_path", "filepath", "path", "target_path"}
GENERATED_ROOTS = (
    ".agents/",
    ".claude/",
    ".codex/",
    ".cursor/",
    ".github/agents/",
    ".github/hooks/",
    ".github/mcp/",
    ".github/permissions/",
    ".github/skills/",
)
GENERATED_FILES = {
    ".github/copilot-instructions.md",
    ".mcp.json",
    ".vscode/mcp.json",
    ".vscode/settings.json",
    "AGENTS.md",
    "CLAUDE.md",
}
PATCH_PATH = re.compile(r"^\*\*\* (?:Add|Delete|Update) File: (.+)$", re.MULTILINE)
GIT = r"git(?:\s+-C\s+(?:\"[^\"]*\"|'[^']*'|\S+))*\s+"
DESTRUCTIVE = re.compile(
    r"(?:^|[;&|]\s*)(?:sudo\s+)?rm\s"
    rf"|{GIT}(?:clean|reset\s+--hard|restore|checkout\s+--|revert|merge)\b"
    rf"|{GIT}commit\b[^;&|]*\s--amend\b"
    rf"|{GIT}push\b[^;&|]*(?:--force(?:-with-lease)?|-f\b|--delete\b)"
)
PHYSICAL = re.compile(
    r"\b(?:devicectl|ios-deploy|iphoneos)\b|generic/platform=iOS(?:[,\s]|$)|xcodebuildmcp\s+device\b"
)
PHYSICAL_TOOL = re.compile(r"(?:^|[_:/.-])device(?:[_:/.-]|$)")
SIGNING = re.compile(
    r"\b(?:beta-(?:archive|export|upload)|app-store)\b|xcodebuild\s+archive\b|-exportArchive"
    r"|xcodebuildmcp\s+device\s+(?:build|build-and-run|install|launch|test)\b"
)
SIGNING_TOOL = re.compile(
    r"(?:^|[_:/.-])device(?:[_:/.-])(?:build|build-and-run|install|launch|test)(?:[_:/.-]|$)"
)
MUTATING_SHELL = re.compile(
    r"(?:^|[;&|]\s*)(?:sudo\s+)?(?:cp|install|mkdir|mv|rm|rmdir|sed\s+-i|touch|truncate)\b"
    rf"|(?:^|[;&|]\s*){GIT}(?:add|commit|merge|push|rebase|tag)\b"
    r"|(?:^|[;&|]\s*)(?:g?make|xcodebuild)\b"
    r"|(?:^|[;&|]\s*)bazel\s+(?:build|clean|run|test)\b|(?:^|[^<])>{1,2}(?!>)"
)
ENVELOPE_BOOTSTRAP = re.compile(r"(?:^|[;&|]\s*)g?make\s+(?:agent-context|agent-task-envelope)(?:\s|$)")


class HookBlocked(ValueError):
    pass


def read_payload() -> dict:
    try:
        value = json.load(sys.stdin)
    except json.JSONDecodeError:
        return {}
    return value if isinstance(value, dict) else {}


def flatten(value: object) -> str:
    if isinstance(value, dict):
        return " ".join(flatten(item) for item in value.values())
    if isinstance(value, list):
        return " ".join(flatten(item) for item in value)
    return str(value)


def tool_name(payload: dict) -> str:
    for key in ("tool_name", "toolName", "tool", "name"):
        if isinstance(payload.get(key), str):
            return payload[key].lower()
    return ""


def tool_input(payload: dict) -> dict:
    for key in ("tool_input", "toolInput", "input", "arguments"):
        if isinstance(payload.get(key), dict):
            return payload[key]
    return payload


def command(payload: dict) -> str:
    source = tool_input(payload)
    for key in ("cmd", "command", "script"):
        if isinstance(source.get(key), str):
            return source[key]
    return ""


def command_identity(value: str) -> dict | None:
    if not value:
        return None
    try:
        verb = shlex.split(value, comments=False, posix=True)[0]
    except (IndexError, ValueError):
        verb = "unknown"
    return {"verb": verb, "sha256": hashlib.sha256(value.encode()).hexdigest()}


def repository_path(root: Path, value: str) -> str:
    path = Path(value)
    resolved = path.resolve() if path.is_absolute() else (root / path).resolve()
    try:
        return resolved.relative_to(root.resolve()).as_posix()
    except ValueError:
        return resolved.as_posix()


def direct_paths(payload: dict, root: Path) -> set[str]:
    found: set[str] = set()

    def visit(value: object, key: str = "") -> None:
        if isinstance(value, dict):
            for child_key, child in value.items():
                visit(child, child_key.lower())
        elif isinstance(value, list):
            for child in value:
                visit(child, key)
        elif isinstance(value, str):
            candidates = [value] if key in PATH_KEYS else PATCH_PATH.findall(value)
            for candidate in candidates:
                found.add(repository_path(root, candidate))

    visit(tool_input(payload))
    return found


def shell_write_paths(value: str, root: Path) -> set[str]:
    found = {repository_path(root, path) for path in PATCH_PATH.findall(value)}
    try:
        tokens = shlex.split(value, comments=False, posix=True)
    except ValueError:
        return found
    for index, token in enumerate(tokens[:-1]):
        if token in {">", ">>", "touch", "mkdir", "rmdir", "rm"}:
            candidate = tokens[index + 1]
            if not candidate.startswith("-") and "$" not in candidate:
                found.add(repository_path(root, candidate))
    segments: list[list[str]] = [[]]
    for token in tokens:
        if token in {"&&", "||", ";", "|"}:
            segments.append([])
        else:
            segments[-1].append(token)
    for segment in segments:
        if segment and segment[0] == "git":
            index = 1
            while index < len(segment) and segment[index] == "-C":
                index += 2
            if index < len(segment) and segment[index] == "add":
                found.update(
                    repository_path(root, path)
                    for path in segment[index + 1:]
                    if not path.startswith("-") and "$" not in path
                )
            continue
        words = [token for token in segment if not token.startswith("-") and "$" not in token]
        if not words:
            continue
        if words[0] == "sudo":
            words = words[1:]
        if not words:
            continue
        if words[0] in {"cp", "install", "mv", "sed"} and len(words) > 1:
            found.add(repository_path(root, words[-1]))
        if words[0] in {"mkdir", "rm", "rmdir", "touch", "truncate"}:
            found.update(repository_path(root, path) for path in words[1:])
    return found


def matches(path: str, patterns: list[str]) -> bool:
    return any(fnmatchcase(path, pattern) or path == pattern.rstrip("/") for pattern in patterns)


def generated(path: str) -> bool:
    return path in GENERATED_FILES or any(path.startswith(root) for root in GENERATED_ROOTS)


def enforce_paths(paths: set[str], envelope: dict) -> None:
    for path in sorted(paths):
        if generated(path):
            raise HookBlocked(f"generated RuleSync output is read-only: {path}")
        if matches(path, envelope.get("forbidden_paths", [])):
            raise HookBlocked(f"task envelope forbids: {path}")
        if matches(path, envelope.get("read_only_paths", [])):
            raise HookBlocked(f"task envelope marks read-only: {path}")
        owned = envelope.get("owned_paths", [])
        if owned and not matches(path, owned):
            raise HookBlocked(f"path is outside task ownership: {path}")


def mutation(payload: dict) -> bool:
    tool = tool_name(payload)
    return tool in DIRECT_WRITE_TOOLS or (tool in SHELL_TOOLS and bool(MUTATING_SHELL.search(command(payload))))


def envelope_bootstrap(value: str) -> bool:
    return bool(ENVELOPE_BOOTSTRAP.search(value))


def require_envelope(root: Path) -> dict:
    try:
        envelope = active_envelope(root)
        from ..task_envelope import validate_envelope

        validate_envelope(root, envelope, current_revision=True)
        return envelope
    except ValueError as error:
        raise HookBlocked(f"active task envelope required before mutation: {error}") from None


def gate(root: Path, name: str) -> None:
    current = subprocess.run(
        ["git", "rev-parse", "HEAD"], cwd=root, check=True, text=True, capture_output=True
    ).stdout.strip()
    if name == "physical_device_allowed":
        status = read_json(agent_root(root).parent / "orlix-tcti" / "status.json", {})
        if (
            status.get("git_sha") != current
            or status.get("physical_device_allowed") is not True
            or status.get("simulator_ladder_current") is not True
        ):
            raise HookBlocked("current simulator ladder and physical_device_allowed=true are required")
        return
    gates = read_json(agent_root(root) / "gates.json", {})
    if gates.get(name) is not True:
        raise HookBlocked(f"current structured gate is not satisfied: {name}")
    if gates.get(f"{name}_revision") != current:
        raise HookBlocked(f"current structured gate is stale: {name}")


def enforce_command_prerequisites(root: Path, value: str) -> None:
    if DESTRUCTIVE.search(value):
        raise HookBlocked("prohibited destructive operation")
    if PHYSICAL.search(value):
        gate(root, "physical_device_allowed")
    if SIGNING.search(value):
        gate(root, "signing_allowed")


def enforce_tool_prerequisites(root: Path, payload: dict) -> None:
    name = tool_name(payload)
    if PHYSICAL_TOOL.search(name):
        gate(root, "physical_device_allowed")
    if SIGNING_TOOL.search(name):
        gate(root, "signing_allowed")


def exit_blocked(error: Exception) -> int:
    print(f"ORLIX-HARNESS-BLOCK: {error}", file=sys.stderr)
    return 2


def completion_claim(payload: dict) -> bool:
    if payload.get("completion_claim") is True or payload.get("status") in {"complete", "completed", "done"}:
        return True
    text = flatten(payload).lower()
    return bool(re.search(r"\b(?:claim|status)[:=]\s*(?:complete|completed|done)\b", text))
