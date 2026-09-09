#!/usr/bin/env python3
"""Write artifacts.lock.json proposals without mutating the committed lock unless signed."""

from __future__ import annotations

import argparse
import json
import tempfile
from pathlib import Path

from compare import require_sha256
from locked_buildset import buildset_digest, load_locked_buildset
from publish import verify_component

EMPTY_LOCK = {"schema": 1, "buildset": None, "components": {}}


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
    components = load_locked_buildset(proposal_path)["components"]
    for name, entry in components.items():
        verify_component({**entry, "component": name, "signed": True})
    lock = {
        "schema": 1,
        "buildset": proposal["buildset"],
        "components": components,
    }
    destination = Path(lock_path)
    with tempfile.NamedTemporaryFile(mode="w", dir=destination.parent, delete=False) as output:
        pending = Path(output.name)
        try:
            output.write(json.dumps(lock, indent=2) + "\n")
            output.flush()
            pending.replace(destination)
        finally:
            pending.unlink(missing_ok=True)


def write_signed_lock_proposal(path: str, signed_paths: list[str]) -> dict:
    if not signed_paths:
        raise ValueError("signed lock proposal requires at least one signed component")
    components: dict[str, dict] = {}
    for signed_path in signed_paths:
        payload = json.loads(Path(signed_path).read_text(encoding="utf-8"))
        if payload.get("signed") is not True:
            raise ValueError(f"unsigned component cannot enter a signed lock: {signed_path}")
        name = payload.get("component")
        if not name:
            raise ValueError(f"signed component missing name: {signed_path}")
        if name in components:
            raise ValueError(f"duplicate component: {name}")
        components[name] = verify_component(payload)
    buildset = buildset_digest(components)
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
