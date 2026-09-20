#!/usr/bin/env python3
"""Resolve requested component mode into per-boundary origins.

This is the only origin-compatibility authority. BUILD files, Make, and CI
must call it rather than re-encoding closure rules.

0.6 may later pass complete facts such as kernel_changed=true. This module
does not classify Git deltas, staged paths, or UAPI bytes.
"""

from __future__ import annotations

import argparse
import json
import sys
from dataclasses import dataclass
from pathlib import Path

COMPONENTS = ("kernel", "uapi", "mlibc", "rootfs")
SOURCE = "source"
PROMOTED = "promoted"
AUTO = "auto"
REQUESTED_MODES = (SOURCE, PROMOTED, AUTO)
FACT_KEYS = ("kernel_changed", "uapi_changed", "mlibc_changed", "rootfs_changed")


class OriginError(ValueError):
    pass


@dataclass(frozen=True)
class OriginVector:
    requested_mode: str
    kernel: str
    uapi: str
    mlibc: str
    rootfs: str
    buildset: str | None

    def as_dict(self) -> dict[str, str]:
        return {name: getattr(self, name) for name in COMPONENTS}

    def promoted_members(self) -> tuple[str, ...]:
        return tuple(name for name in COMPONENTS if getattr(self, name) == PROMOTED)


def _require_origin(value: str, name: str) -> str:
    if value not in (SOURCE, PROMOTED):
        raise OriginError(f"{name} origin must be source or promoted")
    return value


def _all(origin: str, requested_mode: str, buildset: str | None) -> OriginVector:
    origin = _require_origin(origin, "vector")
    return OriginVector(
        requested_mode=requested_mode,
        kernel=origin,
        uapi=origin,
        mlibc=origin,
        rootfs=origin,
        buildset=buildset if origin == PROMOTED else None,
    )


def _lock_buildset(lock_path: str | Path | None) -> str | None:
    if not lock_path:
        return None
    payload = json.loads(Path(lock_path).read_text(encoding="utf-8"))
    buildset = payload.get("buildset")
    if not isinstance(buildset, str) or not buildset:
        raise OriginError("lock is missing a buildset")
    return buildset


def _coarse_mode(changed_paths, lock_schema: int) -> str:
    migration = Path(__file__).resolve().parents[1] / "migration"
    sys.path.insert(0, str(migration))
    from component_selection import select_component_mode

    return select_component_mode(changed_paths or [], lock_schema)


def _from_facts(facts: dict) -> dict[str, str]:
    missing = [key for key in FACT_KEYS if key not in facts or facts[key] is None]
    if missing:
        raise OriginError(f"auto facts are incomplete: {', '.join(missing)}")
    unknown = [key for key in facts if key not in FACT_KEYS]
    if unknown:
        raise OriginError(f"unknown origin facts: {', '.join(sorted(unknown))}")
    typed = {}
    for key in FACT_KEYS:
        value = facts[key]
        if not isinstance(value, bool):
            raise OriginError(f"{key} must be a boolean")
        typed[key] = value
    origins = {
        "kernel": SOURCE if typed["kernel_changed"] else PROMOTED,
        "uapi": SOURCE if typed["uapi_changed"] else PROMOTED,
        "mlibc": SOURCE if typed["mlibc_changed"] else PROMOTED,
        "rootfs": SOURCE if typed["rootfs_changed"] else PROMOTED,
    }
    if origins["uapi"] == SOURCE:
        origins["kernel"] = SOURCE
        origins["mlibc"] = SOURCE
        origins["rootfs"] = SOURCE
    if origins["mlibc"] == SOURCE:
        origins["rootfs"] = SOURCE
    return origins


