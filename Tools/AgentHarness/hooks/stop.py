from __future__ import annotations

from .common import completion_claim, exit_blocked, read_payload, repository_root
from ..proof import validate_completion
from ..state import record_event


def main() -> int:
    payload = read_payload()
    root = repository_root()
    if not completion_claim(payload):
        record_event(root, "stop", {"completion_claim": False})
        return 0
    failures = validate_completion(root)
    record_event(root, "stop", {"completion_claim": True, "hook_failure": bool(failures), "known_failures": failures})
    if failures:
        return exit_blocked(ValueError("unsupported completion claim:\n" + "\n".join(failures)))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
