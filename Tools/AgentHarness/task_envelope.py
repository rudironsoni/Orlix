from __future__ import annotations

import argparse
import json
import subprocess
from pathlib import Path, PurePosixPath

from .context import Ontology, repository_root, scalar, values
from .state import agent_root, read_json, update_metrics, write_json


LIST_FIELDS = (
    "owned_paths",
    "read_only_paths",
    "forbidden_paths",
    "required_skills",
    "required_proof",
    "build_intents",
    "verification_intents",
)
FORBIDDEN_FIELDS = {
    "allowed_mcp_servers",
    "allowed_mcp_workflows",
    "permission_profile",
    "sandbox_mode",
    "approval_policy",
    "native_tool_allowlist",
}


def revision(root: Path) -> str:
    return subprocess.run(
        ["git", "rev-parse", "HEAD"], cwd=root, check=True, text=True, capture_output=True
    ).stdout.strip()


def compile_envelope(root: Path, selector: str) -> dict:
    ontology = Ontology(root)
    closure = ontology.resolve(selector)
    task = ontology.task(selector)
    envelope = {
        "schema_version": 1,
        "repository_revision": revision(root),
        "task": closure["task"],
        "hierarchy": closure["hierarchy"],
        "context_paths": closure["context_paths"],
        "context_sources": closure["context_sources"],
        "context_objects": closure["objects"],
        "required_role": scalar(task.metadata, "required_role"),
    }
    for field in LIST_FIELDS:
        envelope[field] = values(task.metadata, field)
    validate_envelope(root, envelope)
    return envelope


def validate_patterns(name: str, patterns: object) -> None:
    if not isinstance(patterns, list) or not all(isinstance(item, str) and item for item in patterns):
        raise ValueError(f"{name} must be a list of non-empty strings")
    for pattern in patterns:
        path = PurePosixPath(pattern)
        if path.is_absolute() or ".." in path.parts:
            raise ValueError(f"{name} must stay repository-relative: {pattern}")


def validate_envelope(root: Path, envelope: dict, current_revision: bool = False) -> None:
    forbidden = FORBIDDEN_FIELDS.intersection(envelope)
    if forbidden:
        raise ValueError("task envelope contains native client configuration: " + ", ".join(sorted(forbidden)))
    if envelope.get("schema_version") != 1:
        raise ValueError("unsupported task envelope schema")
    task = envelope.get("task")
    if not isinstance(task, dict) or not all(task.get(key) for key in ("identity", "path", "slug", "status")):
        raise ValueError("task envelope has no complete task identity")
    if not envelope.get("required_role"):
        raise ValueError("task envelope has no required role")
    for field in ("owned_paths", "read_only_paths", "forbidden_paths"):
        validate_patterns(field, envelope.get(field))
    for field in ("required_skills", "required_proof", "build_intents", "verification_intents"):
        value = envelope.get(field)
        if not isinstance(value, list) or not all(isinstance(item, str) and item for item in value):
            raise ValueError(f"{field} must be a list of non-empty strings")
    context_paths = envelope.get("context_paths")
    validate_patterns("context_paths", context_paths)
    missing = [path for path in context_paths if not (root / path).is_file()]
    if missing:
        raise ValueError("task context path missing: " + ", ".join(missing))
    if current_revision and envelope.get("repository_revision") != revision(root):
        raise ValueError("task envelope repository revision is stale")


def envelope_path(root: Path) -> Path:
    return agent_root(root) / "task-envelope.json"


def main() -> int:
    parser = argparse.ArgumentParser()
    subcommands = parser.add_subparsers(dest="command", required=True)
    generate = subcommands.add_parser("generate")
    generate.add_argument("task")
    subcommands.add_parser("check")
    args = parser.parse_args()
    root = repository_root()
    path = envelope_path(root)
    try:
        if args.command == "generate":
            envelope = compile_envelope(root, args.task)
            write_json(path, envelope)
            from .rulesync_reports import jsonc

            mcp = jsonc((root / ".rulesync" / "mcp.jsonc").read_text(encoding="utf-8"))
            update_metrics(
                root,
                "task-select",
                {
                    "configured_mcp_servers": sorted(mcp.get("mcpServers", {})),
                    "mcp_tool_count": None,
                },
            )
            print(f"task envelope written: {path}")
        else:
            envelope = read_json(path)
            validate_envelope(root, envelope, current_revision=True)
            print(f"task envelope current: {envelope['task']['slug']}")
    except (OSError, ValueError) as error:
        raise SystemExit(str(error)) from None
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
