#!/usr/bin/env python3
"""Compare two unsigned component digests from independent promotion builds."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from content_digest import tree_digest  # noqa: E402


def require_sha256(digest: str) -> str:
    text = digest.strip()
    if len(text) != 64 or any(c not in "0123456789abcdef" for c in text):
        raise ValueError(f"invalid sha256 digest: {digest!r}")
    return text


def read_digest(path: str) -> str:
    return require_sha256(Path(path).read_text(encoding="utf-8"))


def compare_digests(first: str, second: str) -> str:
    left = read_digest(first)
    right = read_digest(second)
    if left != right:
        raise ValueError(f"promotion mismatch: {left} != {right}")
    return left


def compare_trees(first: str, second: str) -> str:
    left, right = Path(first), Path(second)
    if left.resolve() == right.resolve():
        raise ValueError("promotion requires independent component trees")
    digest = tree_digest(left)
    if digest != tree_digest(right):
        raise ValueError("promotion component contents differ")
    return digest


def write_proposal(path: str, component: str, digest: str, content_digest: str | None = None) -> None:
    payload = {
        "schema": 1,
        "component": component,
        "unsigned_digest": digest,
        "signed": False,
        "oci_digest": None,
    }
    if content_digest is not None:
        payload["output_tree_digest"] = require_sha256(content_digest)
    Path(path).write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("first")
    parser.add_argument("second")
    parser.add_argument("--first-tree")
    parser.add_argument("--second-tree")
    parser.add_argument("--component", default="uapi")
    parser.add_argument("--proposal")
    parser.add_argument("--sbom")
    parser.add_argument("--in-toto")
    parser.add_argument("--lock-proposal")
    parser.add_argument("--lock", default="artifacts.lock.json")
    args = parser.parse_args(argv)
    digest = compare_digests(args.first, args.second)
    content_digest = None
    if any((args.proposal, args.sbom, args.in_toto, args.lock_proposal)) or args.first_tree or args.second_tree:
        if not args.first_tree or not args.second_tree:
            raise ValueError("promotion requires both component trees")
        content_digest = compare_trees(args.first_tree, args.second_tree)
    print(digest)
    if args.proposal:
        write_proposal(args.proposal, args.component, digest, content_digest)
    if args.sbom:
        from sbom import write_sbom

        write_sbom(args.sbom, args.component, digest, actual_digest=digest)
    if args.in_toto:
        from in_toto import write_provenance

        write_provenance(args.in_toto, args.component, digest)
    if args.lock_proposal:
        from lock_proposal import apply_lock_proposal, write_lock_proposal

        lock_path = Path(args.lock)
        before = lock_path.read_text(encoding="utf-8") if lock_path.is_file() else None
        write_lock_proposal(args.lock_proposal, args.component, digest)
        try:
            apply_lock_proposal(args.lock_proposal, args.lock)
        except ValueError:
            pass
        else:
            raise ValueError("unsigned lock proposal must not mutate artifacts.lock.json")
        after = lock_path.read_text(encoding="utf-8") if lock_path.is_file() else None
        if before != after:
            raise ValueError("unsigned lock proposal mutated artifacts.lock.json")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
