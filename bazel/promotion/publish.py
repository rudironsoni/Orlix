#!/usr/bin/env python3
"""Publish a signed component to GHCR. Unsigned proposals must not publish."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import subprocess
import tempfile
from pathlib import Path

from sign import attach_registry_config, cosign_env
from locked_buildset import LockedBuildsetError, validate_component


class PublishError(ValueError):
    pass


VERIFICATION_POLICY_VERSION = 1


def require_signed_proposal(path: str) -> dict:
    try:
        payload = json.loads(Path(path).read_text(encoding="utf-8"))
    except (OSError, ValueError) as error:
        raise PublishError(f"invalid signed proposal: {path}") from error
    if not isinstance(payload, dict):
        raise PublishError("signed proposal must be a JSON object")
    if payload.get("signed") is not True:
        raise PublishError("unsigned proposal must not publish to GHCR")
    if not payload.get("oci_digest"):
        raise PublishError("proposal missing oci_digest must not publish to GHCR")
    if not payload.get("oci_reference"):
        raise PublishError("proposal missing oci_reference must not publish to GHCR")
    try:
        validate_component(
            payload["component"],
            payload,
            schema=payload.get("schema", 1),
        )
    except (KeyError, LockedBuildsetError, TypeError, ValueError) as error:
        raise PublishError(str(error)) from error
    return payload


def _run(argv: list[str], env: dict[str, str] | None = None) -> subprocess.CompletedProcess[str]:
    try:
        return subprocess.run(
            argv,
            check=True,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            env=env,
        )
    except subprocess.CalledProcessError as error:
        output = error.stdout or ""
        raise PublishError(f"{argv[0]} failed: {output}") from error


def trusted_public_key() -> str:
    key_path = os.environ.get("ORLIX_COSIGN_KEY", "").removeprefix("file://")
    public = os.environ.get("ORLIX_COSIGN_PUB") or (f"{key_path}.pub" if key_path else "")
    if not public:
        raise PublishError("ORLIX_COSIGN_PUB is required to verify signatures")
    path = Path(public.removeprefix("file://"))
    fingerprint = hashlib.sha256(path.read_bytes()).hexdigest()
    policy = json.loads(Path(__file__).with_name("trust-policy.json").read_text())
    if fingerprint not in policy["accepted_key_ids"]:
        raise PublishError("public key is not accepted by trust-policy.json")
    return str(path)


def verification_context() -> dict:
    public = Path(trusted_public_key())
    policy_path = Path(__file__).with_name("trust-policy.json")
    policy_bytes = policy_path.read_bytes()
    return {
        "signing_key_fingerprint": hashlib.sha256(public.read_bytes()).hexdigest(),
        "trust_policy_sha256": hashlib.sha256(policy_bytes).hexdigest(),
        "verification_policy_version": VERIFICATION_POLICY_VERSION,
    }


def verify_component(payload: dict, run=_run) -> dict:
    if payload.get("signed") is not True:
        raise PublishError("unsigned component cannot authorize promotion")
    try:
        entry = validate_component(
            payload["component"],
            payload,
            schema=payload.get("schema", 1),
        )
    except (KeyError, LockedBuildsetError, TypeError, ValueError) as error:
        raise PublishError(str(error)) from error
    if shutil.which("cosign") is None:
        raise PublishError("cosign is required to verify a published component")
    public = trusted_public_key()
    with tempfile.TemporaryDirectory(prefix="orlix-verify-") as tmp:
        run(
            ["cosign", "verify", "--key", public, "--insecure-ignore-tlog", entry["oci_reference"]],
            env=attach_registry_config(cosign_env(), Path(tmp)),
        )
    return entry


def publish(proposal_path: str, run=_run) -> dict:
    payload = require_signed_proposal(proposal_path)
    if shutil.which("oras") is None:
        raise PublishError("oras is required to pull a published component")
    verify_component(payload, run=run)
    image = payload["oci_reference"]
    with tempfile.TemporaryDirectory(prefix="orlix-publish-") as tmp:
        pull = ["oras", "pull", image, "-o", tmp]
        config = os.environ.get("ORLIX_ORAS_REGISTRY_CONFIG")
        if config:
            pull[2:2] = ["--registry-config", config]
        run(pull)
        identity = payload.get("artifact_identity")
        if isinstance(identity, dict) and identity.get("format") == "artifact-identity-v2":
            blob = Path(tmp) / "component.tar"
            if not blob.is_file():
                raise PublishError("v2 component pull did not write component.tar")
            from reconstruct import _extract_component_tar
            from locked_buildset import validate_v2_product

            tree = Path(tmp) / "component"
            _extract_component_tar(blob, tree)
            try:
                validate_v2_product(tree, identity, payload["component"])
            except (OSError, TypeError, ValueError) as error:
                raise PublishError(str(error)) from error
    return payload


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--proposal", required=True)
    args = parser.parse_args(argv)
    payload = publish(args.proposal)
    print(payload["oci_digest"])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
