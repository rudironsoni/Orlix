from __future__ import annotations

import json
import os
from datetime import datetime, timezone
from pathlib import Path


def now() -> str:
    return datetime.now(timezone.utc).isoformat()


def agent_root(root: Path) -> Path:
    build = Path(os.environ.get("ORLIX_BUILD_ROOT", root / "Build"))
    return build / "AgentHarness" / "agent"


def read_json(path: Path, default: object | None = None):
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except FileNotFoundError:
        if default is not None:
            return default
        raise ValueError(f"required structured state is missing: {path}") from None
    except json.JSONDecodeError as error:
        raise ValueError(f"invalid structured state: {path}: {error}") from None


def write_json(path: Path, value: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    temporary.replace(path)


def active_envelope(root: Path) -> dict:
    value = read_json(agent_root(root) / "task-envelope.json")
    if not isinstance(value, dict):
        raise ValueError("active task envelope must be an object")
    return value


def record_event(root: Path, event: str, details: dict) -> None:
    path = agent_root(root) / "state.json"
    state = read_json(path, {"schema_version": 1, "event_counts": {}, "known_failures": []})
    counts = state.setdefault("event_counts", {})
    counts[event] = int(counts.get(event, 0)) + 1
    if details.get("failure") and event == "post-tool-use":
        command = details.get("command_identity") or {}
        failure_id = details.get("evidence_id") or command.get("sha256") or f"post-tool-use:{counts[event]}"
        failures = state.setdefault("known_failures", [])
        if not any(failure.get("id") == failure_id for failure in failures if isinstance(failure, dict)):
            failures.append({"id": failure_id, "summary": details.get("failure_summary")})
    state.update({"last_event": event, "last": details, "updated_at": now()})
    write_json(path, state)
    update_metrics(root, event, details)


def update_metrics(root: Path, event: str, details: dict) -> None:
    path = agent_root(root) / "metrics.json"
    envelope = read_json(agent_root(root) / "task-envelope.json", {})
    metrics = read_json(
        path,
        {
            "schema_version": 1,
            "skills_invoked": [],
            "subagents_started": 0,
            "hook_failures": 0,
            "proof_records_produced": 0,
            "compactions": 0,
            "continuation_restores": 0,
            "active_subagents": 0,
            "maximum_concurrency": 0,
            "configured_maximum_concurrency": 4,
        },
    )
    for key, default in (
        ("skills_invoked", []),
        ("subagents_started", 0),
        ("hook_failures", 0),
        ("proof_records_produced", 0),
        ("compactions", 0),
        ("continuation_restores", 0),
        ("active_subagents", 0),
        ("maximum_concurrency", 0),
        ("configured_maximum_concurrency", 4),
    ):
        metrics.setdefault(key, default)
    metrics["selected_task"] = envelope.get("task")
    metrics["context_paths_selected"] = envelope.get("context_paths", [])
    metrics["ontology_object_count"] = len(envelope.get("context_objects", []))
    if event == "subagent-start":
        metrics["subagents_started"] += 1
        metrics["active_subagents"] += 1
        metrics["maximum_concurrency"] = max(metrics["maximum_concurrency"], metrics["active_subagents"])
    if event == "subagent-stop":
        metrics["active_subagents"] = max(0, metrics["active_subagents"] - 1)
    if event == "pre-compact":
        metrics["compactions"] += 1
    if event == "continuation-restore":
        metrics["continuation_restores"] += 1
    if details.get("hook_failure"):
        metrics["hook_failures"] += 1
    if details.get("evidence_id"):
        metrics["proof_records_produced"] += 1
    invoked = details.get("skills_invoked", [])
    if isinstance(invoked, list):
        metrics["skills_invoked"] = sorted(set(metrics["skills_invoked"]) | {skill for skill in invoked if isinstance(skill, str)})
    metrics["configured_mcp_servers"] = details.get(
        "configured_mcp_servers", metrics.get("configured_mcp_servers", [])
    )
    metrics["mcp_tool_count"] = details.get("mcp_tool_count", metrics.get("mcp_tool_count"))
    metrics["updated_at"] = now()
    write_json(path, metrics)


def save_continuation(root: Path, payload: dict) -> dict:
    envelope = active_envelope(root)
    continuation = {
        "schema_version": 1,
        "task": envelope["task"],
        "current_hypothesis": payload.get("current_hypothesis"),
        "current_plan_state": payload.get("current_plan_state"),
        "files_changed": sorted(set(payload.get("files_changed", []))),
        "checks_run": payload.get("checks_run", []),
        "evidence_ids": sorted(set(payload.get("evidence_ids", []))),
        "known_failures": payload.get("known_failures", []),
        "next_gate": payload.get("next_gate"),
        "unresolved_decisions": payload.get("unresolved_decisions", []),
        "updated_at": now(),
    }
    write_json(agent_root(root) / "continuation.json", continuation)
    return continuation


def restore_continuation(root: Path) -> dict | None:
    directory = agent_root(root)
    lifecycle = read_json(directory / "state.json", {})
    if lifecycle.get("last_event") != "pre-compact":
        return None
    continuation = read_json(directory / "continuation.json")
    envelope = active_envelope(root)
    if continuation.get("task") != envelope.get("task"):
        raise ValueError("continuation task does not match the active task envelope")
    record_event(root, "continuation-restore", {"evidence_ids": continuation.get("evidence_ids", [])})
    return continuation
