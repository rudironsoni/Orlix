#!/usr/bin/env python3
"""Fail closed unless artifacts.lock.json names a digest-pinned signed buildset."""

from __future__ import annotations

import argparse
import json
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
    for name in required:
        entry = components[name]
        unsigned = require_sha256(entry["unsigned_digest"])
        oci = entry.get("oci_digest")
        if not isinstance(oci, str) or not oci.startswith(OCI_PREFIX):
            raise LockedBuildsetError(f"{name} lock entry missing oci_digest")
        require_sha256(oci[len(OCI_PREFIX) :])
        reference = entry.get("oci_reference") or ""
        if not reference:
            raise LockedBuildsetError(f"{name} lock entry missing oci_reference")
        if ":latest" in reference.split("@", 1)[0] or reference.endswith(":latest"):
            raise LockedBuildsetError(f"{name} oci_reference must not use mutable latest: {reference}")
        if f"@{oci}" not in reference:
            raise LockedBuildsetError(f"{name} oci_reference is not pinned to {oci}")
        locked[name] = {
            "unsigned_digest": unsigned,
            "oci_digest": oci,
            "oci_reference": reference,
        }
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
