#!/usr/bin/env python3
"""Write an unsigned artifacts.lock.json proposal without mutating the committed lock."""

from __future__ import annotations

import json
from pathlib import Path

from compare import require_sha256

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


def apply_lock_proposal(proposal_path: str, lock_path: str) -> None:
    proposal = json.loads(Path(proposal_path).read_text(encoding="utf-8"))
    if proposal.get("signed") is not True or not proposal.get("buildset"):
        raise ValueError("unsigned lock proposal must not mutate artifacts.lock.json")
    Path(lock_path).write_text(json.dumps(proposal, indent=2) + "\n", encoding="utf-8")
