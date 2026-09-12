#!/usr/bin/env python3
"""Write unsigned in-toto provenance bound to a matching component digest."""

from __future__ import annotations

import json
from pathlib import Path

from compare import require_sha256
from locked_buildset import validate_artifact_identity


def write_provenance(
    path: str,
    component: str,
    subject_digest: str,
    artifact_identity: dict | None = None,
) -> dict:
    digest = require_sha256(subject_digest)
    if not component:
        raise ValueError("in-toto component name is required")
    payload = {
        "schema": 1,
        "kind": "in-toto",
        "_type": "https://in-toto.io/Statement/v1",
        "subject": [{"name": component, "digest": {"sha256": digest}}],
        "predicateType": "https://in-toto.io/attestation/release/v0.1",
        "predicate": {"component": component},
        "signed": False,
        "oci_digest": None,
    }
    if artifact_identity is not None:
        identity = validate_artifact_identity(artifact_identity)
        if identity["digest"] != digest:
            raise ValueError("in-toto artifact identity differs from subject digest")
        payload["artifact_identity"] = identity
    Path(path).write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
    return payload
