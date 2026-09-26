#!/usr/bin/env python3
"""Resolve source, promoted, or auto into per-boundary origins.

This is the only place that applies the kernel, installed UAPI, mlibc/sysroot,
and rootfs closure. `component_mode` stays the all-source or all-promoted
switch for promotion reproducibility. Daily builds use this resolver.
"""

from __future__ import annotations

import argparse
import json
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path

from materialize_bytes import file_pairs, materialize_tree, selected_identity
from worktree_classifier import ClassifierError, classify_paths

COMPONENTS = ("kernel", "uapi", "mlibc", "rootfs")
SOURCE = "source"
PROMOTED = "promoted"
AUTO = "auto"
REQUESTED = (SOURCE, PROMOTED, AUTO)
PROFILES = ("release", "development")
DESTINATIONS = ("iphonesimulator", "iphoneos")
BAZEL_LINK_NAMES = frozenset({"bazel-bin", "bazel-out", "bazel-testlogs"})
FACT_KEYS = (
    "kernel_changed",
    "uapi_changed",
    "mlibc_changed",
    "rootfs_changed",
    "package_changed",
    "toolchain_changed",
)


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


def kernel_slice(profile: str, destination: str) -> str:
    if profile not in PROFILES:
        raise OriginError(f"unsupported profile: {profile}")
    if destination not in DESTINATIONS:
        raise OriginError(f"unsupported destination: {destination}")
    return f"kernel-{profile}-{destination}"


def acquire_components(
    vector: OriginVector,
    *,
    profile: str,
    destination: str,
) -> list[str]:
    """Names of promoted members only. Empty acquire is legal."""
    names: list[str] = []
    if vector.uapi == PROMOTED:
        names.append("uapi")
    if vector.mlibc == PROMOTED:
        names.append("mlibc")
    if vector.rootfs == PROMOTED:
        names.append("rootfs")
    if vector.kernel == PROMOTED:
        names.append(kernel_slice(profile, destination))
    return names


def acquire_plan(
    vector: OriginVector,
    *,
    profile: str,
    destination: str,
) -> dict[str, object]:
    """Fetch list for promoted members. Named acquire must not delete siblings."""
    return {
        "components": acquire_components(vector, profile=profile, destination=destination),
        "delete_reconstruct_root": False,
        "delete_siblings": False,
    }


def bazel_flags(vector: OriginVector) -> list[str]:
    flags: list[str] = []
    if vector.requested_mode in (SOURCE, PROMOTED):
        flags.append(f"--//bazel/config:component_mode={vector.requested_mode}")
    for name in COMPONENTS:
        flags.append(f"--//bazel/config:origin_{name}={getattr(vector, name)}")
    lock = "true" if vector.promoted_members() else "false"
    flags.append(f"--//bazel/config:use_promoted_lock={lock}")
    return flags


def require_explicit_output_base(output_base: Path) -> Path:
    path = Path(output_base)
    if any(part in BAZEL_LINK_NAMES for part in path.parts):
        raise OriginError("UAPI equality does not read bazel-bin")
    if path.is_symlink():
        raise OriginError("UAPI equality requires an explicit output base")
    if not path.is_dir():
        raise OriginError("explicit output base is missing")
    resolved = path.resolve()
    if any(part in BAZEL_LINK_NAMES for part in resolved.parts):
        raise OriginError("UAPI equality does not read bazel-bin")
    return path


def uapi_tree_digest(output_base: Path, relative: str) -> str:
    """Digest installed UAPI bytes under an explicit output base."""
    base = require_explicit_output_base(output_base)
    relative_path = Path(relative)
    if relative_path.is_absolute() or any(
        part in ("", ".", "..") or part in BAZEL_LINK_NAMES for part in relative_path.parts
    ):
        raise OriginError("UAPI equality does not read bazel-bin")
    tree = base / relative_path
    if tree.is_symlink():
        raise OriginError("UAPI equality requires an explicit output base")
    if not tree.is_dir():
        raise OriginError("installed UAPI tree is missing from the explicit output base")
    with tempfile.TemporaryDirectory() as tmp:
        staging = Path(tmp) / "headers"
        materialize_tree(tree, staging)
        return selected_identity(artifacts=file_pairs(staging))


