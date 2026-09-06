#!/usr/bin/env python3
"""Pull and Cosign-verify every locked component. Fail closed if the lock is unsigned."""

from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import tempfile
from pathlib import Path


class ReconstructError(ValueError):
    pass


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
        raise ReconstructError(f"{argv[0]} failed: {error.stdout or ''}") from error


def reconstruct(lock_path: str, run=_run) -> dict:
    lock = json.loads(Path(lock_path).read_text(encoding="utf-8"))
    buildset = lock.get("buildset")
    components = lock.get("components") or {}
    if not buildset or not components:
        raise ReconstructError("empty lock cannot reconstruct a buildset")
    if shutil.which("oras") is None or shutil.which("cosign") is None:
        raise ReconstructError("oras and cosign are required to reconstruct")
    key = os.environ.get("ORLIX_COSIGN_KEY")
    if not key:
        raise ReconstructError("ORLIX_COSIGN_KEY is required to verify reconstruction")
    key_path = key[len("file://") :] if key.startswith("file://") else key
    pub = os.environ.get("ORLIX_COSIGN_PUB") or f"{key_path}.pub"
    pulled = {}
    with tempfile.TemporaryDirectory(prefix="orlix-reconstruct-") as tmp:
        for name, component in sorted(components.items()):
            reference = component.get("oci_reference")
            digest = component.get("oci_digest")
            if not reference or not digest:
                raise ReconstructError(f"{name} lock entry missing oci_reference or oci_digest")
            dest = Path(tmp) / name
            dest.mkdir()
            pull = ["oras", "pull", reference, "-o", str(dest)]
            if reference.startswith("localhost:") or os.environ.get("ORLIX_ORAS_PLAIN_HTTP") == "1":
                pull.insert(2, "--plain-http")
            run(pull)
            verify = [
                "cosign",
                "verify",
                "--key",
                pub,
                "--insecure-ignore-tlog",
            ]
            if reference.startswith("localhost:") or os.environ.get("ORLIX_ORAS_PLAIN_HTTP") == "1":
                verify.extend(["--allow-http-registry", "--allow-insecure-registry"])
            run(verify + [reference])
            pulled[name] = {"oci_digest": digest, "oci_reference": reference}
    return {"buildset": buildset, "components": pulled}


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--lock", default="artifacts.lock.json")
    args = parser.parse_args(argv)
    payload = reconstruct(args.lock)
    print(payload["buildset"])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
