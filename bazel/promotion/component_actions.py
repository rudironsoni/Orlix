#!/usr/bin/env python3
"""Count component-producing Bazel actions in an execution log.

In a promoted build the seven component products come from the signed lock, so
the actions that produce them must not execute. This reads the execution log of
an Apple CI run and counts executed actions whose target label is one of the
component registry labels, so reuse is proven from actual execution evidence
rather than inferred from reconstruction succeeding.
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path


class ComponentActionError(ValueError):
    pass


def _registry_labels(registry_path: str | Path) -> list[str]:
    payload = json.loads(Path(registry_path).read_text(encoding="utf-8"))
    components = payload.get("components")
    if not isinstance(components, list):
        raise ComponentActionError(f"component registry has no components list: {registry_path}")
    return [entry["label"] for entry in components if isinstance(entry, dict) and entry.get("label")]


def _iter_entries(path: Path):
    text = path.read_text(encoding="utf-8")
    stripped = text.lstrip()
    if stripped.startswith("["):
        for entry in json.loads(text):
            yield entry
        return
    for line in text.splitlines():
        line = line.strip()
        if not line:
            continue
        yield json.loads(line)


def count_component_actions(execution_log: str | Path, registry: str | Path) -> dict:
    labels = set(_registry_labels(registry))
    path = Path(execution_log)
    if not path.is_file():
        raise ComponentActionError(f"execution log is missing: {path}")
    counts: dict[str, int] = {label: 0 for label in sorted(labels)}
    total = 0
    for entry in _iter_entries(path):
        if not isinstance(entry, dict):
            continue
        total += 1
        label = entry.get("targetLabel") or entry.get("target_label")
        if isinstance(label, str) and label in labels:
            counts[label] += 1
    component_actions = sum(counts.values())
    return {
        "schema": 1,
        "kind": "promoted-component-actions",
        "total_actions": total,
        "component_actions": component_actions,
        "components": counts,
    }


def main(argv=None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--execution-log", required=True)
    parser.add_argument("--registry", required=True)
    parser.add_argument("--out")
    args = parser.parse_args(argv)
    try:
        payload = count_component_actions(args.execution_log, args.registry)
    except (ComponentActionError, OSError, ValueError) as error:
        print(f"component-actions: {error}", file=sys.stderr)
        return 1
    if args.out:
        destination = Path(args.out)
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(payload["component_actions"])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