def uapi_digests_match(left_output_base: Path, right_output_base: Path, relative: str) -> bool:
    return uapi_tree_digest(left_output_base, relative) == uapi_tree_digest(right_output_base, relative)


def _require_origin(value: object, name: str) -> str:
    if value not in (SOURCE, PROMOTED):
        raise OriginError(f"{name} origin must be source or promoted")
    return str(value)


def _sha256(value: object, label: str) -> str:
    text = str(value).strip()
    if len(text) != 64 or any(char not in "0123456789abcdef" for char in text):
        raise OriginError(f"{label} is not a sha256 digest")
    return text


def _read_lock(lock_path: Path) -> tuple[str, frozenset[str]]:
    try:
        payload = json.loads(Path(lock_path).read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise OriginError("locked buildset is unreadable") from error
    if not isinstance(payload, dict):
        raise OriginError("locked buildset is unreadable")
    buildset = _sha256(payload.get("buildset"), "lock buildset")
    components = payload.get("components")
    if not isinstance(components, dict) or not components:
        raise OriginError("lock is missing components")
    return buildset, frozenset(str(name) for name in components)


def _facts_from_mapping(facts: dict) -> dict[str, bool]:
    unknown = sorted(set(facts) - set(FACT_KEYS))
    if unknown:
        raise OriginError(f"unknown origin facts: {', '.join(unknown)}")
    required = ("kernel_changed", "uapi_changed", "mlibc_changed", "rootfs_changed")
    missing = [key for key in required if key not in facts]
    if missing:
        raise OriginError(f"auto facts are incomplete: {', '.join(missing)}")
    typed = {key: False for key in FACT_KEYS}
    for key, value in facts.items():
        if not isinstance(value, bool):
            raise OriginError(f"{key} must be a boolean")
        typed[key] = value
    return typed


def _unchanged_origin(use_promoted_lock: bool) -> str:
    return PROMOTED if use_promoted_lock else SOURCE


def _origins_from_facts(facts: dict[str, bool], use_promoted_lock: bool) -> dict[str, str]:
    if facts["toolchain_changed"]:
        return {name: SOURCE for name in COMPONENTS}
    origins = {
        "kernel": SOURCE if facts["kernel_changed"] else _unchanged_origin(use_promoted_lock),
        "uapi": SOURCE if facts["uapi_changed"] else _unchanged_origin(use_promoted_lock),
        "mlibc": SOURCE if facts["mlibc_changed"] else _unchanged_origin(use_promoted_lock),
        "rootfs": SOURCE
        if facts["rootfs_changed"] or facts["package_changed"]
        else _unchanged_origin(use_promoted_lock),
    }
    return origins


def _apply_closure(
    requested: dict[str, str],
    resolved: dict[str, str],
    *,
    facts: dict[str, bool] | None,
    selected_uapi_digest: str | None,
    mlibc_consumed_uapi_digest: str | None,
) -> dict[str, str]:
    # Source kernel rebuilds that archive only. It does not force the others.
    if facts and (facts.get("package_changed") or facts.get("rootfs_changed")):
        resolved["rootfs"] = SOURCE
    if resolved["uapi"] == SOURCE:
        resolved["kernel"] = SOURCE
        resolved["rootfs"] = SOURCE
        if requested["mlibc"] == PROMOTED:
            if (
                not selected_uapi_digest
                or not mlibc_consumed_uapi_digest
                or selected_uapi_digest != mlibc_consumed_uapi_digest
            ):
                raise OriginError(
                    "promoted mlibc against source UAPI requires matching consumed UAPI digests"
                )
        else:
            resolved["mlibc"] = SOURCE
    if resolved["mlibc"] == SOURCE:
        resolved["rootfs"] = SOURCE
    return resolved


def _check_promoted(
    vector: OriginVector,
    *,
    use_promoted_lock: bool,
    lock_components: frozenset[str] | None,
    profile: str,
    destination: str,
    promoted_buildsets: dict[str, str] | None,
) -> None:
    promoted = vector.promoted_members()
    if not promoted:
        return
    if not use_promoted_lock:
        raise OriginError("promoted origins require use_promoted_lock")
    if not vector.buildset:
        raise OriginError("promoted origins require one locked buildset")
    buildsets = {vector.buildset}
    for name, buildset in (promoted_buildsets or {}).items():
        if name in promoted:
            buildsets.add(_sha256(buildset, f"{name} buildset"))
    if len(buildsets) != 1:
        raise OriginError("mixed promoted members must share one buildset")
    if vector.kernel == PROMOTED:
        slice_name = kernel_slice(profile, destination)
        if lock_components is None or slice_name not in lock_components:
            raise OriginError("unmatched promoted kernel fails analysis")


def resolve(
    *,
    requested_mode: str | None = None,
    origins: dict[str, str] | None = None,
    facts: dict | None = None,
    changed_paths: list[str] | None = None,
    component_mode: str | None = None,
    use_promoted_lock: bool = False,
    lock_buildset: str | None = None,
    lock_path: Path | None = None,
    lock_components: set[str] | frozenset[str] | None = None,
    profile: str = "release",
    destination: str = "iphonesimulator",
    selected_uapi_digest: str | None = None,
    mlibc_consumed_uapi_digest: str | None = None,
    promoted_buildsets: dict[str, str] | None = None,
    source_output_base: Path | None = None,
    promoted_output_base: Path | None = None,
    uapi_relative: str = "headers",
) -> OriginVector:
    if component_mode not in (None, SOURCE, PROMOTED):
        raise OriginError("component_mode must be source or promoted")
    if requested_mode not in (None, *REQUESTED):
        raise OriginError("requested mode must be source, promoted, or auto")
    mode = component_mode or requested_mode or (AUTO if origins or facts or changed_paths is not None else None)
    if mode is None:
        raise OriginError("requested mode must be source, promoted, or auto")

    components: frozenset[str] | None
    buildset = _sha256(lock_buildset, "lock buildset") if lock_buildset else None
    if lock_path is not None:
        locked_buildset, locked_components = _read_lock(Path(lock_path))
        if buildset and buildset != locked_buildset:
            raise OriginError("mixed promoted members must share one buildset")
        buildset = locked_buildset
        components = locked_components
    else:
        components = frozenset(lock_components) if lock_components is not None else None

    if mode == SOURCE:
        vector = OriginVector(SOURCE, SOURCE, SOURCE, SOURCE, SOURCE, None)
        return vector
    if mode == PROMOTED:
        vector = OriginVector(PROMOTED, PROMOTED, PROMOTED, PROMOTED, PROMOTED, buildset)
        _check_promoted(
            vector,
            use_promoted_lock=True,
            lock_components=components,
            profile=profile,
            destination=destination,
            promoted_buildsets=promoted_buildsets,
        )
        return vector

    if changed_paths is not None and facts is not None:
        raise OriginError("pass either changed paths or facts, not both")
    typed_facts: dict[str, bool] | None = None
    if changed_paths is not None:
        try:
            classification = classify_paths(list(changed_paths))
        except ClassifierError as error:
            raise OriginError(str(error)) from error
        typed_facts = dict(classification.facts)
        if classification.probe_uapi:
            if source_output_base is None or promoted_output_base is None:
                raise OriginError("UAPI equality requires an explicit output base")
            typed_facts["uapi_changed"] = not uapi_digests_match(
                Path(source_output_base),
                Path(promoted_output_base),
                uapi_relative,
            )
    elif facts is not None:
        typed_facts = _facts_from_mapping(facts)

    if origins is None:
        if typed_facts is None:
            raise OriginError("auto requires classified paths or facts")
        requested = {name: AUTO for name in COMPONENTS}
        resolved = _origins_from_facts(typed_facts, use_promoted_lock)
    else:
        unknown = sorted(set(origins) - set(COMPONENTS))
        if unknown:
            raise OriginError(f"unknown origin boundary: {', '.join(unknown)}")
        requested = {}
        resolved = {}
        auto_origins = (
            _origins_from_facts(typed_facts, use_promoted_lock) if typed_facts is not None else None
        )
        for name in COMPONENTS:
            choice = origins.get(name, AUTO)
            if choice not in REQUESTED:
                raise OriginError(f"{name} origin must be source, promoted, or auto")
            requested[name] = choice
            if choice == AUTO:
                if auto_origins is None:
                    raise OriginError("auto requires classified paths or facts")
                resolved[name] = auto_origins[name]
            else:
                resolved[name] = choice

    resolved = _apply_closure(
        requested,
        resolved,
        facts=typed_facts,
        selected_uapi_digest=selected_uapi_digest,
        mlibc_consumed_uapi_digest=mlibc_consumed_uapi_digest,
    )
    for name in COMPONENTS:
        _require_origin(resolved[name], name)
    vector = OriginVector(
        requested_mode=AUTO,
        kernel=resolved["kernel"],
        uapi=resolved["uapi"],
        mlibc=resolved["mlibc"],
        rootfs=resolved["rootfs"],
        buildset=buildset if PROMOTED in resolved.values() else None,
    )
    _check_promoted(
        vector,
        use_promoted_lock=use_promoted_lock,
        lock_components=components,
        profile=profile,
        destination=destination,
        promoted_buildsets=promoted_buildsets,
    )
    return vector


def payload(vector: OriginVector, *, profile: str, destination: str) -> dict:
    plan = acquire_plan(vector, profile=profile, destination=destination)
    return {
        "schema": 1,
        "kind": "origin-vector",
        "requested_mode": vector.requested_mode,
        "origins": vector.as_dict(),
        "buildset": vector.buildset,
        "acquire": plan["components"],
        "delete_reconstruct_root": False,
        "delete_siblings": False,
        "bazel_flags": bazel_flags(vector),
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--requested-mode", choices=REQUESTED)
    parser.add_argument("--component-mode", choices=(SOURCE, PROMOTED))
    parser.add_argument("--lock", type=Path)
    parser.add_argument("--profile", default="release")
    parser.add_argument("--destination", default="iphonesimulator")
    parser.add_argument("--origins", help="JSON object of per-boundary source, promoted, or auto")
    parser.add_argument("--facts", help="JSON object of *_changed booleans")
    parser.add_argument("--changed-paths", type=Path, help="file with one repository path per line")
    parser.add_argument("--selected-uapi-digest")
    parser.add_argument("--mlibc-consumed-uapi-digest")
    parser.add_argument("--promoted-buildsets", help="JSON object of boundary to buildset")
    parser.add_argument("--source-output-base", type=Path)
    parser.add_argument("--promoted-output-base", type=Path)
    parser.add_argument("--uapi-relative", default="headers")
    parser.add_argument("--use-promoted-lock", action="store_true")
    parser.add_argument("--print-acquire", action="store_true")
    parser.add_argument("--print-flags", action="store_true")
    args = parser.parse_args(argv)
    paths = None
    if args.changed_paths:
        paths = [
            line.strip()
            for line in args.changed_paths.read_text(encoding="utf-8").splitlines()
            if line.strip()
        ]
    try:
        vector = resolve(
            requested_mode=args.requested_mode,
            origins=json.loads(args.origins) if args.origins else None,
            facts=json.loads(args.facts) if args.facts else None,
            changed_paths=paths,
            component_mode=args.component_mode,
            use_promoted_lock=args.use_promoted_lock or args.component_mode == PROMOTED,
            lock_path=args.lock,
            profile=args.profile,
            destination=args.destination,
            selected_uapi_digest=args.selected_uapi_digest,
            mlibc_consumed_uapi_digest=args.mlibc_consumed_uapi_digest,
            promoted_buildsets=json.loads(args.promoted_buildsets) if args.promoted_buildsets else None,
            source_output_base=args.source_output_base,
            promoted_output_base=args.promoted_output_base,
            uapi_relative=args.uapi_relative,
        )
        body = payload(vector, profile=args.profile, destination=args.destination)
    except OriginError as error:
        print(error, file=sys.stderr)
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
