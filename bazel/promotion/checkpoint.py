#!/usr/bin/env python3
"""Resumable promotion checkpoint for the dual clean build.

The dual clean builds plus staging produce, per component, an
artifact-identity-v2 digest and two independently staged product trees whose
contents were verified identical. That result set is the expensive proof.
A later-stage failure (proposal, signing, publication) that does not change
the source, toolchain, or component set must not force those builds again.

A checkpoint binds that proof to the exact source SHA, toolchain digest, and
component set, and records each component's artifact-identity-v2 digest. It is
never trusted on the manifest alone: every reused stage is still passed through
compare.py, which re-validates both trees against their recorded digests before
any evidence is emitted.
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

CHECKPOINT_FILENAME = "buildset-checkpoint.json"
SIDES = ("a", "b")


class CheckpointError(ValueError):
    pass


def checkpoint_path(promote_root: str | Path) -> Path:
    return Path(promote_root) / CHECKPOINT_FILENAME


def _digest_file(promote_root: Path, component: str, side: str) -> Path:
    return promote_root / component / side / "digest.sha256"


def _staged_tree(promote_root: Path, component: str, side: str) -> Path:
    return promote_root / component / side / "staged"


def _read_digest(path: Path) -> str:
    try:
        value = path.read_text(encoding="ascii").strip()
    except OSError as error:
        raise CheckpointError(f"missing component digest: {path}") from error
    if len(value) != 64 or any(char not in "0123456789abcdef" for char in value):
        raise CheckpointError(f"invalid component digest: {path}")
    return value


def write_checkpoint(
    promote_root: str | Path,
    source_sha: str,
    toolchain_sha256: str,
    components: list[str],
) -> dict:
    root = Path(promote_root)
    if len(source_sha) != 40 or any(char not in "0123456789abcdef" for char in source_sha):
        raise CheckpointError(f"invalid source SHA: {source_sha!r}")
    if len(toolchain_sha256) != 64:
        raise CheckpointError(f"invalid toolchain digest: {toolchain_sha256!r}")
    recorded: dict[str, dict] = {}
    for component in sorted(components):
        digests = [_read_digest(_digest_file(root, component, side)) for side in SIDES]
        if digests[0] != digests[1]:
            raise CheckpointError(f"component digests differ between sides: {component}")
        for side in SIDES:
            tree = _staged_tree(root, component, side)
            if not (tree / "artifact-identity-v2.json").is_file():
                raise CheckpointError(f"staged identity is missing: {tree}")
        recorded[component] = {"digest": digests[0]}
    payload = {
        "schema": 1,
        "kind": "promotion-checkpoint",
        "source_sha": source_sha,
        "toolchain_sha256": toolchain_sha256,
        "components": recorded,
    }
    destination = checkpoint_path(root)
    destination.parent.mkdir(parents=True, exist_ok=True)
    staging = destination.with_name(f"{destination.name}.tmp")
    staging.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    staging.replace(destination)
    return payload


def validate_checkpoint(
    promote_root: str | Path,
    source_sha: str,
    toolchain_sha256: str,
    components: list[str],
) -> dict:
    root = Path(promote_root)
    path = checkpoint_path(root)
    try:
        payload = json.loads(path.read_text(encoding="utf-8"))
    except OSError as error:
        raise CheckpointError(f"no promotion checkpoint: {path}") from error
    except ValueError as error:
        raise CheckpointError(f"unreadable promotion checkpoint: {path}") from error
    if not isinstance(payload, dict) or payload.get("kind") != "promotion-checkpoint":
        raise CheckpointError("promotion checkpoint has the wrong kind")
    if payload.get("source_sha") != source_sha:
        raise CheckpointError("promotion checkpoint names a different source")
    if payload.get("toolchain_sha256") != toolchain_sha256:
        raise CheckpointError("promotion checkpoint names a different toolchain")
    recorded = payload.get("components")
    if not isinstance(recorded, dict) or set(recorded) != set(components):
        raise CheckpointError("promotion checkpoint names a different component set")
    for component in sorted(components):
        entry = recorded.get(component)
        if not isinstance(entry, dict) or not isinstance(entry.get("digest"), str):
            raise CheckpointError(f"promotion checkpoint entry is invalid: {component}")
        for side in SIDES:
            actual = _read_digest(_digest_file(root, component, side))
            if actual != entry["digest"]:
                raise CheckpointError(f"staged digest differs from checkpoint: {component} side {side}")
            tree = _staged_tree(root, component, side)
            if not (tree / "artifact-identity-v2.json").is_file():
                raise CheckpointError(f"staged identity is missing: {tree}")
    return payload


def main(argv=None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--promote-root", required=True)
    parser.add_argument("--source-sha", required=True)
    parser.add_argument("--toolchain-sha256", required=True)
    parser.add_argument("--components", required=True, help="newline separated component names")
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--check", action="store_true")
    mode.add_argument("--write", action="store_true")
    args = parser.parse_args(argv)
    components = [line.strip() for line in args.components.splitlines() if line.strip()]
    if not components:
        print("checkpoint: no components given", file=sys.stderr)
        return 1
    try:
        if args.write:
            write_checkpoint(args.promote_root, args.source_sha, args.toolchain_sha256, components)
        else:
            validate_checkpoint(args.promote_root, args.source_sha, args.toolchain_sha256, components)
    except CheckpointError as error:
        print(f"checkpoint: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
