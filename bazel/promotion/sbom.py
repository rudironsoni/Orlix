#!/usr/bin/env python3
"""Write a deterministic CycloneDX 1.6 SBOM bound to a matching component digest.

The SBOM describes only software contained in or represented by the promoted
product: the root subject is the promoted component, and every listed file
comes from the component's artifact-identity-v2 manifest entries. Build,
toolchain, trust, and workflow facts belong in in-toto/SLSA provenance, not
here. No package versions or dependencies are invented: file components carry
content hashes, and the root version is the unsigned content digest.

The document is fully deterministic for identical inputs: no timestamps, no
random serials. The serialNumber derives from the component artifact digest.
"""

from __future__ import annotations

import json
import uuid
from pathlib import Path

from locked_buildset import require_sha256, validate_artifact_identity

SPEC_VERSION = "1.6"

ROOT_COMPONENT_TYPES = {
    "uapi": "library",
    "mlibc": "library",
    "rootfs": "application",
    "kernel-release-iphoneos": "library",
    "kernel-release-iphonesimulator": "library",
    "kernel-development-iphoneos": "library",
    "kernel-development-iphonesimulator": "library",
}


def bind_subject(declared: str, actual: str) -> str:
    left = require_sha256(declared)
    right = require_sha256(actual)
    if left != right:
        raise ValueError(f"sbom subject digest mismatch: {left} != {right}")
    return left


def component_serial(component: str, digest: str) -> str:
    return f"urn:uuid:{uuid.uuid5(uuid.NAMESPACE_URL, f'orlix:component:{component}:{digest}')}"


def write_sbom(
    path: str,
    component: str,
    subject_digest: str,
    actual_digest: str | None = None,
    artifact_identity: dict | None = None,
    manifest_entries: list | None = None,
) -> dict:
    digest = require_sha256(subject_digest)
    if actual_digest is not None:
        digest = bind_subject(digest, actual_digest)
    if not component:
        raise ValueError("sbom component name is required")
    if artifact_identity is not None:
        identity = validate_artifact_identity(artifact_identity)
        if identity["digest"] != digest:
            raise ValueError("sbom artifact identity differs from subject digest")
    if component not in ROOT_COMPONENT_TYPES:
        raise ValueError(f"sbom component is not a promoted component: {component!r}")
    entries = manifest_entries if manifest_entries is not None else []
    if not isinstance(entries, list):
        raise ValueError("sbom manifest entries must be a list")
    files = []
    for entry in entries:
        if not isinstance(entry, dict) or entry.get("type") != "file":
            raise ValueError(f"sbom manifest entry is not a file: {entry!r}")
        for field in ("path", "content_sha256", "mode"):
            if field not in entry:
                raise ValueError(f"sbom manifest entry is missing {field}: {entry!r}")
        if not isinstance(entry["path"], str) or not entry["path"]:
            raise ValueError(f"sbom manifest entry has an invalid path: {entry!r}")
        if not isinstance(entry["mode"], int) or isinstance(entry["mode"], bool):
            raise ValueError(f"sbom manifest entry has an invalid mode: {entry!r}")
        files.append(
            {
                "type": "file",
                "name": entry["path"],
                "hashes": [{"alg": "SHA-256", "content": require_sha256(entry["content_sha256"])}],
                "properties": [{"name": "orlix:mode", "value": oct(entry["mode"])}],
            }
        )
    files.sort(key=lambda item: item["name"])
    payload = {
        "bomFormat": "CycloneDX",
        "specVersion": SPEC_VERSION,
        "serialNumber": component_serial(component, digest),
        "version": 1,
        "metadata": {
            "component": {
                "bom-ref": component,
                "type": ROOT_COMPONENT_TYPES[component],
                "name": component,
                "version": digest,
                "hashes": [{"alg": "SHA-256", "content": digest}],
            }
        },
        "components": files,
    }
    Path(path).write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return payload
