from __future__ import annotations

from .common import (
    command,
    command_identity,
    enforce_command_prerequisites,
    enforce_tool_prerequisites,
    exit_blocked,
    read_payload,
    repository_root,
)
from ..state import record_event


def main() -> int:
    payload = read_payload()
    root = repository_root()
    try:
        enforce_command_prerequisites(root, command(payload))
        enforce_tool_prerequisites(root, payload)
    except ValueError as error:
        record_event(root, "permission-request", {"hook_failure": True, "failure": str(error)})
        return exit_blocked(error)
    record_event(root, "permission-request", {"command_identity": command_identity(command(payload))})
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
