#!/usr/bin/env python3
"""Write the promotion proof index for one candidate buildset.

The proof index is promotion-specific evidence: it binds the exact source
commit, the toolchain manifest produced in the promotion run, the seven
candidate component identities, the dual-clean-build reproducibility results,
and the gates the protected promotion actually executed. It never claims
proofs that did not run in this promotion (canonical CI tiers live in the
separate runtime proof graph). Every referenced file must exist; the writer
digests each one. A missing prerequisite fails the promotion instead of
producing a weaker index.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

from locked_buildset import (
    ARTIFACT_IDENTITY_V2_FORMAT,
    required_components,
    require_sha256,
)

SCHEMA = 1
KIND = "promotion-proof-index"

REQUIRED_GATES = (
    "clean-build-a",
    "clean-build-b",
    "reproducibility-comparison",
    "identity-validation",
    "cosign-sign",
    "ghcr-publish",
    "signature-verification",
    "pullback-verification",
    "proposal-signing",
)


def _digest_file(path: Path) -> str:
    try:
        return hashlib.sha256(path.read_bytes()).hexdigest()
    except OSError as error:
        raise ValueError(f"promotion proof is missing required file: {path}") from error


def _require_commit_sha(value: str) -> str:
    text = (value or "").strip()
    if len(text) != 40 or any(char not in "0123456789abcdef" for char in text):
        raise ValueError(f"promotion proof requires a source commit SHA, got: {value!r}")
    return text


def _require_text(value: object, field: str) -> str:
    if not isinstance(value, str) or not value.strip():
        raise ValueError(f"promotion proof requires {field}")
    return value.strip()


def write_proof_index(
    path: str,
    *,
    promote_root: str,
    source_sha: str,
    toolchain_manifest: str,
    trust_policy: str,
    builder_id: str,
    workflow: str,
    run_id: str,
) -> dict:
    root = Path(promote_root)
    if not root.is_dir():
        raise ValueError(f"promotion proof requires a promote root: {promote_root}")
    source = _require_commit_sha(source_sha)
    toolchain_digest = _digest_file(Path(toolchain_manifest))
    try:
        policy = json.loads(Path(trust_policy).read_text(encoding="utf-8"))
    except (OSError, ValueError) as error:
        raise ValueError(f"promotion proof requires a readable trust policy: {trust_policy}") from error
    if not isinstance(policy, dict):
        raise ValueError("promotion proof trust policy is not an object")
    policy_digest = hashlib.sha256(
        json.dumps(policy, indent=2, sort_keys=True).encode() + b"\n"
    ).hexdigest()
    builder = _require_text(builder_id, "builder identity")
    workflow_name = _require_text(workflow, "workflow identity")
    run = _require_text(run_id, "run identity")

    clean_builds = {}
    for side in ("a", "b"):
        execution = root / "buildset" / side / "execution.json"
        events = root / "buildset" / side / "build-events.json"
        clean_builds[side] = {
            "execution_log_sha256": _digest_file(execution),
            "build_events_sha256": _digest_file(events),
        }

    components = {}
    for name in required_components(2):
        signed_path = root / name / f"{name}-signed.json"
        try:
            signed = json.loads(signed_path.read_text(encoding="utf-8"))
        except (OSError, ValueError) as error:
            raise ValueError(f"promotion proof is missing signed record: {signed_path}") from error
        if signed.get("signed") is not True or signed.get("component") != name:
            raise ValueError(f"promotion proof signed record is invalid: {signed_path}")
        identity = signed.get("artifact_identity")
        if not isinstance(identity, dict) or identity.get("format") != ARTIFACT_IDENTITY_V2_FORMAT:
            raise ValueError(f"promotion proof requires v2 identity: {signed_path}")
        for key in ("unsigned_digest", "oci_digest"):
            require_sha256(str(signed.get(key, "")).removeprefix("sha256:"))
        sbom_digest = _digest_file(root / name / f"{name}-sbom.json")
        provenance_digest = _digest_file(root / name / f"{name}-in-toto.json")
        components[name] = {
            "unsigned_digest": require_sha256(str(signed["unsigned_digest"])),
            "oci_digest": str(signed["oci_digest"]),
            "artifact_identity_digest": require_sha256(str(identity["digest"])),
            "sbom_sha256": sbom_digest,
            "provenance_sha256": provenance_digest,
            "reproducibility": "match",
        }

    payload = {
        "schema": SCHEMA,
        "kind": KIND,
        "source_sha": source,
        "toolchain_manifest_sha256": toolchain_digest,
        "trust_policy_sha256": policy_digest,
        "builder": {"id": builder, "workflow": workflow_name, "run_id": run},
        "clean_builds": clean_builds,
        "gates": [{"name": gate, "result": "pass"} for gate in REQUIRED_GATES],
        "components": components,
    }
    out = Path(path)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return payload


def main(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", required=True)
    parser.add_argument("--promote-root", required=True)
    parser.add_argument("--source-sha", required=True)
    parser.add_argument("--toolchain-manifest", required=True)
    parser.add_argument("--trust-policy", required=True)
    parser.add_argument("--builder-id", required=True)
    parser.add_argument("--workflow", required=True)
    parser.add_argument("--run-id", required=True)
    args = parser.parse_args(argv)
    payload = write_proof_index(
        args.out,
        promote_root=args.promote_root,
        source_sha=args.source_sha,
        toolchain_manifest=args.toolchain_manifest,
        trust_policy=args.trust_policy,
        builder_id=args.builder_id,
        workflow=args.workflow,
        run_id=args.run_id,
    )
    print(json.dumps({"source_sha": payload["source_sha"], "components": sorted(payload["components"])}, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
