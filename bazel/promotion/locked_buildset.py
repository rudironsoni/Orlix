#!/usr/bin/env python3
"""Fail closed unless artifacts.lock.json names a digest-pinned signed buildset."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
from pathlib import Path

REQUIRED = ("uapi", "mlibc", "rootfs")
OCI_PREFIX = "sha256:"


class LockedBuildsetError(ValueError):
    pass


def require_sha256(digest: str) -> str:
    text = str(digest).strip()
    if len(text) != 64 or any(c not in "0123456789abcdef" for c in text):
        raise LockedBuildsetError(f"invalid sha256 digest: {digest!r}")
    return text


def validate_component(name: str, entry: dict) -> dict:
    if not re.fullmatch(r"[a-z0-9]+(?:-[a-z0-9]+)*", name):
        raise LockedBuildsetError(f"invalid component name: {name!r}")
    unsigned = require_sha256(entry["unsigned_digest"])
    oci = entry.get("oci_digest")
    if not isinstance(oci, str) or not oci.startswith(OCI_PREFIX):
        raise LockedBuildsetError(f"{name} lock entry missing oci_digest")
    require_sha256(oci[len(OCI_PREFIX):])
    reference = entry.get("oci_reference") or ""
    expected = f"ghcr.io/rudironsoni/orlix/{name}@{oci}"
    if reference != expected:
        raise LockedBuildsetError(f"{name} requires {expected}, not {reference!r}; mutable latest is forbidden")
    return {"unsigned_digest": unsigned, "oci_digest": oci, "oci_reference": reference}


def buildset_digest(components: dict) -> str:
    parts = [f"{name}:{entry['oci_digest']}" for name, entry in sorted(components.items())]
    return hashlib.sha256("\n".join(parts).encode("utf-8")).hexdigest()


def load_locked_buildset(path: str, required: tuple[str, ...] = REQUIRED) -> dict:
    payload = json.loads(Path(path).read_text(encoding="utf-8"))
    buildset = payload.get("buildset")
    if not buildset:
        raise LockedBuildsetError("promoted mode requires a signed artifacts.lock.json buildset")
    buildset = require_sha256(str(buildset))
    components = payload.get("components") or {}
    missing = [name for name in required if name not in components]
    if missing:
        raise LockedBuildsetError(f"lock missing required components: {missing}")
    locked: dict[str, dict] = {}
    for name, entry in components.items():
        locked[name] = validate_component(name, entry)
    if buildset != buildset_digest(locked):
        raise LockedBuildsetError("buildset digest does not match its component set")
    return {
        "schema": 1,
        "kind": "locked-buildset",
        "buildset": buildset,
        "components": locked,
    }


def write_locked_buildset(lock_path: str, out_path: str) -> dict:
    payload = load_locked_buildset(lock_path)
    Path(out_path).write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
    return payload


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--lock", required=True)
    parser.add_argument("--out", required=True)
    args = parser.parse_args(argv)
    payload = write_locked_buildset(args.lock, args.out)
    print(payload["buildset"])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
