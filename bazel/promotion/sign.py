#!/usr/bin/env python3
"""Cosign-sign a matching unsigned digest. Never invent a signature."""

from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import subprocess
import tarfile
import tempfile
from pathlib import Path

from compare import require_sha256
from locked_buildset import GHCR_REPOSITORY, require_component_name

DIGEST_RE = re.compile(r"Digest:\s*(sha256:[0-9a-f]{64})")


class SignError(ValueError):
    pass


def require_signing_env() -> str:
    key = os.environ.get("ORLIX_COSIGN_KEY")
    if not key:
        raise SignError("ORLIX_COSIGN_KEY is required to sign; refusing to invent a signature")
    if shutil.which("cosign") is None:
        raise SignError("cosign is required to sign; refusing to invent a signature")
    return key


def _cosign_key_path(key: str) -> str:
    if key.startswith("file://"):
        return key[len("file://") :]
    return key


def cosign_env(base: dict[str, str] | None = None) -> dict[str, str]:
    env = dict(os.environ if base is None else base)
    password = env.get("ORLIX_COSIGN_KEY_PASSWORD")
    if password:
        env["COSIGN_PASSWORD"] = password
    return env


def oras_config_args() -> list[str]:
    path = os.environ.get("ORLIX_ORAS_REGISTRY_CONFIG")
    if path:
        return ["--registry-config", path]
    return []


def attach_registry_config(env: dict[str, str], tmp: Path) -> dict[str, str]:
    path = os.environ.get("ORLIX_ORAS_REGISTRY_CONFIG")
    if not path:
        return env
    dest = tmp / "docker-config"
    dest.mkdir(exist_ok=True)
    shutil.copy(path, dest / "config.json")
    attached = dict(env)
    attached["DOCKER_CONFIG"] = str(dest)
    return attached


def _parse_oras_digest(stdout: str) -> str:
    match = DIGEST_RE.search(stdout)
    if not match:
        raise SignError("oras push did not print an OCI digest; refusing to invent oci_digest")
    return match.group(1)


def _run(
    argv: list[str],
    env: dict[str, str] | None = None,
    cwd: str | None = None,
) -> subprocess.CompletedProcess[str]:
    try:
        return subprocess.run(
            argv,
            check=True,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            env=env,
            cwd=cwd,
        )
    except subprocess.CalledProcessError as error:
        output = error.stdout or ""
        raise SignError(f"{argv[0]} failed: {output}") from error


def sign_digest(
    digest: str,
    component: str,
    artifact: str | None = None,
    repository: str | None = None,
    run=_run,
) -> dict:
    digest = require_sha256(digest)
    if not component:
        raise SignError("sign component name is required")
    require_component_name(component)
    key = require_signing_env()
    artifact_path = artifact or os.environ.get("ORLIX_PROMOTE_ARTIFACT")
    if not artifact_path:
        raise SignError("artifact path is required to publish; refusing to invent oci_digest")
    if shutil.which("oras") is None:
        raise SignError("oras is required to publish; refusing to invent oci_digest")
    repo = repository or os.environ.get("ORLIX_GHCR_REPOSITORY", GHCR_REPOSITORY)
    if repo != GHCR_REPOSITORY:
        raise SignError(f"promotion requires {GHCR_REPOSITORY}")
    source = Path(artifact_path)
    if not source.exists():
        raise SignError(f"missing promote artifact: {artifact_path}")
    image = f"{repo}/{component}:{digest}"
    with tempfile.TemporaryDirectory(prefix="orlix-sign-") as tmp:
        blob = Path(tmp) / "component.tar"
        with tarfile.open(blob, "w") as archive:
            if source.is_dir():
                archive.add(source, arcname=".")
            else:
                archive.add(source, arcname=source.name)
        oras_push = ["oras", "push"]
        oras_push.extend(oras_config_args())
        oras_push.extend(
            [
                "--annotation",
                "org.opencontainers.image.source=https://github.com/rudironsoni/Orlix",
            ]
        )
        pushed = run(
            oras_push
            + [
                image,
                "component.tar:application/vnd.orlix.component.v1.tar",
            ],
            cwd=tmp,
        )
        oci_digest = _parse_oras_digest(pushed.stdout)
        signed_ref = f"{repo}/{component}@{oci_digest}"
        key_path = _cosign_key_path(key)
        env = attach_registry_config(cosign_env(), Path(tmp))
        cosign_sign = [
            "cosign",
            "sign",
            "--key",
            key_path,
            "--yes",
            "--use-signing-config=false",
            "--tlog-upload=false",
        ]
        run(cosign_sign + [signed_ref], env=env)
    return {
        "schema": 1,
        "component": component,
        "unsigned_digest": digest,
        "signed": True,
        "oci_digest": oci_digest,
        "oci_reference": signed_ref,
    }


def write_signed_proposal(
    path: str,
    component: str,
    digest: str,
    artifact: str | None = None,
    repository: str | None = None,
) -> dict:
    payload = sign_digest(digest, component, artifact=artifact, repository=repository)
    Path(path).write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
    return payload


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--component", required=True)
    parser.add_argument("--digest", required=True)
    parser.add_argument("--proposal", required=True)
    parser.add_argument("--artifact")
    parser.add_argument("--repository")
    args = parser.parse_args(argv)
    write_signed_proposal(
        args.proposal,
        args.component,
        args.digest,
        artifact=args.artifact,
        repository=args.repository,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
