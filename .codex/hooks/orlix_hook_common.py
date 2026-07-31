#!/usr/bin/env python3
"""Shared helpers for repository lifecycle hooks."""

from __future__ import annotations

import hashlib
import json
import os
import re
import shlex
import subprocess
import sys
import tempfile
import time
from pathlib import Path


WRITE_TOOLS = {"apply_patch", "edit", "write", "multiedit"}
BASH_TOOLS = {"bash", "exec_command", "functions.exec_command"}
BAD_OUTPUT_RE = re.compile(r"\b(?:FAIL|ERROR|not ok|panic|SIGSEGV|crash)\b", re.IGNORECASE)
MACOS_RUNTIME_RE = re.compile(r"\bmacOS\s+(?:runtime|userspace|userland)\b", re.IGNORECASE)
CONTAINER_SUPPORT_RE = re.compile(r"\b(?:Docker|container) support\b", re.IGNORECASE)
OCI_ORLIX_RE = re.compile(r"\bOCI(?:-derived)?\b", re.IGNORECASE)
INVENTED_MECHANISM_RE = re.compile(r"\b(?:add|create|build|introduce|implement)\s+(?:OrlixImageBuilder|OrlixLinuxOracle|\.workflow/)\b", re.IGNORECASE)
WORKFLOW_DELEGATION_RE = re.compile(r"\b(?:subagent|subagents|swarm|parallel agents?)\b", re.IGNORECASE)
WORKFLOW_AUTH_RE = re.compile(r"\b(?:authorized|authorised|explicitly requested|user requested)\b", re.IGNORECASE)
OCI_RUNTIME_CLAIM_RE = re.compile(r"\bOCI(?: Runtime Spec)? (?:support|compatible|compliant)\b", re.IGNORECASE)
OCI_LIFECYCLE_RE = re.compile(r"\bcreate\b.*\bstart\b.*\bstate\b.*\bkill\b.*\bdelete\b", re.IGNORECASE | re.DOTALL)
IMAGE_ONLY_OCI_RE = re.compile(r"\b(?:image|layout|rootfs) import\b", re.IGNORECASE)
CLAIM_RE = re.compile(r"\b(?:complete|completed|done|fixed|green|passes|passing|runtime-ready|package-ready)\b", re.IGNORECASE)
KNOWLEDGE_PATHS = (
    "AGENTS.md",
    "docs/objects",
    "docs/concepts",
    "docs/sources",
    "docs/ontology.md",
    "docs/README.md",
    "docs/AGENTS.md",
)


def read_stdin_text() -> str:
    return sys.stdin.read()


def parse_json(text: str):
    try:
        return json.loads(text)
    except json.JSONDecodeError:
        return {}


def flattened_text(value) -> str:
    if isinstance(value, dict):
        return " ".join(flattened_text(item) for item in value.values())
    if isinstance(value, list):
        return " ".join(flattened_text(item) for item in value)
    return str(value)


def repo_root() -> Path:
    try:
        return Path(subprocess.check_output(["git", "rev-parse", "--show-toplevel"], text=True).strip())
    except Exception:
        return Path.cwd()


def warn(message: str) -> None:
    print(f"ORLIX-HARNESS-WARN: {message}", file=sys.stderr)


def block(message: str) -> None:
    print(f"ORLIX-HARNESS-BLOCK: {message}", file=sys.stderr)
    raise SystemExit(2)


def _frontmatter_scalar(text: str, key: str) -> str:
    match = re.match(r"---\n(.*?)\n---\n", text, re.DOTALL)
    if not match:
        return ""
    value = re.search(rf"^{key}:\s*[\"']?([^\n\"']+)", match.group(1), re.MULTILINE)
    return value.group(1).strip() if value else ""


def doing_work_pages(root: Path) -> list[Path]:
    objects_root = root / "docs" / "objects"
    pages: list[Path] = []
    for kind in ("epic", "story", "task"):
        status_root = objects_root / kind / "doing"
        if status_root.is_dir():
            pages.extend(status_root.glob("*.md"))
    return sorted(pages)


def required_plan_context_paths(root: Path) -> list[Path]:
    paths = [root / "AGENTS.md", root / "docs" / "index.md"]
    paths.extend(doing_work_pages(root))
    return [path for path in paths if path.is_file()]


def _state_path(root: Path) -> Path:
    digest = hashlib.sha256(str(root.resolve()).encode()).hexdigest()[:24]
    return Path(os.environ.get("ORLIX_PLAN_GUARD_STATE_DIR", tempfile.gettempdir())) / "orlix-ontology-context-guard" / f"{digest}.json"


def load_plan_context_state(root: Path) -> dict:
    path = _state_path(root)
    try:
        return json.loads(path.read_text())
    except Exception:
        return {"read_paths": [], "knowledge_mutation_time": 0.0, "log_update_time": 0.0, "index_update_time": 0.0}


def _save_state(root: Path, state: dict) -> None:
    path = _state_path(root)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(state, sort_keys=True))


def plan_context_loaded(root: Path, state: dict | None = None) -> bool:
    state = state or load_plan_context_state(root)
    required = {str(path.resolve()) for path in required_plan_context_paths(root)}
    return required.issubset(set(state.get("read_paths", [])))


