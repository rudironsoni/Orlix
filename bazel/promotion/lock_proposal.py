#!/usr/bin/env python3
"""Write artifacts.lock.json proposals without mutating the committed lock unless signed."""

from __future__ import annotations

import argparse
import json
import tempfile
from pathlib import Path

from compare import require_sha256
from locked_buildset import (
    LEGACY_IDENTITY_FORMAT,
    SCHEMA1,
    SCHEMA2,
    buildset_digest,
    entry_artifact_digest,
    load_locked_buildset,
    required_components,
    validate_artifact_identity,
)
from publish import verify_component

EMPTY_LOCK = {"schema": 1, "buildset": None, "components": {}}


def read_lock(path: str) -> dict:
    return json.loads(Path(path).read_text(encoding="utf-8"))


def assert_lock_unsigned_empty(path: str) -> dict:
    lock = read_lock(path)
    if lock.get("buildset") is not None or lock.get("components") not in ({}, None):
        raise ValueError(f"committed lock is not empty: {path}")
    return lock


def write_lock_proposal(
    path: str,
    component: str,
    unsigned_digest: str,
    artifact_identity: dict | None = None,
) -> dict:
    digest = require_sha256(unsigned_digest)
    if not component:
        raise ValueError("lock proposal component name is required")
    if artifact_identity is None:
        component_entry = {"unsigned_digest": digest}
        schema = SCHEMA1
    else:
        identity = validate_artifact_identity(artifact_identity)
        if identity["digest"] != digest:
            raise ValueError("artifact identity digest differs from lock proposal digest")
        component_entry = {
            "unsigned_digest": digest,
            "artifact_identity": identity,
        }
        schema = SCHEMA2
    payload = {
        "schema": schema,
        "kind": "lock-proposal",
        "signed": False,
        "buildset": None,
        "oci_digest": None,
        "components": {component: component_entry},
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
        entry = payload["components"][name]
        schema = payload.get("schema", SCHEMA1)
        if schema == SCHEMA1:
            unsigned = require_sha256(entry["unsigned_digest"])
            want = (components.get(name) or {}).get("unsigned_digest")
            if want != unsigned:
                raise ValueError(f"{name} unsigned digest {unsigned} does not match lock {want}")
        elif schema == SCHEMA2:
            identity = validate_artifact_identity(entry.get("artifact_identity"))
            if lock.get("schema", SCHEMA1) == SCHEMA1:
                if identity["format"] != LEGACY_IDENTITY_FORMAT:
                    raise ValueError("schema-1 lock can match only an explicit legacy identity")
                want = (components.get(name) or {}).get("unsigned_digest")
                if identity["digest"] != want:
                    raise ValueError(f"{name} artifact digest does not match lock")
            else:
                locked_identity = validate_artifact_identity(
                    (components.get(name) or {}).get("artifact_identity")
                )
                if identity != locked_identity:
                    raise ValueError(f"{name} artifact identity does not match lock")
                if entry_artifact_digest(entry) != entry_artifact_digest(components[name]):
                    raise ValueError(f"{name} artifact digest does not match lock")
        else:
            raise ValueError(f"unsupported lock proposal schema: {schema}")
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
    schema = proposal.get("schema", SCHEMA1)
    if schema not in (SCHEMA1, SCHEMA2):
        raise ValueError(f"unsupported lock proposal schema: {schema}")
    raw_components = proposal.get("components") or {}
    if not isinstance(raw_components, dict) or not raw_components:
        raise ValueError("signed lock proposal requires components")
    components = load_locked_buildset(proposal_path)["components"]
    for name, entry in components.items():
        verify_component(
            {**entry, "component": name, "signed": True, "schema": schema}
        )
    lock = {
        "schema": schema,
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
    payloads: list[dict] = []
    for signed_path in signed_paths:
        payload = json.loads(Path(signed_path).read_text(encoding="utf-8"))
        if payload.get("signed") is not True:
            raise ValueError(f"unsigned component cannot enter a signed lock: {signed_path}")
        name = payload.get("component")
        if not name:
            raise ValueError(f"signed component missing name: {signed_path}")
        if name in components:
            raise ValueError(f"duplicate component: {name}")
        payloads.append(payload)
        components[name] = verify_component(payload)
    schemas = {payload.get("schema", SCHEMA1) for payload in payloads}
    if not schemas.issubset({SCHEMA1, SCHEMA2}):
        raise ValueError(f"unsupported signed component schema: {schemas}")
    schema = SCHEMA2 if SCHEMA2 in schemas else SCHEMA1
    if schema == SCHEMA2:
        for payload in payloads:
            if payload.get("schema", SCHEMA1) == SCHEMA1:
                identity = payload.get("artifact_identity")
                if not isinstance(identity, dict) or identity.get("format") != "legacy-marker-sha256":
                    raise ValueError(
                        "schema-2 lock requires explicit legacy artifact identities"
                    )
                name = payload["component"]
                components[name] = verify_component(
                    {**payload, "schema": SCHEMA2}
                )
    missing = [name for name in required_components(schema) if name not in components]
    if missing:
        raise ValueError(f"signed lock proposal missing required components: {missing}")
    buildset = buildset_digest(components, schema=schema)
    proposal = {
        "schema": schema,
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
