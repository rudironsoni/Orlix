#!/usr/bin/env python3
"""Read the canonical promotion component registry.

Every component-varying promotion fact (Bazel label, digest discovery path,
identity manifest stem, product layout, component type) lives in
components.json. Make recipes and workflow shells query this CLI instead of
repeating the seven-component lists; Python modules import load_registry()
directly. Target names that must exist literally (Make eval lines, static
workflow artifact lists) are covered by parity tests, not by duplication.
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

REGISTRY_FILENAME = "components.json"
REQUIRED_FIELDS = ("name", "label", "digest_path", "manifest_stem", "product", "type")


def registry_path() -> Path:
    return Path(__file__).with_name(REGISTRY_FILENAME)


def load_registry(path: str | Path | None = None) -> dict:
    location = Path(path) if path is not None else registry_path()
    try:
        payload = json.loads(location.read_text(encoding="utf-8"))
    except (OSError, ValueError) as error:
        raise ValueError(f"component registry is unreadable: {location}") from error
    if not isinstance(payload, dict) or payload.get("schema") != 1:
        raise ValueError(f"component registry has the wrong schema: {location}")
    entries = payload.get("components")
    if not isinstance(entries, list) or not entries:
        raise ValueError(f"component registry has no components: {location}")
    names = []
    for entry in entries:
        if not isinstance(entry, dict):
            raise ValueError(f"component registry entry is not an object: {entry!r}")
        for field in REQUIRED_FIELDS:
            if field not in entry:
                raise ValueError(f"component registry entry is missing {field}: {entry!r}")
        if entry["name"] in names:
            raise ValueError(f"component registry has a duplicate component: {entry['name']!r}")
        names.append(entry["name"])
        product = entry["product"]
        if not isinstance(product, dict) or product.get("kind") not in ("tree", "manifest-files"):
            raise ValueError(f"component registry product is invalid: {entry!r}")
        if product["kind"] == "tree" and not isinstance(product.get("subdir"), str):
            raise ValueError(f"component registry tree product needs a subdir: {entry!r}")
    return payload


def main(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("--registry", default=None)
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--names", action="store_true")
    group.add_argument("--labels", action="store_true")
    group.add_argument("--field", nargs=2, metavar=("NAME", "FIELD"))
    group.add_argument("--shell", metavar="NAME")
    args = parser.parse_args(argv)
    registry = load_registry(args.registry)
    entries = registry["components"]
    if args.names:
        print(" ".join(entry["name"] for entry in entries))
    elif args.labels:
        print(" ".join(entry["label"] for entry in entries))
    elif args.shell:
        for entry in entries:
            if entry["name"] == args.shell:
                product = entry["product"]
                print(f"digest_path={entry['digest_path']}")
                print(f"manifest_stem={entry['manifest_stem']}")
                print(f"product_kind={product['kind']}")
                print(f"product_subdir={product.get('subdir', '')}")
                print(f"component_type={entry['type']}")
                return 0
        raise SystemExit(f"unknown component: {args.shell}")
    else:
        name, field = args.field
        for entry in entries:
            if entry["name"] == name:
                value = entry[field]
                if isinstance(value, dict):
                    print(json.dumps(value, sort_keys=True))
                else:
                    print(value)
                return 0
        raise SystemExit(f"unknown component or field: {name} {field}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