def _tool_name(payload) -> str:
    for key in ("tool_name", "tool", "toolName", "name"):
        value = payload.get(key) if isinstance(payload, dict) else None
        if isinstance(value, str):
            return value.lower()
    return ""


def _command(payload) -> str:
    if not isinstance(payload, dict):
        return ""
    for key in ("tool_input", "input", "parameters", "args", "arguments"):
        value = payload.get(key)
        if isinstance(value, dict):
            for command_key in ("command", "cmd", "script"):
                command = value.get(command_key)
                if isinstance(command, str):
                    return command
    return ""


def tool_mutates_workspace(payload) -> bool:
    name = _tool_name(payload)
    if name in WRITE_TOOLS:
        return True
    if name not in BASH_TOOLS:
        return False
    command = _command(payload)
    return bool(re.search(r"(?:^|[;&|]\s*)(?:rtk\s+(?:proxy\s+)?)?(?:apply_patch|make|rm|mkdir|mv|cp|install|touch|git\s+(?:apply|commit|push))\b|(?:^|\s)(?:>|>>)\s*\S", command))


def tool_requires_plan_context(payload) -> bool:
    return tool_mutates_workspace(payload) and not is_git_commit_or_push(payload)


def is_git_commit_or_push(payload) -> bool:
    return bool(re.search(r"(?:^|[;&|]\s*)(?:rtk\s+(?:proxy\s+)?)?git\s+(?:commit|push)\b", _command(payload)))


def oversized_goal_messages(root: Path) -> list[str]:
    return [f"{path.relative_to(root)} exceeds the 16000 character work-page limit" for path in doing_work_pages(root) if len(path.read_text(errors="replace")) > 16000]


def plan_context_post_update(payload) -> None:
    root = repo_root()
    state = load_plan_context_state(root)
    text = flattened_text(payload).replace("\\n", "\n")
    read_paths = set(state.get("read_paths", []))
    if not tool_mutates_workspace(payload):
        for path in required_plan_context_paths(root):
            if str(path) in text or str(path.relative_to(root)) in text:
                read_paths.add(str(path.resolve()))
    state["read_paths"] = sorted(read_paths)
    if tool_mutates_workspace(payload):
        now = time.time()
        if re.search(r"(?:^|\s)docs/(?:objects|concepts|sources|ontology\.md|README\.md|AGENTS\.md)", text):
            state["knowledge_mutation_time"] = now
        if "docs/log.md" in text:
            state["log_update_time"] = now
        if "docs/index.md" in text or "build_index.py" in text:
            state["index_update_time"] = now
    _save_state(root, state)


def knowledge_paths_dirty(root: Path) -> bool | None:
    result = subprocess.run(
        ["git", "status", "--porcelain=v1", "-z", "--untracked-files=all", "--", *KNOWLEDGE_PATHS],
        cwd=root,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        check=False,
    )
    if result.returncode != 0:
        return None
    return bool(result.stdout)


def knowledge_updates_current(root: Path, state: dict) -> bool:
    dirty = knowledge_paths_dirty(root)
    if dirty is None:
        return False
    if not dirty:
        return True
    mutation = float(state.get("knowledge_mutation_time", 0.0) or 0.0)
    return bool(mutation) and (
        float(state.get("log_update_time", 0.0) or 0.0) >= mutation
        and float(state.get("index_update_time", 0.0) or 0.0) >= mutation
    )


def macos_runtime_wording(text: str) -> bool:
    return bool(MACOS_RUNTIME_RE.search(text))


def vague_container_support(text: str) -> bool:
    return bool(CONTAINER_SUPPORT_RE.search(text) and not OCI_ORLIX_RE.search(text))


def invented_mechanism_without_scope(text: str) -> bool:
    return bool(INVENTED_MECHANISM_RE.search(text))


def workflow_without_authorization(text: str) -> bool:
    return bool(WORKFLOW_DELEGATION_RE.search(text) and not WORKFLOW_AUTH_RE.search(text))


def oci_runtime_claim_without_lifecycle(text: str) -> bool:
    return bool(OCI_RUNTIME_CLAIM_RE.search(text) and not OCI_LIFECYCLE_RE.search(text))


def orlix_run_claim_without_lifecycle(text: str) -> bool:
    return bool(re.search(r"\borlix run\b.*\b(?:complete|supported|ready)\b", text, re.IGNORECASE) and not OCI_LIFECYCLE_RE.search(text))


def oci_compatible_from_image_only(text: str) -> bool:
    return bool(OCI_RUNTIME_CLAIM_RE.search(text) and IMAGE_ONLY_OCI_RE.search(text) and not OCI_LIFECYCLE_RE.search(text))


def claim_evidence_terms(text: str):
    return ()


def has_claim_relevant_evidence(path: Path, terms=()):
    return False


def has_implementation_evidence(path: Path):
    return False


def stale_remaining_task_list(text: str) -> bool:
    return bool(re.search(r"\bstale remaining(?:-task)? list\b", text, re.IGNORECASE))
