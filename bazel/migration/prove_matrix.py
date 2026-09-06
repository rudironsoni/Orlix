#!/usr/bin/env python3
"""Record local evidence against apple-build-matrix.json. Never pass a gated row."""

from __future__ import annotations

import argparse
import json
from pathlib import Path


class ProveError(ValueError):
    pass


def prove_rows(matrix: dict, evidence: dict[str, str]) -> dict:
    rows = {row["id"]: row for row in matrix["rows"]}
    report = []
    for row_id, path in evidence.items():
        if row_id not in rows:
            raise ProveError(f"unknown matrix row: {row_id}")
        row = rows[row_id]
        if row["result"] in {"gated", "unsupported"}:
            raise ProveError(f"{row_id} is {row['result']} and cannot be proved locally")
        evidence_path = Path(path)
        if not evidence_path.is_file() or evidence_path.stat().st_size == 0:
            raise ProveError(f"{row_id} evidence missing: {path}")
        report.append(
            {
                "id": row_id,
                "result": "supported",
                "evidence": str(evidence_path),
            }
        )
    gated = [row["id"] for row in matrix["rows"] if row["result"] == "gated"]
    return {
        "schema": 1,
        "kind": "matrix-prove",
        "proved": report,
        "gated": gated,
        "complete": False,
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--matrix", required=True)
    parser.add_argument("--out", required=True)
    parser.add_argument("--evidence", action="append", default=[], help="row_id=path")
    args = parser.parse_args(argv)
    matrix = json.loads(Path(args.matrix).read_text(encoding="utf-8"))
    evidence = {}
    for item in args.evidence:
        row_id, path = item.split("=", 1)
        evidence[row_id] = path
    payload = prove_rows(matrix, evidence)
    Path(args.out).write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