def validate_vector(
    vector: OriginVector,
    *,
    selected_uapi_digest: str | None = None,
    mlibc_consumed_uapi_digest: str | None = None,
    selected_sysroot_digest: str | None = None,
    rootfs_consumed_sysroot_digest: str | None = None,
    promoted_buildsets: list[str] | tuple[str, ...] | None = None,
) -> OriginVector:
    for name in COMPONENTS:
        _require_origin(getattr(vector, name), name)
    promoted = vector.promoted_members()
    extra = {item for item in (promoted_buildsets or []) if item}
    if extra and vector.buildset:
        extra.add(vector.buildset)
    if len(extra) > 1:
        raise OriginError("promoted members must come from the same locked buildset")
    if promoted and not vector.buildset:
        raise OriginError("promoted origins require one locked signed buildset")
    if vector.uapi == SOURCE and vector.kernel == PROMOTED:
        raise OriginError("promoted kernel cannot pair with source UAPI without identity proof")
    if vector.mlibc == PROMOTED and vector.uapi == SOURCE:
        if not selected_uapi_digest or not mlibc_consumed_uapi_digest:
            raise OriginError(
                "promoted mlibc against source UAPI requires matching consumed_uapi digest"
            )
        if selected_uapi_digest != mlibc_consumed_uapi_digest:
            raise OriginError(
                "promoted mlibc consumed_uapi_digest does not match selected UAPI"
            )
    if vector.rootfs == PROMOTED and vector.mlibc == SOURCE:
        if not selected_sysroot_digest or not rootfs_consumed_sysroot_digest:
            raise OriginError(
                "promoted rootfs against source mlibc requires matching sysroot digest"
            )
        if selected_sysroot_digest != rootfs_consumed_sysroot_digest:
            raise OriginError("promoted rootfs sysroot identity does not match selected sysroot")
    if vector.rootfs == PROMOTED and vector.uapi == SOURCE:
        raise OriginError("promoted rootfs cannot pair with source UAPI without identity proof")
    return vector


def acquire_components(
    vector: OriginVector,
    *,
    profile: str,
    destination: str,
) -> list[str]:
    if profile not in ("release", "development"):
        raise OriginError(f"unsupported profile: {profile}")
    if destination not in ("iphonesimulator", "iphoneos"):
        raise OriginError(f"unsupported destination: {destination}")
    names: list[str] = []
    if vector.uapi == PROMOTED:
        names.append("uapi")
    if vector.mlibc == PROMOTED:
        names.append("mlibc")
    if vector.rootfs == PROMOTED:
        names.append("rootfs")
    if vector.kernel == PROMOTED:
        names.append(f"kernel-{profile}-{destination}")
    return names


def bazel_flags(vector: OriginVector) -> list[str]:
    flags = [f"--//bazel/config:component_mode={vector.requested_mode}"]
    for name in COMPONENTS:
        flags.append(f"--//bazel/config:origin_{name}={getattr(vector, name)}")
    lock = "true" if vector.promoted_members() else "false"
    flags.append(f"--//bazel/config:use_promoted_lock={lock}")
    return flags


