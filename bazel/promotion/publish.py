#!/usr/bin/env python3
"""Publish a signed component to GHCR. Unsigned proposals must not publish."""

from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import tempfile
from pathlib import Path


class PublishError(ValueError):
    pass


def require_signed_proposal(path: str) -> dict:
    payload = json.loads(Path(path).read_text(encoding="utf-8"))
    if payload.get("signed") is not True:
        raise PublishError("unsigned proposal must not publish to GHCR")
    if not payload.get("oci_digest"):
        raise PublishError("proposal missing oci_digest must not publish to GHCR")
    if not payload.get("oci_reference"):
        raise PublishError("proposal missing oci_reference must not publish to GHCR")
    return payload


def _run(argv: list[str]) -> subprocess.CompletedProcess[str]:
    try:
        return subprocess.run(
            argv,
            check=True,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
        )
    except subprocess.CalledProcessError as error:
        output = error.stdout or ""
        raise PublishError(f"{argv[0]} failed: {output}") from error


def publish(proposal_path: str, run=_run) -> dict:
    payload = require_signed_proposal(proposal_path)
    if shutil.which("oras") is None:
        raise PublishError("oras is required to pull a published component")
    if shutil.which("cosign") is None:
        raise PublishError("cosign is required to verify a published component")
    key = os.environ.get("ORLIX_COSIGN_KEY")
    if not key:
        raise PublishError("ORLIX_COSIGN_KEY is required to verify; refusing unsigned pull")
    key_path = key[len("file://") :] if key.startswith("file://") else key
    pub = os.environ.get("ORLIX_COSIGN_PUB") or f"{key_path}.pub"
    image = payload["oci_reference"]
    with tempfile.TemporaryDirectory(prefix="orlix-publish-") as tmp:
        pull = ["oras", "pull", image, "-o", tmp]
        if image.startswith("localhost:") or os.environ.get("ORLIX_ORAS_PLAIN_HTTP") == "1":
            pull.insert(2, "--plain-http")
        run(pull)
        verify = [
            "cosign",
            "verify",
            "--key",
            pub,
            "--insecure-ignore-tlog",
        ]
        if image.startswith("localhost:") or os.environ.get("ORLIX_ORAS_PLAIN_HTTP") == "1":
            verify.extend(["--allow-http-registry", "--allow-insecure-registry"])
        run(verify + [image])
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
