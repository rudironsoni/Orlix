#!/usr/bin/env python3
"""Bind the ADR 0017 proof ladder to exact subject digests. Never invent a pass."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

from bind import TIERS, BindError, bind_report, require_sha256


def load_digest(path: str) -> str:
    text = Path(path).read_text(encoding="utf-8").strip()
    return require_sha256(text)


def subjects_from_lock(path: str) -> tuple[dict[str, str], str | None]:
    payload = json.loads(Path(path).read_text(encoding="utf-8"))
    subjects: dict[str, str] = {}
    for name in ("uapi", "mlibc", "rootfs"):
        entry = (payload.get("components") or {}).get(name) or {}
        unsigned = entry.get("unsigned_digest")
        if unsigned:
            subjects[name] = require_sha256(unsigned)
    buildset = payload.get("buildset")
    return subjects, require_sha256(str(buildset)) if buildset else None


def merge_subjects(lock_subjects: dict[str, str], live: dict[str, str]) -> dict[str, str]:
    merged = dict(lock_subjects)
    for key, digest in live.items():
        if key in merged and merged[key] != digest:
            raise BindError(
                f"{key} digest {digest} does not match artifacts.lock.json {merged[key]}"
            )
        merged[key] = digest
    return merged


def select_matching_live_digest(
    lock_subjects: dict[str, str],
    name: str,
    digest_path: str | None,
    mismatch_out: str | None = None,
) -> str | None:
    if not digest_path:
        return None
    path = Path(digest_path)
    if not path.is_file() or path.stat().st_size == 0:
        return None
    live = load_digest(str(path))
    locked = lock_subjects.get(name)
    if locked and live != locked:
        if mismatch_out:
            out = Path(mismatch_out)
            out.parent.mkdir(parents=True, exist_ok=True)
            out.write_text(
                json.dumps(
                    {
                        "schema": 1,
                        "kind": "live-lock-mismatch",
                        "component": name,
                        "live_digest": live,
                        "lock_unsigned_digest": locked,
                    },
                    indent=2,
                )
                + "\n",
                encoding="utf-8",
            )
        raise BindError(f"{name} live digest {live} does not match locked digest {locked}")
    return str(path)


def write_graph(
    out_dir: str,
    *,
    subjects: dict[str, str],
    toolchain_digest: str,
    profile: str,
    destination: str,
    evidence: dict[str, str] | None = None,
    buildset_digest: str | None = None,
) -> dict:
    evidence = evidence or {}
    locked = require_sha256(buildset_digest) if buildset_digest else None
    out = Path(out_dir)
    out.mkdir(parents=True, exist_ok=True)
    reports = []
    prerequisite: list[str] = []
    for tier in TIERS:
        subject_key = {
            "kernel-dependency": "kernel",
            "kunit": "kernel",
            "kselftest": "kernel",
            "orlixmlibc": "mlibc",
            "syscall-uapi": "mlibc",
            "posix-shell": "rootfs",
            "jq": "rootfs",
            "curl": "rootfs",
            "zsh": "rootfs",
            "product-integration": "app",
        }[tier]
        if subject_key not in subjects:
            raise BindError(f"missing subject digest for {subject_key} ({tier})")
        # A blocked earlier tier stops the ladder. Later subjects are not required.
        evidence_path = evidence.get(tier)
        result = "pass" if evidence_path else "blocked"
        if result == "pass":
            path = Path(evidence_path)
            if not path.is_file() or path.stat().st_size == 0:
                raise BindError(f"{tier} pass requires a non-empty evidence file")
        report_path = out / f"{tier}.json"
        payload = bind_report(
            str(report_path),
            tier=tier,
            owner={
                "kernel-dependency": "OrlixKernel",
                "kunit": "OrlixKernel",
                "kselftest": "OrlixKernel",
                "orlixmlibc": "OrlixMLibC",
                "syscall-uapi": "OrlixMLibC",
                "posix-shell": "OrlixOS",
                "jq": "OrlixOS",
                "curl": "OrlixOS",
                "zsh": "OrlixOS",
                "product-integration": "Orlix",
            }[tier],
            subject_digest=subjects[subject_key],
            profile=profile,
            destination=destination,
            toolchain_digest=toolchain_digest,
            result=result,
            prerequisite_digests=list(prerequisite),
            buildset_digest=locked,
            evidence_path=evidence_path,
        )
        reports.append(payload)
        if result == "pass":
            prerequisite.append(hashlib.sha256(report_path.read_bytes()).hexdigest())
        else:
            break
    index = {
        "schema": 1,
        "kind": "proof-graph",
        "profile": profile,
        "destination": destination,
        "buildset_digest": locked,
        "reports": [item["proof_tier"] + ":" + item["result"] for item in reports],
        "complete": all(item["result"] == "pass" for item in reports) and len(reports) == len(TIERS),
    }
    (out / "index.json").write_text(json.dumps(index, indent=2) + "\n", encoding="utf-8")
    return index


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", required=True)
    parser.add_argument("--uapi-digest", required=True)
    parser.add_argument("--kernel-digest", required=True)
    parser.add_argument("--mlibc-digest")
    parser.add_argument("--rootfs-digest")
    parser.add_argument("--app-digest")
    parser.add_argument("--toolchain-digest", required=True)
    parser.add_argument("--profile", default="release")
    parser.add_argument("--destination", default="iphonesimulator")
    parser.add_argument("--buildset-digest")
    parser.add_argument("--lock")
    parser.add_argument(
        "--evidence",
        action="append",
        default=[],
        metavar="TIER=PATH",
        help="hosted evidence file for one ADR 0017 tier; missing tiers stay blocked",
    )
    args = parser.parse_args(argv)

    def _digest(value: str | None) -> str | None:
        if not value:
            return None
        return load_digest(value) if Path(value).is_file() else require_sha256(value)

    lock_subjects: dict[str, str] = {}
    lock_buildset = None
    if args.lock:
        lock_subjects, lock_buildset = subjects_from_lock(args.lock)
    live = {"uapi": _digest(args.uapi_digest), "kernel": _digest(args.kernel_digest)}
    if args.mlibc_digest:
        live["mlibc"] = _digest(args.mlibc_digest)
    if args.rootfs_digest:
        live["rootfs"] = _digest(args.rootfs_digest)
    if args.app_digest:
        live["app"] = _digest(args.app_digest)
    subjects = merge_subjects(lock_subjects, live)
    evidence: dict[str, str] = {}
    for item in args.evidence:
        if "=" not in item:
            raise BindError(f"evidence must be TIER=PATH, got {item}")
        tier, path = item.split("=", 1)
        evidence[tier] = path
    write_graph(
        args.out,
        subjects=subjects,
        toolchain_digest=_digest(args.toolchain_digest),
        profile=args.profile,
        destination=args.destination,
        evidence=evidence,
        buildset_digest=args.buildset_digest or lock_buildset,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
