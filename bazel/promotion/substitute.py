#!/usr/bin/env python3
"""Map reconstructed OCI trees onto a signed lock. Do not mutate artifacts.lock.json."""

from __future__ import annotations

import argparse
import json
import shutil
from pathlib import Path

import locked_buildset


class SubstituteError(ValueError):
    pass


def substitute(lock_path: str, reconstruct_dir: str, out_path: str) -> dict:
    lock_file = Path(lock_path)
    before = lock_file.read_bytes()
    locked = locked_buildset.load_locked_buildset(lock_path)
    root = Path(reconstruct_dir) / locked["buildset"]
    components: dict[str, dict] = {}
    for name, entry in locked["components"].items():
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


def stage_imported(payload: dict, stage_dir: str) -> None:
    root = Path(stage_dir)
    root.mkdir(parents=True, exist_ok=True)
    for name, entry in payload["components"].items():
        tree = Path(entry["tree"]).resolve()
        if not tree.is_dir():
            raise SubstituteError(f"missing reconstructed {name} tree: {tree}")
        dest = root / name
        if dest.is_symlink() or dest.is_file():
            dest.unlink()
        elif dest.is_dir():
            shutil.rmtree(dest)
        dest.symlink_to(tree, target_is_directory=True)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--lock", required=True)
    parser.add_argument("--reconstruct-dir", required=True)
    parser.add_argument("--out", required=True)
    parser.add_argument("--stage")
    args = parser.parse_args(argv)
    payload = substitute(args.lock, args.reconstruct_dir, args.out)
    if args.stage:
        stage_imported(payload, args.stage)
    print(payload["buildset"])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
