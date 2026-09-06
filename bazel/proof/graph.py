#!/usr/bin/env python3
"""Bind the ADR 0017 proof ladder to exact subject digests. Never invent a pass."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from bind import TIERS, BindError, bind_report, require_sha256


def load_digest(path: str) -> str:
    text = Path(path).read_text(encoding="utf-8").strip()
    return require_sha256(text)


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
    out = Path(out_dir)
    out.mkdir(parents=True, exist_ok=True)
    reports = []
    prerequisite: list[str] = []
    for tier in TIERS:
        subject_key = {
            "kernel-dependency": "uapi",
            "kunit": "uapi",
            "kselftest": "uapi",
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
            buildset_digest=buildset_digest,
        )
        reports.append(payload)
        if result == "pass":
            prerequisite.append(payload["subject_digest"])
        else:
            break
    index = {
        "schema": 1,
        "kind": "proof-graph",
        "profile": profile,
        "destination": destination,
        "buildset_digest": buildset_digest,
        "reports": [item["proof_tier"] + ":" + item["result"] for item in reports],
        "complete": all(item["result"] == "pass" for item in reports) and len(reports) == len(TIERS),
    }
    (out / "index.json").write_text(json.dumps(index, indent=2) + "\n", encoding="utf-8")
    return index


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", required=True)
    parser.add_argument("--uapi-digest", required=True)
    parser.add_argument("--mlibc-digest")
    parser.add_argument("--rootfs-digest")
    parser.add_argument("--app-digest")
    parser.add_argument("--toolchain-digest", required=True)
    parser.add_argument("--profile", default="release")
    parser.add_argument("--destination", default="iphonesimulator")
    parser.add_argument("--buildset-digest")
    args = parser.parse_args(argv)

    def _digest(value: str | None) -> str | None:
        if not value:
            return None
        return load_digest(value) if Path(value).is_file() else require_sha256(value)

    subjects = {"uapi": _digest(args.uapi_digest)}
    if args.mlibc_digest:
        subjects["mlibc"] = _digest(args.mlibc_digest)
    if args.rootfs_digest:
        subjects["rootfs"] = _digest(args.rootfs_digest)
    if args.app_digest:
        subjects["app"] = _digest(args.app_digest)
    write_graph(
        args.out,
        subjects=subjects,
        toolchain_digest=_digest(args.toolchain_digest),
        profile=args.profile,
        destination=args.destination,
        buildset_digest=args.buildset_digest,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
