#!/usr/bin/env python3
"""Resumable promotion checkpoint for the dual clean build.

The dual clean builds plus staging produce, per component, an
artifact-identity-v2 digest and two independently staged product trees whose
contents were verified identical. That result set is the expensive proof.

Whether that proof is reusable is decided by the identity of the component
PRODUCING inputs, not by the commit that happens to contain them. A promotion
control-plane-only change (proposal, signing, publication, this module) keeps
the same component input identity and reuses the proof; any change to a
component producer, its Starlark rules, the component registry, the relevant
dependency locks, the build configuration, or the toolchain changes the
identity and forces clean A/B builds again.

The checkpoint records `proof_source_sha` only for provenance. Reuse is
decided by `component_input_identity` and `toolchain_sha256`.

The identity is computed from the source files that the component registry's
targets actually depend on, enumerated by Bazel (`bazel query`). The registry
is the authority for which targets exist; Bazel is the authority for what they
consume. `component_input_identity()` is pure over that file list so it can be
tested without Bazel.
"""

from __future__ import annotations

import argparse
import hashlib
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


def _sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def component_input_identity(
    repo_root: str | Path,
    inputs: list[str],
    registry_path: str | Path,
    build_config: str,
) -> str:
    """Hash the component-producing inputs: source files, registry, config.

    `inputs` are repository-relative source paths that the component targets
    depend on. Registry bytes locate the producer targets; `build_config`
    carries the destination/profile/compilation/xcode selections whose changes
    alter component bytes. Order does not matter.
    """
    root = Path(repo_root)
    digest = hashlib.sha256()
    for relative in sorted(set(str(item).strip() for item in inputs if str(item).strip())):
        digest.update(relative.encode("utf-8") + b"\0")
        candidate = root / relative
        try:
            digest.update(_sha256_file(candidate).encode("ascii"))
        except OSError:
            # A missing declared input must not be silently skipped: record it
            # so the identity differs from any checkpoint computed without it.
            digest.update(b"missing")
        digest.update(b"\n")
    try:
        digest.update(Path(registry_path).read_bytes())
    except OSError as error:
        raise CheckpointError(f"cannot read component registry: {registry_path}") from error
    digest.update(b"\0" + build_config.encode("utf-8"))
    return digest.hexdigest()


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
    proof_source_sha: str,
    component_input_identity: str,
    toolchain_sha256: str,
    components: list[str],
) -> dict:
    root = Path(promote_root)
    if len(proof_source_sha) != 40 or any(char not in "0123456789abcdef" for char in proof_source_sha):
        raise CheckpointError(f"invalid source SHA: {proof_source_sha!r}")
    if len(component_input_identity) != 64:
        raise CheckpointError("invalid component input identity")
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
        "schema": 2,
        "kind": "promotion-checkpoint",
        "proof_source_sha": proof_source_sha,
        "component_input_identity": component_input_identity,
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
    component_input_identity: str,
    toolchain_sha256: str,
    components: list[str],
) -> dict:
    """Accept a checkpoint only when the producing inputs and toolchain match."""
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
    if payload.get("component_input_identity") != component_input_identity:
        raise CheckpointError("promotion checkpoint names different component inputs")
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
    parser.add_argument("--promote-root")
    parser.add_argument("--proof-source-sha")
    parser.add_argument("--component-input-identity")
    parser.add_argument("--toolchain-sha256")
    parser.add_argument("--components", help="newline separated component names")
    parser.add_argument("--repo-root")
    parser.add_argument("--inputs-file", help="newline separated repository-relative source paths")
    parser.add_argument("--registry")
    parser.add_argument("--build-config", default="")
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--check", action="store_true")
    mode.add_argument("--write", action="store_true")
    mode.add_argument("--compute-identity", action="store_true")
    args = parser.parse_args(argv)
    if args.compute_identity:
        if not args.repo_root or not args.inputs_file or not args.registry:
            print("checkpoint: --compute-identity requires --repo-root, --inputs-file, --registry", file=sys.stderr)
            return 1
        try:
            inputs = Path(args.inputs_file).read_text(encoding="utf-8").splitlines()
        except OSError as error:
            print(f"checkpoint: {error}", file=sys.stderr)
            return 1
        try:
            print(
                component_input_identity(
                    args.repo_root, inputs, args.registry, args.build_config
                )
            )
        except CheckpointError as error:
            print(f"checkpoint: {error}", file=sys.stderr)
            return 1
        return 0
    components = [line.strip() for line in (args.components or "").splitlines() if line.strip()]
    if not components:
        print("checkpoint: no components given", file=sys.stderr)
        return 1
    for required in ("promote_root", "component_input_identity", "toolchain_sha256"):
        if not getattr(args, required):
            print(f"checkpoint: --{required.replace('_', '-')} is required", file=sys.stderr)
            return 1
    try:
        if args.write:
            if not args.proof_source_sha:
                raise CheckpointError("--proof-source-sha is required to write")
            write_checkpoint(
                args.promote_root,
                args.proof_source_sha,
                args.component_input_identity,
                args.toolchain_sha256,
                components,
            )
        else:
            validate_checkpoint(
                args.promote_root,
                args.component_input_identity,
                args.toolchain_sha256,
                components,
            )
    except CheckpointError as error:
        print(f"checkpoint: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
