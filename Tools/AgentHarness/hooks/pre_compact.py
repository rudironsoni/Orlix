from __future__ import annotations

from .common import read_payload, repository_root
from ..state import record_event, save_continuation


def main() -> int:
    root = repository_root()
    continuation = save_continuation(root, read_payload())
    record_event(root, "pre-compact", {"evidence_ids": continuation["evidence_ids"]})
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
