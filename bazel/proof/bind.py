#!/usr/bin/env python3
"""Bind an ADR 0017 proof report to an exact subject digest."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
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


def validate_evidence(path: str, expected: dict) -> dict:
    try:
        evidence = json.loads(Path(path).read_text())
    except (OSError, ValueError) as error:
        raise BindError("pass requires structured proof evidence") from error
    if evidence.get("kind") != "proof-evidence":
        raise BindError("pass requires proof-evidence metadata")
    for field in ("proof_tier", "subject_digest", "profile", "destination", "toolchain_digest", "buildset_digest"):
        if evidence.get(field) != expected[field]:
            raise BindError(f"evidence {field} does not match the tested subject")
    command = evidence.get("command")
    if not isinstance(command, list) or not command or not all(isinstance(arg, str) and arg for arg in command):
        raise BindError("evidence requires the executed command")
    for field in ("exit_code", "failures", "skips"):
        if type(evidence.get(field)) is not int or evidence[field] != 0:
            raise BindError(f"evidence {field} must be zero")
    tier = expected["proof_tier"]
    files = ("artifact", "log", "toolchain") + (("no_userspace_log",) if tier == "kernel-dependency" else ())
    for name in files:
        try:
            source = Path(evidence[f"{name}_path"])
            with source.open("rb") as stream:
                digest = hashlib.file_digest(stream, "sha256").hexdigest()
        except (OSError, KeyError) as error:
            raise BindError(f"evidence requires a readable {name}") from error
        if digest != evidence.get(f"{name}_digest"):
            raise BindError(f"evidence {name} content changed")
    host = "Orlix" if tier == "product-integration" else "OrlixOSTestApp"
    if evidence.get("test_host") != host:
        raise BindError(f"runtime evidence must come from {host}")
    if tier in {"kernel-dependency", "kunit", "kselftest"} and evidence["artifact_digest"] != expected["subject_digest"]:
        raise BindError("kernel proof must identify the tested kernel artifact")
    log = Path(evidence["log_path"]).read_text().replace("\r", "")
    if re.search(r"\bnot ok\s+\d+|\bBail out!|\bSKIP\b|Kernel panic|kernel panic|Oops|BUG:|Out of memory|oom-killer|Killed process|Attempted (?:to )?kill init|Assertion .* failed", log):
        raise BindError("evidence log contains a failure or skip")
    markers = {
        "kernel-dependency": ("as init process",),
        "kunit": ("ORLIX-KSELFTEST-END",),
        "kselftest": ("ORLIX-KSELFTEST-END",),
        "orlixmlibc": ("ORLIX-MLIBC-TEST-END",),
        "syscall-uapi": ("ORLIX-KSELFTEST-END",),
        "posix-shell": ("ORLIX-COREUTILS-TEST-END failures=0 skips=0",),
    }.get(tier, ("** TEST SUCCEEDED **",))
    if not all(marker in log for marker in markers):
        raise BindError(f"{tier} completion marker is missing")
    if tier == "kernel-dependency":
        if not re.search(r"Run /\S+ as init process", log):
            raise BindError("kernel boot did not reach Linux init handoff")
        no_userspace = Path(evidence["no_userspace_log_path"]).read_text()
        expected_failure = r"[^\n]*Kernel panic[^\n]*No working init found\.[^\n]*"
        if not re.search(expected_failure, no_userspace):
            raise BindError("kernel dependency proof lacks the expected no-userspace failure")
        remaining = re.sub(expected_failure, "", no_userspace)
        if re.search(r"Kernel panic|kernel panic|Oops|BUG:|Out of memory|oom-killer", remaining):
            raise BindError("no-userspace proof contains another fatal failure")
    if tier == "posix-shell" and not re.search(r"ORLIX-COREUTILS-TEST-END failures=0 skips=0 total=[1-9][0-9]*$", log, re.MULTILINE):
        raise BindError("Coreutils evidence lacks a complete zero-failure zero-skip result")
    return {**evidence, "evidence_digest": hashlib.sha256(Path(path).read_bytes()).hexdigest()}


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
    evidence_path: str | None = None,
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
    if result == "pass":
        if not evidence_path:
            raise BindError("pass requires structured proof evidence")
        payload["evidence"] = validate_evidence(evidence_path, payload)
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
    parser.add_argument("--evidence")
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
        evidence_path=args.evidence,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
