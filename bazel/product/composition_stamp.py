"""Promote a byte-hashed composition.json into locked-buildset provenance.

The byte-hashed document stays unchanged. Promoted mode copies it and replaces
the null buildset field with the digest already recorded by OrlixLockedBuildset.
Source mode copies the null document and refuses a promoted digest.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path

NULL_BUILDSET = '"buildset": null'
_HEX = frozenset("0123456789abcdef")


class CompositionStampError(ValueError):
    pass


def _require_digest(value: object, label: str) -> str:
    if not isinstance(value, str) or len(value) != 64 or any(character not in _HEX for character in value):
        raise CompositionStampError(f"{label} must be 64 lowercase hex characters")
    return value


def stamp_promoted_composition(composition_text: str, locked: dict, expected_buildset: str) -> str:
    expected = _require_digest(expected_buildset, "stamp buildset")
    if not isinstance(locked, dict):
        raise CompositionStampError("locked buildset must be a JSON object")
    actual = _require_digest(locked.get("buildset"), "locked buildset")
    if actual != expected:
        raise CompositionStampError("locked buildset does not match the stamp command")
    if NULL_BUILDSET not in composition_text:
        raise CompositionStampError("byte-hashed composition must keep buildset null")
    if actual in composition_text:
        raise CompositionStampError("byte-hashed composition must not record a buildset")
    return composition_text.replace(NULL_BUILDSET, f'"buildset": "{actual}"', 1)


def copy_source_composition(composition_text: str) -> str:
    if NULL_BUILDSET not in composition_text:
        raise CompositionStampError("source composition must keep buildset null")
    if composition_text.count(NULL_BUILDSET) != 1:
        raise CompositionStampError("source composition must contain one null buildset")
    return composition_text


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--composition", required=True)
    parser.add_argument("--out", required=True)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--promoted", action="store_true")
    mode.add_argument("--source", action="store_true")
    parser.add_argument("--locked")
    parser.add_argument("--expected")
    args = parser.parse_args(argv)
    composition_text = Path(args.composition).read_text(encoding="utf-8")
    if args.promoted:
        if not args.locked or not args.expected:
            raise CompositionStampError("promoted stamp requires the locked buildset and expected digest")
        locked = json.loads(Path(args.locked).read_text(encoding="utf-8"))
        stamped = stamp_promoted_composition(composition_text, locked, args.expected)
    else:
        stamped = copy_source_composition(composition_text)
    Path(args.out).write_text(stamped, encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
