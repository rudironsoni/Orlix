from __future__ import annotations

from .common import (
    SHELL_TOOLS,
    command,
    command_identity,
    direct_paths,
    enforce_command_prerequisites,
    enforce_tool_prerequisites,
    enforce_paths,
    envelope_bootstrap,
    exit_blocked,
    mutation,
    read_payload,
    repository_root,
    require_envelope,
    shell_write_paths,
    tool_name,
)
from ..state import record_event, restore_continuation


def run(payload: dict) -> None:
    root = repository_root()
    restore_continuation(root)
    value = command(payload)
    enforce_command_prerequisites(root, value)
    enforce_tool_prerequisites(root, payload)
    mutates = mutation(payload)
    paths = direct_paths(payload, root) if mutates else set()
    if mutates and tool_name(payload) in SHELL_TOOLS:
        paths.update(shell_write_paths(value, root))
    envelope = require_envelope(root) if mutates and not envelope_bootstrap(value) else {}
    if paths:
        enforce_paths(paths, envelope)
    record_event(root, "pre-tool-use", {"tool": tool_name(payload), "command_identity": command_identity(value), "paths": sorted(paths)})


def main() -> int:
    try:
        run(read_payload())
    except ValueError as error:
        record_event(repository_root(), "pre-tool-use", {"hook_failure": True, "failure": str(error)})
        return exit_blocked(error)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