def resolve(
    *,
    requested_mode: str,
    vector: dict | None = None,
    facts: dict | None = None,
    changed_paths=None,
    lock_schema: int = 2,
    lock_path: str | Path | None = None,
    selected_uapi_digest: str | None = None,
    mlibc_consumed_uapi_digest: str | None = None,
    selected_sysroot_digest: str | None = None,
    rootfs_consumed_sysroot_digest: str | None = None,
    promoted_buildsets: list[str] | None = None,
) -> OriginVector:
    if requested_mode not in REQUESTED_MODES:
        raise OriginError("requested mode must be source, promoted, or auto")
    buildset = _lock_buildset(lock_path)
    if requested_mode == SOURCE:
        resolved = _all(SOURCE, SOURCE, None)
    elif requested_mode == PROMOTED:
        if not buildset:
            raise OriginError("promoted mode requires a locked signed buildset")
        resolved = _all(PROMOTED, PROMOTED, buildset)
    elif vector is not None:
        if facts is not None:
            raise OriginError("pass either an explicit vector or facts, not both")
        resolved = OriginVector(
            requested_mode=AUTO,
            kernel=_require_origin(vector.get("kernel"), "kernel"),
            uapi=_require_origin(vector.get("uapi"), "uapi"),
            mlibc=_require_origin(vector.get("mlibc"), "mlibc"),
            rootfs=_require_origin(vector.get("rootfs"), "rootfs"),
            buildset=buildset if any(vector.get(name) == PROMOTED for name in COMPONENTS) else None,
        )
    elif facts is not None:
        origins = _from_facts(facts)
        resolved = OriginVector(
            requested_mode=AUTO,
            kernel=origins["kernel"],
            uapi=origins["uapi"],
            mlibc=origins["mlibc"],
            rootfs=origins["rootfs"],
            buildset=buildset if PROMOTED in origins.values() else None,
        )
    else:
        coarse = _coarse_mode(changed_paths, lock_schema)
        resolved = _all(coarse, AUTO, buildset if coarse == PROMOTED else None)
    return validate_vector(
        resolved,
        selected_uapi_digest=selected_uapi_digest,
        mlibc_consumed_uapi_digest=mlibc_consumed_uapi_digest,
        selected_sysroot_digest=selected_sysroot_digest,
        rootfs_consumed_sysroot_digest=rootfs_consumed_sysroot_digest,
        promoted_buildsets=promoted_buildsets,
    )


def payload(
    vector: OriginVector,
    *,
    profile: str,
    destination: str,
) -> dict:
    return {
        "schema": 1,
        "kind": "origin-vector",
        "requested_mode": vector.requested_mode,
        "origins": vector.as_dict(),
        "buildset": vector.buildset,
        "acquire": acquire_components(vector, profile=profile, destination=destination),
        "bazel_flags": bazel_flags(vector),
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--requested-mode", required=True, choices=REQUESTED_MODES)
    parser.add_argument("--lock")
    parser.add_argument("--lock-schema", type=int, default=2)
    parser.add_argument("--profile", default="release")
    parser.add_argument("--destination", default="iphonesimulator")
    parser.add_argument("--vector", help="JSON object of kernel/uapi/mlibc/rootfs origins")
    parser.add_argument("--facts", help="JSON object of *_changed booleans")
    parser.add_argument("--changed-paths", help="file with one path per line")
    parser.add_argument("--selected-uapi-digest")
    parser.add_argument("--mlibc-consumed-uapi-digest")
    parser.add_argument("--selected-sysroot-digest")
    parser.add_argument("--rootfs-consumed-sysroot-digest")
    parser.add_argument("--promoted-buildsets", help="comma-separated buildset ids to reject if mixed")
    parser.add_argument("--out")
    parser.add_argument("--print-acquire", action="store_true")
    parser.add_argument("--print-flags", action="store_true")
    args = parser.parse_args(argv)
    paths = None
    if args.changed_paths:
        paths = [
            line.strip()
            for line in Path(args.changed_paths).read_text(encoding="utf-8").splitlines()
            if line.strip()
        ]
    extra = [part.strip() for part in (args.promoted_buildsets or "").split(",") if part.strip()]
    try:
        vector = resolve(
            requested_mode=args.requested_mode,
            vector=json.loads(args.vector) if args.vector else None,
            facts=json.loads(args.facts) if args.facts else None,
            changed_paths=paths,
            lock_schema=args.lock_schema,
            lock_path=args.lock,
            selected_uapi_digest=args.selected_uapi_digest,
            mlibc_consumed_uapi_digest=args.mlibc_consumed_uapi_digest,
            selected_sysroot_digest=args.selected_sysroot_digest,
            rootfs_consumed_sysroot_digest=args.rootfs_consumed_sysroot_digest,
            promoted_buildsets=extra or None,
        )
        body = payload(vector, profile=args.profile, destination=args.destination)
    except OriginError as error:
        print(error, file=sys.stderr)
        return 1
    if args.out:
        destination = Path(args.out)
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_text(json.dumps(body, indent=2, sort_keys=True) + "\n", encoding="utf-8")
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
