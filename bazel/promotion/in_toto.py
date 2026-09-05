#!/usr/bin/env python3
"""Write unsigned in-toto provenance bound to a matching component digest."""

from __future__ import annotations

import json
import os
from pathlib import Path

from compare import require_sha256


def write_provenance(path: str, component: str, subject_digest: str) -> dict:
    digest = require_sha256(subject_digest)
    if not component:
        raise ValueError("in-toto component name is required")
    if os.environ.get("ORLIX_COSIGN_KEY"):
        raise ValueError("ORLIX_COSIGN_KEY is set; this slice writes unsigned provenance only")
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
    Path(path).write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
    return payload
