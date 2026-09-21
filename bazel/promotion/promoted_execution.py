#!/usr/bin/env python3
"""Origin-aware promoted execution proof from a Bazel execution log.

Forbidden source producers are those whose resolved origin is promoted.
Required promoted presence is the selected promoted component for that graph.
Malformed or missing evidence fails closed.
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

from source_producers import (
    classify_source_producer,
    forbidden_origins,
    matches_presence,
    required_promoted,
)


class PromotedExecutionError(ValueError):
    pass


def _iter_entries(path: Path):
    try:
        text = path.read_text(encoding="utf-8")
    except OSError as error:
        raise PromotedExecutionError(f"execution log is missing: {path}") from error
    stripped = text.lstrip()
    if not stripped:
        raise PromotedExecutionError(f"execution log is empty: {path}")
    try:
        if stripped.startswith("["):
            payload = json.loads(text)
            if not isinstance(payload, list):
                raise PromotedExecutionError(f"execution log is not a list: {path}")
            for entry in payload:
                yield entry
            return
        decoder = json.JSONDecoder()
        index = 0
        length = len(text)
        while index < length:
            while index < length and text[index].isspace():
                index += 1
            if index >= length:
                return
            entry, index = decoder.raw_decode(text, index)
            yield entry
    except json.JSONDecodeError as error:
        raise PromotedExecutionError(f"execution log is malformed: {path}: {error}") from error


def _load_origins(args) -> dict:
    selection = Path(__file__).resolve().parents[1] / "selection"
    if str(selection) not in sys.path:
        sys.path.insert(0, str(selection))
    from origin_resolver import OriginError, resolve

    try:
        vector = resolve(
            requested_mode=args.requested_mode,
            vector=json.loads(args.vector) if args.vector else None,
            facts=json.loads(args.facts) if args.facts else None,
            lock_path=args.lock,
        )
    except OriginError as error:
        raise PromotedExecutionError(str(error)) from error
    return vector.as_dict()


def prove_execution(
    execution_log: str | Path,
    origins: dict[str, str],
    *,
    profile: str,
    destination: str,
) -> dict:
    path = Path(execution_log)
    if not path.is_file():
        raise PromotedExecutionError(f"execution log is missing: {path}")
    forbidden = set(forbidden_origins(origins))
    required = required_promoted(origins, profile=profile, destination=destination)
    present = {item["origin"]: False for item in required}
    executed_forbidden: list[dict] = []
    total = 0
    for entry in _iter_entries(path):
        if not isinstance(entry, dict):
            raise PromotedExecutionError(f"execution log entry is not an object: {path}")
        total += 1
        mnemonic = entry.get("mnemonic")
        label = entry.get("targetLabel") or entry.get("target_label")
        origin = classify_source_producer(
            mnemonic if isinstance(mnemonic, str) else None,
            label if isinstance(label, str) else None,
        )
        if origin in forbidden:
            executed_forbidden.append(
                {
                    "origin": origin,
                    "mnemonic": mnemonic,
                    "targetLabel": label,
                }
            )
        for requirement in required:
            if not present[requirement["origin"]] and matches_presence(entry, requirement):
                present[requirement["origin"]] = True
    missing = [item["origin"] for item in required if not present[item["origin"]]]
    ok = not executed_forbidden and not missing
    return {
        "schema": 1,
        "kind": "promoted-execution-proof",
        "origins": dict(origins),
        "profile": profile,
        "destination": destination,
        "total_actions": total,
        "forbidden_origins": sorted(forbidden),
        "forbidden_executed": executed_forbidden,
        "required_promoted": [item["origin"] for item in required],
        "required_missing": missing,
        "ok": ok,
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--execution-log", required=True)
    parser.add_argument("--requested-mode", default="promoted", choices=("source", "promoted", "auto"))
    parser.add_argument("--vector")
    parser.add_argument("--facts")
    parser.add_argument("--lock")
    parser.add_argument("--profile", default="release")
    parser.add_argument("--destination", default="iphonesimulator")
    parser.add_argument("--origins-json", help="resolved origin vector JSON; skips the resolver")
    parser.add_argument("--out")
    args = parser.parse_args(argv)
    try:
        if args.origins_json:
            origins = json.loads(args.origins_json)
            if not isinstance(origins, dict):
                raise PromotedExecutionError("origins-json must be an object")
        else:
            origins = _load_origins(args)
        payload = prove_execution(
            args.execution_log,
            origins,
            profile=args.profile,
            destination=args.destination,
        )
    except (PromotedExecutionError, OSError, ValueError, json.JSONDecodeError) as error:
        print(f"promoted-execution: {error}", file=sys.stderr)
        return 1
    if args.out:
        destination = Path(args.out)
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps(payload, indent=2, sort_keys=True))
    return 0 if payload["ok"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
