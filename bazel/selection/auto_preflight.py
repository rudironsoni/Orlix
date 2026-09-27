#!/usr/bin/env python3
"""Resolve daily auto origins without reading bazel-bin or running a product build."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

from origin_resolver import (
    AUTO,
    PROMOTED,
    SOURCE,
    OriginError,
    payload,
    resolve,
)


def run_preflight(
    *,
    requested_mode: str,
    changed_paths: list[str] | None = None,
    lock_path: Path | None = None,
    profile: str = "release",
    destination: str = "iphonesimulator",
    use_promoted_lock: bool = True,
    source_output_base: Path | None = None,
    promoted_output_base: Path | None = None,
    uapi_relative: str = "headers",
    selected_uapi_digest: str | None = None,
    mlibc_consumed_uapi_digest: str | None = None,
) -> dict:
    if requested_mode == SOURCE:
        vector = resolve(component_mode=SOURCE)
    elif requested_mode == PROMOTED:
        vector = resolve(
            component_mode=PROMOTED,
            lock_path=lock_path,
            profile=profile,
            destination=destination,
        )
    elif requested_mode == AUTO:
        if changed_paths is None:
            raise OriginError("auto requires classified paths or facts")
        vector = resolve(
            requested_mode=AUTO,
            changed_paths=changed_paths,
            use_promoted_lock=use_promoted_lock,
            lock_path=lock_path,
            profile=profile,
            destination=destination,
            source_output_base=source_output_base,
            promoted_output_base=promoted_output_base,
            uapi_relative=uapi_relative,
            selected_uapi_digest=selected_uapi_digest,
            mlibc_consumed_uapi_digest=mlibc_consumed_uapi_digest,
        )
    else:
        raise OriginError("requested mode must be source, promoted, or auto")
    return payload(vector, profile=profile, destination=destination)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--requested-mode", default=AUTO)
    parser.add_argument("--lock", type=Path)
    parser.add_argument("--changed-paths", type=Path)
    parser.add_argument("--profile", default="release")
    parser.add_argument("--destination", default="iphonesimulator")
    parser.add_argument("--source-output-base", type=Path)
    parser.add_argument("--promoted-output-base", type=Path)
    parser.add_argument("--uapi-relative", default="headers")
    parser.add_argument("--no-promoted-lock", action="store_true")
    parser.add_argument("--print-flags", action="store_true")
    parser.add_argument("--print-acquire", action="store_true")
    args = parser.parse_args(argv)
    paths = None
    if args.changed_paths:
        paths = [
            line.strip()
            for line in args.changed_paths.read_text(encoding="utf-8").splitlines()
            if line.strip()
        ]
    try:
        body = run_preflight(
            requested_mode=args.requested_mode,
            changed_paths=paths,
            lock_path=args.lock,
            profile=args.profile,
            destination=args.destination,
            use_promoted_lock=not args.no_promoted_lock,
            source_output_base=args.source_output_base,
            promoted_output_base=args.promoted_output_base,
            uapi_relative=args.uapi_relative,
        )
    except OriginError as error:
        print(f"auto-preflight: {error}", file=sys.stderr)
        return 1
    if args.print_acquire:
        print(" ".join(body["acquire"]))
        return 0
    if args.print_flags:
        print(" ".join(body["bazel_flags"]))
        return 0
    print(json.dumps(body, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
