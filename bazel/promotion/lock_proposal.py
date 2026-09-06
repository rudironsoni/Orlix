#!/usr/bin/env python3
"""Write artifacts.lock.json proposals without mutating the committed lock unless signed."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

from compare import require_sha256

EMPTY_LOCK = {"schema": 1, "buildset": None, "components": {}}
OCI_PREFIX = "sha256:"


def read_lock(path: str) -> dict:
    return json.loads(Path(path).read_text(encoding="utf-8"))


def assert_lock_unsigned_empty(path: str) -> dict:
    lock = read_lock(path)
    if lock.get("buildset") is not None or lock.get("components") not in ({}, None):
        raise ValueError(f"committed lock is not empty: {path}")
    return lock


def write_lock_proposal(path: str, component: str, unsigned_digest: str) -> dict:
    digest = require_sha256(unsigned_digest)
    if not component:
        raise ValueError("lock proposal component name is required")
    payload = {
        "schema": 1,
        "kind": "lock-proposal",
        "signed": False,
        "buildset": None,
        "oci_digest": None,
        "components": {component: {"unsigned_digest": digest}},
    }
    Path(path).write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
    return payload


def assert_unsigned_lock_proposals(paths: list[str], lock_path: str) -> None:
    lock = read_lock(lock_path)
    components = lock.get("components") or {}
    before = Path(lock_path).read_text(encoding="utf-8")
    for path in paths:
        payload = json.loads(Path(path).read_text(encoding="utf-8"))
        if payload.get("signed") is not False:
            raise ValueError(f"unsigned lock proposal must set signed false: {path}")
        if payload.get("buildset") is not None or payload.get("oci_digest") is not None:
            raise ValueError(f"unsigned lock proposal must not carry a buildset or oci digest: {path}")
        names = list((payload.get("components") or {}).keys())
        if len(names) != 1:
            raise ValueError(f"unsigned lock proposal must name one component: {path}")
        name = names[0]
        unsigned = require_sha256(payload["components"][name]["unsigned_digest"])
        want = (components.get(name) or {}).get("unsigned_digest")
        if want != unsigned:
            raise ValueError(f"{name} unsigned digest {unsigned} does not match lock {want}")
        try:
            apply_lock_proposal(path, lock_path)
        except ValueError:
            pass
        else:
            raise ValueError(f"unsigned lock proposal mutated {lock_path}")
        if Path(lock_path).read_text(encoding="utf-8") != before:
            raise ValueError(f"unsigned lock proposal mutated {lock_path}")


def apply_lock_proposal(proposal_path: str, lock_path: str) -> None:
    proposal = json.loads(Path(proposal_path).read_text(encoding="utf-8"))
    if proposal.get("signed") is not True or not proposal.get("buildset"):
        raise ValueError("unsigned lock proposal must not mutate artifacts.lock.json")
    lock = {
        "schema": 1,
        "buildset": proposal["buildset"],
        "components": proposal["components"],
    }
    Path(lock_path).write_text(json.dumps(lock, indent=2) + "\n", encoding="utf-8")


def _require_oci_digest(value: str) -> str:
    if not isinstance(value, str) or not value.startswith(OCI_PREFIX):
        raise ValueError(f"invalid oci digest: {value!r}")
    require_sha256(value[len(OCI_PREFIX) :])
    return value


def write_signed_lock_proposal(path: str, signed_paths: list[str]) -> dict:
    if not signed_paths:
        raise ValueError("signed lock proposal requires at least one signed component")
    components: dict[str, dict] = {}
    parts: list[str] = []
    for signed_path in signed_paths:
        payload = json.loads(Path(signed_path).read_text(encoding="utf-8"))
        if payload.get("signed") is not True:
            raise ValueError(f"unsigned component cannot enter a signed lock: {signed_path}")
        name = payload.get("component")
        if not name:
            raise ValueError(f"signed component missing name: {signed_path}")
        unsigned = require_sha256(payload["unsigned_digest"])
        oci = _require_oci_digest(payload["oci_digest"])
        reference = payload.get("oci_reference")
        if not reference:
            raise ValueError(f"signed component missing oci_reference: {signed_path}")
        components[name] = {
            "unsigned_digest": unsigned,
            "oci_digest": oci,
            "oci_reference": reference,
        }
        parts.append(f"{name}:{oci}")
    buildset = hashlib.sha256("\n".join(sorted(parts)).encode("utf-8")).hexdigest()
    proposal = {
        "schema": 1,
        "kind": "lock-proposal",
        "signed": True,
        "buildset": buildset,
        "components": components,
    }
    Path(path).write_text(json.dumps(proposal, indent=2) + "\n", encoding="utf-8")
    return proposal


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", required=True)
    parser.add_argument("--signed", action="append", default=[])
    parser.add_argument("--apply-lock")
    args = parser.parse_args(argv)
    proposal = write_signed_lock_proposal(args.out, args.signed)
    if args.apply_lock:
        apply_lock_proposal(args.out, args.apply_lock)
    print(proposal["buildset"])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
