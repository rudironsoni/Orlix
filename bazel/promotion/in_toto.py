#!/usr/bin/env python3
"""Write in-toto provenance with a SLSA v1 predicate for a promoted component.

The predicate binds the promoted subject to facts available inside the
promotion run: the source commit, the builder and workflow identity, the
invocation configuration, the resolved dual-build materials, the toolchain
identity, and the dual-build comparison result. No field is invented: every
value arrives as an explicit argument, and the writer rejects empty or
malformed inputs instead of defaulting them.
"""

from __future__ import annotations

import json
from pathlib import Path

from locked_buildset import require_sha256, validate_artifact_identity

STATEMENT_TYPE = "https://in-toto.io/Statement/v1"
SLSA_PREDICATE_TYPE = "https://slsa.dev/provenance/v1"
BUILD_TYPE = "https://orlix.build/promotion/dual-clean/v1"


def _require_text(value: object, field: str) -> str:
    if not isinstance(value, str) or not value.strip():
        raise ValueError(f"in-toto provenance requires {field}")
    return value.strip()


def write_provenance(
    path: str,
    component: str,
    subject_digest: str,
    artifact_identity: dict | None = None,
    *,
    source_sha: str | None = None,
    builder_id: str | None = None,
    invocation_id: str | None = None,
    build_config: list | None = None,
    toolchain_digest: str | None = None,
    materials: list | None = None,
) -> dict:
    digest = require_sha256(subject_digest)
    if not component:
        raise ValueError("in-toto component name is required")
    if artifact_identity is not None:
        identity = validate_artifact_identity(artifact_identity)
        if identity["digest"] != digest:
            raise ValueError("in-toto artifact identity differs from subject digest")
    source = _require_text(source_sha, "source SHA")
    if len(source) != 40 or any(char not in "0123456789abcdef" for char in source):
        raise ValueError(f"in-toto source SHA is not a commit SHA: {source!r}")
    builder = _require_text(builder_id, "builder identity")
    invocation = _require_text(invocation_id, "invocation identity")
    if toolchain_digest is None:
        raise ValueError("in-toto provenance requires a toolchain digest")
    toolchain = require_sha256(toolchain_digest)
    if not isinstance(build_config, list) or not build_config or not all(
        isinstance(item, str) and item.strip() for item in build_config
    ):
        raise ValueError("in-toto provenance requires a non-empty build configuration list")
    if not isinstance(materials, list) or not materials:
        raise ValueError("in-toto provenance requires resolved materials")
    resolved = []
    for material in materials:
        if not isinstance(material, dict):
            raise ValueError(f"in-toto material is not an object: {material!r}")
        uri = _require_text(material.get("uri"), "material uri")
        material_digest = material.get("digest")
        if not isinstance(material_digest, dict) or set(material_digest) != {"sha256"}:
            raise ValueError(f"in-toto material digest must be exactly sha256: {material!r}")
        require_sha256(material_digest["sha256"])
        resolved.append({"uri": uri, "digest": {"sha256": material_digest["sha256"]}})
    payload = {
        "schema": 1,
        "kind": "in-toto",
        "_type": STATEMENT_TYPE,
        "subject": [{"name": component, "digest": {"sha256": digest}}],
        "predicateType": SLSA_PREDICATE_TYPE,
        "predicate": {
            "buildDefinition": {
                "buildType": BUILD_TYPE,
                "externalParameters": {
                    "component": component,
                    "config": sorted(build_config),
                    "source": source,
                },
                "internalParameters": {},
                "resolvedDependencies": resolved,
            },
            "runDetails": {
                "builder": {"id": builder},
                "metadata": {"invocationId": invocation},
                "byproducts": [],
            },
        },
        "toolchain_digest": toolchain,
        "signed": False,
        "oci_digest": None,
    }
    Path(path).write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return payload
