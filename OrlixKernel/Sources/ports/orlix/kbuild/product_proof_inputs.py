#!/usr/bin/env python3
"""Hash proof inputs in the same order as orlix_tcti_proof_inputs_sha256."""

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path


def digest(prefix: str, inputs: list[str]) -> str:
    manifest = bytearray(prefix.encode())
    for path in inputs:
        file_digest = hashlib.sha256()
        with open(path, "rb") as stream:
            for chunk in iter(lambda: stream.read(1024 * 1024), b""):
                file_digest.update(chunk)
        manifest.extend(file_digest.hexdigest().encode())
        manifest.extend(b"\n")
    return hashlib.sha256(manifest).hexdigest()


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--prefix", required=True)
    parser.add_argument("--inputs", required=True, type=Path)
    args = parser.parse_args(argv)
    inputs = [line for line in args.inputs.read_text().splitlines() if line]
    print(digest(args.prefix, inputs))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
