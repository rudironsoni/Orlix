from __future__ import annotations

from .common import read_payload, repository_root, require_envelope
from ..state import agent_root, record_event, write_json


def main() -> int:
    payload = read_payload()
    root = repository_root()
    envelope = require_envelope(root)
    packet = {
        "task": envelope["task"],
        "role": payload.get("role") or envelope["required_role"],
        "context_paths": envelope["context_paths"],
        "owned_paths": envelope["owned_paths"],
        "read_only_paths": envelope["read_only_paths"],
        "forbidden_paths": envelope["forbidden_paths"],
        "required_skills": envelope["required_skills"],
        "required_output": payload.get(
            "required_output",
            ["findings_or_changes", "evidence_identities", "checks_performed", "known_failures", "unresolved_questions", "remaining_proof"],
        ),
        "required_proof": envelope["required_proof"],
    }
    write_json(agent_root(root) / "subagent-packet.json", packet)
    record_event(root, "subagent-start", {"role": packet["role"]})
    print(__import__("json").dumps(packet, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
