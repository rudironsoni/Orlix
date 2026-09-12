from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any


def _records(path: Path) -> list[dict[str, Any]]:
    if not path.is_file():
        return []
    return [json.loads(line) for line in path.read_text().splitlines() if line.strip()]


def observe(execution: Path, bep: Path, access: str) -> dict[str, Any]:
    actions = _records(execution)
    metrics = next(
        (record["buildMetrics"] for record in reversed(_records(bep)) if "buildMetrics" in record),
        {},
    )
    summary = metrics.get("actionSummary", {})
    timing = metrics.get("timingMetrics", {})
    runners = [action.get("runner", "") for action in actions]
    return {
        "schema": 1,
        "buildbuddy_access": access,
        "remote_hits": runners.count("remote cache hit"),
        "remote_misses": None,
        "remote_uploads": None,
        "remote_downloads": None,
        "uploaded_bytes": None,
        "downloaded_bytes": None,
        "local_cache_hits": runners.count("disk cache hit"),
        "elapsed_ms": int(timing.get("wallTimeInMs", 0)),
        "actions_created": int(summary.get("actionsCreated", 0)),
        "actions_executed": int(summary.get("actionsExecuted", 0)),
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--execution", type=Path, required=True)
    parser.add_argument("--bep", type=Path, required=True)
    parser.add_argument("--access", choices=("off", "read", "write"), required=True)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    result = observe(args.execution, args.bep, args.access)
    args.out.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
