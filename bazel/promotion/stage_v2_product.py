#!/usr/bin/env python3
"""Stage exactly the product described by an artifact-identity-v2 manifest.

Copies only the file entries named by the manifest from a product source tree
into a promotion staging directory, plus the identity manifest and digest
themselves. Anything the manifest does not name never enters the promoted
payload. Validity of the manifest (file-only entries, canonical bytes,
digest agreement) stays with validate_v2_product, which fails closed.
"""

from __future__ import annotations

import argparse
import json
import shutil
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from locked_buildset import require_sha256  # noqa: E402


class StageError(ValueError):
    pass


def stage_product(manifest_path: str | Path, product_src: str | Path, stage_dir: str | Path) -> dict:
    manifest_path = Path(manifest_path)
    product_src = Path(product_src)
    stage = Path(stage_dir)
    try:
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    except (OSError, ValueError) as error:
        raise StageError(f"unreadable artifact identity manifest: {manifest_path}") from error
    entries = manifest.get("entries")
    if not isinstance(entries, list):
        raise StageError(f"artifact identity manifest has no entries list: {manifest_path}")
    files = [entry["path"] for entry in entries if isinstance(entry, dict) and entry.get("type") == "file"]
    if not files:
        raise StageError(f"artifact identity manifest names no files: {manifest_path}")
    digest_path = manifest_path.with_suffix(".sha256")
    try:
        digest = require_sha256(digest_path.read_text(encoding="ascii"))
    except (OSError, ValueError) as error:
        raise StageError(f"unreadable artifact identity digest: {digest_path}") from error
    product = stage / "product"
    for relative in files:
        source = product_src / relative
        if not source.is_file() or source.is_symlink():
            raise StageError(f"promotion product is missing file: {relative}")
        target = product / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)
    try:
        shutil.copy2(manifest_path, stage / "artifact-identity-v2.json")
        shutil.copy2(digest_path, stage / "artifact-identity-v2.sha256")
    except OSError as error:
        raise StageError(f"cannot stage artifact identity files: {error}") from error
    return {"files": len(files), "digest": digest}


def stage_cold_source(
    source_tree: str | Path,
    stage_dir: str | Path,
    manifest_stem: str,
    product_kind: str,
    product_subdir: str = "",
) -> dict:
    """Stage a cold Bazel output directory as the reconstructed component.

    Source actions emit ``<stem>.artifact-identity-v2.json``. Reconstruction
    extracts the promoted product, which names that manifest
    ``artifact-identity-v2.json``. ``stage_product`` performs that copy.
    """
    source = Path(source_tree)
    if (
        not isinstance(manifest_stem, str)
        or not manifest_stem
        or "/" in manifest_stem
        or manifest_stem in {".", ".."}
    ):
        raise StageError(f"invalid artifact identity stem: {manifest_stem!r}")
    manifest = source / f"{manifest_stem}.artifact-identity-v2.json"
    if product_kind == "tree":
        if not isinstance(product_subdir, str) or not product_subdir:
            raise StageError(f"tree product subdir is invalid: {product_subdir!r}")
        subdir = Path(product_subdir)
        if subdir.is_absolute() or ".." in subdir.parts:
            raise StageError(f"tree product subdir is invalid: {product_subdir!r}")
        product_src = source / subdir
    elif product_kind == "manifest-files":
        if product_subdir:
            raise StageError("manifest-files product does not take a subdirectory")
        product_src = source
    else:
        raise StageError(f"unknown product kind: {product_kind!r}")
    return stage_product(manifest, product_src, stage_dir)


def main(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("--manifest")
    parser.add_argument("--product-src")
    parser.add_argument("--stage", required=True)
    parser.add_argument("--cold-source")
    parser.add_argument("--manifest-stem")
    parser.add_argument("--product-kind")
    parser.add_argument("--product-subdir", default="")
    args = parser.parse_args(argv)
    if args.cold_source is not None:
        if args.manifest or args.product_src:
            raise StageError("cold source staging does not take a separate manifest or product source")
        if not args.manifest_stem or not args.product_kind:
            raise StageError("cold source staging requires the identity stem and product kind")
        payload = stage_cold_source(
            args.cold_source,
            args.stage,
            args.manifest_stem,
            args.product_kind,
            args.product_subdir,
        )
    else:
        if not args.manifest or not args.product_src:
            raise StageError("manifest and product source are required")
        payload = stage_product(args.manifest, args.product_src, args.stage)
    print(json.dumps(payload, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
