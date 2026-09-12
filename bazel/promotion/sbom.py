#!/usr/bin/env python3
"""Write an unsigned SBOM bound to a matching component digest."""

from __future__ import annotations

import json
from pathlib import Path

from compare import require_sha256
from locked_buildset import validate_artifact_identity


def bind_subject(declared: str, actual: str) -> str:
    left = require_sha256(declared)
    right = require_sha256(actual)
    if left != right:
        raise ValueError(f"sbom subject digest mismatch: {left} != {right}")
    return left


def write_sbom(
    path: str,
    component: str,
    subject_digest: str,
    actual_digest: str | None = None,
    artifact_identity: dict | None = None,
) -> dict:
    digest = require_sha256(subject_digest)
    if actual_digest is not None:
        digest = bind_subject(digest, actual_digest)
    if not component:
        raise ValueError("sbom component name is required")
    payload = {
        "schema": 1,
        "kind": "sbom",
        "component": component,
        "subject_digest": digest,
    }
    if artifact_identity is not None:
        identity = validate_artifact_identity(artifact_identity)
        if identity["digest"] != digest:
            raise ValueError("sbom artifact identity differs from subject digest")
        payload["artifact_identity"] = identity
    Path(path).write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
    return payload
