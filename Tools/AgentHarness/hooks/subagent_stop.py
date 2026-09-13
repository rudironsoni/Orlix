from __future__ import annotations

from .common import exit_blocked, read_payload, repository_root
from ..state import agent_root, record_event, write_json


REQUIRED = (
    "findings_or_changes",
    "evidence_identities",
    "checks_performed",
    "known_failures",
    "unresolved_questions",
    "remaining_proof",
)


def main() -> int:
    payload = read_payload()
    missing = [key for key in REQUIRED if key not in payload]
    if missing:
        record_event(repository_root(), "subagent-stop", {"hook_failure": True, "failure": missing})
        return exit_blocked(ValueError("subagent result is missing: " + ", ".join(missing)))
    root = repository_root()
    result = {key: payload[key] for key in REQUIRED}
    write_json(agent_root(root) / "subagent-result.json", result)
    record_event(root, "subagent-stop", {"known_failures": result["known_failures"]})
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
