#!/usr/bin/env python3
"""CLI for component-mode selection. Reads changed paths and the lock schema."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

from component_selection import PROMOTED_MODE, SOURCE_MODE, select_component_mode


def main(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("--lock-schema", type=int, required=True)
    parser.add_argument("--event", default="pull_request")
    parser.add_argument("--changed-paths", required=True,
                        help="file with one changed path per line; empty means unknown")
    parser.add_argument("--out", default=None, help="optional JSON evidence destination")
    args = parser.parse_args(argv)
    paths = [
        line.strip()
        for line in Path(args.changed_paths).read_text(encoding="utf-8").splitlines()
        if line.strip()
    ] if args.changed_paths != "-" else [
        line.strip() for line in sys.stdin.read().splitlines() if line.strip()
    ]
    mode = select_component_mode(paths, args.lock_schema, event_name=args.event)
    payload = {
        "schema": 1,
        "kind": "component-mode-selection",
        "event": args.event,
        "lock_schema": args.lock_schema,
        "changed_paths": paths,
        "mode": mode,
    }
    if args.out:
        destination = Path(args.out)
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(mode)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
