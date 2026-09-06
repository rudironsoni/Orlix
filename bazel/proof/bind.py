#!/usr/bin/env python3
"""Bind an ADR 0017 proof report to an exact subject digest."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "promotion"))
from compare import require_sha256  # noqa: E402

TIERS = (
    "kernel-dependency",
    "kunit",
    "kselftest",
    "orlixmlibc",
    "syscall-uapi",
    "posix-shell",
    "jq",
    "curl",
    "zsh",
    "product-integration",
)


class BindError(ValueError):
    pass


def bind_report(
    path: str,
    *,
    tier: str,
    owner: str,
    subject_digest: str,
    profile: str,
    destination: str,
    toolchain_digest: str,
    result: str,
    prerequisite_digests: list[str] | None = None,
    buildset_digest: str | None = None,
    cache_hit_only: bool = False,
) -> dict:
    if cache_hit_only:
        raise BindError("a cache hit cannot authorize promotion")
    if tier not in TIERS:
        raise BindError(f"unknown proof tier: {tier}")
    if result not in {"pass", "fail", "blocked"}:
        raise BindError(f"unknown proof result: {result}")
    digest = require_sha256(subject_digest)
    toolchain = require_sha256(toolchain_digest)
    prerequisites = [require_sha256(item) for item in (prerequisite_digests or [])]
    payload = {
        "schema": 1,
        "kind": "proof-report",
        "proof_tier": tier,
        "owner": owner,
        "subject_digest": digest,
        "buildset_digest": buildset_digest,
        "profile": profile,
        "destination": destination,
        "toolchain_digest": toolchain,
        "prerequisite_digests": prerequisites,
        "result": result,
        "cache_hit_only": False,
    }
    Path(path).write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
    return payload


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--report", required=True)
    parser.add_argument("--tier", required=True)
    parser.add_argument("--owner", required=True)
    parser.add_argument("--subject-digest", required=True)
    parser.add_argument("--profile", required=True)
    parser.add_argument("--destination", required=True)
    parser.add_argument("--toolchain-digest", required=True)
    parser.add_argument("--result", required=True)
    parser.add_argument("--buildset-digest")
    parser.add_argument("--prerequisite-digest", action="append", default=[])
    args = parser.parse_args(argv)
    bind_report(
        args.report,
        tier=args.tier,
        owner=args.owner,
        subject_digest=args.subject_digest,
        profile=args.profile,
        destination=args.destination,
        toolchain_digest=args.toolchain_digest,
        result=args.result,
        prerequisite_digests=args.prerequisite_digest,
        buildset_digest=args.buildset_digest,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
