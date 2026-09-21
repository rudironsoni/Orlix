#!/usr/bin/env python3
"""Map reconstructed OCI trees onto a signed lock. Do not mutate artifacts.lock.json."""

from __future__ import annotations

import argparse
import json
import os
import shutil
from pathlib import Path

import locked_buildset


class SubstituteError(ValueError):
    pass


def substitute(
    lock_path: str,
    reconstruct_dir: str,
    out_path: str,
    component_names: list[str] | None = None,
) -> dict:
    lock_file = Path(lock_path)
    before = lock_file.read_bytes()
    locked = locked_buildset.load_locked_buildset(lock_path)
    root = Path(reconstruct_dir) / locked["buildset"]
    available = locked["components"]
    if component_names:
        unknown = [name for name in component_names if name not in available]
        if unknown:
            raise SubstituteError(f"unknown promoted components: {', '.join(unknown)}")
        selected = {name: available[name] for name in component_names}
    else:
        selected = available
    components: dict[str, dict] = {}
    for name, entry in selected.items():
        tree = root / name
        if not tree.is_dir():
            raise SubstituteError(f"missing reconstructed {name} tree: {tree}")
        identity = entry.get("artifact_identity")
        component = {
            "unsigned_digest": entry["unsigned_digest"],
            "oci_digest": entry["oci_digest"],
            "oci_reference": entry["oci_reference"],
            "tree": str(tree),
        }
        if identity is not None:
            identity = locked_buildset.validate_artifact_identity(identity)
            if identity["format"] == locked_buildset.ARTIFACT_IDENTITY_V2_FORMAT:
                try:
                    locked_buildset.validate_v2_product(tree, identity, name)
                except (OSError, TypeError, ValueError) as error:
                    raise SubstituteError(str(error)) from error
            else:
                marker = tree / identity["marker"]
                if not marker.is_file() or marker.is_symlink() or marker.read_text(encoding="utf-8").strip() != identity["digest"]:
                    raise SubstituteError(f"{name} reconstructed tree has an invalid legacy artifact marker")
            component["artifact_identity"] = identity
        else:
            matches = [
                path
                for path in tree.rglob("*.sha256")
                if path.read_text(encoding="utf-8").strip() == entry["unsigned_digest"]
            ]
            if not matches:
                raise SubstituteError(
                    f"{name} reconstructed tree is missing unsigned digest {entry['unsigned_digest']}"
                )
            component["unsigned_digest_path"] = str(matches[0])
        components[name] = component
    after = lock_file.read_bytes()
    if after != before:
        raise SubstituteError("substitute mutated artifacts.lock.json")
    payload = {
        "schema": locked["schema"],
        "kind": "promoted-components",
        "buildset": locked["buildset"],
        "components": components,
    }
    Path(out_path).parent.mkdir(parents=True, exist_ok=True)
    Path(out_path).write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
    return payload


def bind_imported(
    lock_path: str,
    imported_dir: str,
    out_path: str,
    component_names: list[str],
) -> dict:
    """Record imported trees that already match the lock.

    A second worktree has those trees in git. It must not download them or
    require a signature key. A tree that does not match fails closed.
    """
    if not component_names:
        raise SubstituteError("imported reuse requires component names")
    lock_file = Path(lock_path)
    before = lock_file.read_bytes()
    locked = locked_buildset.load_locked_buildset(lock_path)
    available = locked["components"]
    unknown = [name for name in component_names if name not in available]
    if unknown:
        raise SubstituteError(f"unknown promoted components: {', '.join(unknown)}")
    stage = Path(imported_dir)
    components: dict[str, dict] = {}
    for name in component_names:
        entry = available[name]
        tree = stage / name
        identity = entry.get("artifact_identity")
        if identity is None:
            raise SubstituteError(f"{name} lock entry has no artifact identity")
        identity = locked_buildset.validate_artifact_identity(identity)
        if identity["format"] != locked_buildset.ARTIFACT_IDENTITY_V2_FORMAT:
            raise SubstituteError(f"{name} imported reuse requires artifact-identity-v2")
        try:
            locked_buildset.validate_v2_product(tree, identity, name)
        except (OSError, TypeError, ValueError) as error:
            raise SubstituteError(f"{name} imported tree does not match the lock: {error}") from error
        components[name] = {
            "unsigned_digest": entry["unsigned_digest"],
            "oci_digest": entry["oci_digest"],
            "oci_reference": entry["oci_reference"],
            "tree": str(tree),
            "artifact_identity": identity,
        }
    after = lock_file.read_bytes()
    if after != before:
        raise SubstituteError("substitute mutated artifacts.lock.json")
    payload = {
        "schema": locked["schema"],
        "kind": "promoted-components",
        "buildset": locked["buildset"],
        "components": components,
    }
    Path(out_path).parent.mkdir(parents=True, exist_ok=True)
    Path(out_path).write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
    return payload


def stage_imported(payload: dict, stage_dir: str) -> None:
    """Project reconstructed components into a package-visible real directory.

    Bazel glob() discovers package source files, not opaque whole-directory
    symlinks, so each component becomes a real directory. Files are hard-linked
    when the store and the worktree share a filesystem, and copied otherwise;
    the projection is read-only and disposable, the reconstructed store stays
    authoritative. Stale entries from an earlier staging never survive.
    """
    root = Path(stage_dir)
    root.mkdir(parents=True, exist_ok=True)
    keep = set(payload["components"])
    for child in list(root.iterdir()) if root.exists() else []:
        if child.name.startswith(".") or child.name == "BUILD.bazel":
            continue
        if child.name not in keep:
            if child.is_symlink() or child.is_file():
                child.unlink()
            elif child.is_dir():
                shutil.rmtree(child)
    for name, entry in payload["components"].items():
        tree = Path(entry["tree"]).resolve()
        if not tree.is_dir():
            raise SubstituteError(f"missing reconstructed {name} tree: {tree}")
        dest = root / name
        if dest.is_symlink() or dest.is_file():
            dest.unlink()
        elif dest.is_dir():
            shutil.rmtree(dest)
        try:
            shutil.copytree(tree, dest, symlinks=True, copy_function=os.link)
        except OSError:
            shutil.rmtree(dest, ignore_errors=True)
            try:
                shutil.copytree(tree, dest, symlinks=True)
            except OSError as error:
                raise SubstituteError(f"cannot project {name} into the package: {error}") from error


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--lock", required=True)
    parser.add_argument("--reconstruct-dir")
    parser.add_argument("--out", required=True)
    parser.add_argument("--stage")
    parser.add_argument(
        "--from-imported",
        help="directory of already-local component trees; skip download when they match the lock",
    )
    parser.add_argument(
        "--components",
        help="comma-separated lock component names to stage; default is the full lock",
    )
    args = parser.parse_args(argv)
    names = [part.strip() for part in args.components.split(",") if part.strip()] if args.components else None
    if args.from_imported:
        if not names:
            raise SubstituteError("imported reuse requires --components")
        payload = bind_imported(args.lock, args.from_imported, args.out, names)
    else:
        if not args.reconstruct_dir:
            raise SubstituteError("reconstruct-dir is required unless imported trees are reused")
        payload = substitute(args.lock, args.reconstruct_dir, args.out, component_names=names)
        if args.stage:
            stage_imported(payload, args.stage)
    print(payload["buildset"])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
