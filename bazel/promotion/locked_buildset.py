#!/usr/bin/env python3
"""Fail closed unless artifacts.lock.json names a digest-pinned signed buildset."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import stat
from pathlib import Path

REQUIRED = ("uapi", "mlibc", "rootfs")
KERNEL_COMPONENTS = (
    "kernel-release-iphoneos",
    "kernel-release-iphonesimulator",
    "kernel-development-iphoneos",
    "kernel-development-iphonesimulator",
)
KERNEL_PRODUCT_PATHS = frozenset(
    (
        "OrlixKernel.a",
        "arch/orlix/boot/dts/development.dtb",
        "arch/orlix/boot/dts/release.dtb",
    )
)
SCHEMA1 = 1
SCHEMA2 = 2
OCI_PREFIX = "sha256:"
GHCR_REPOSITORY = "ghcr.io/rudironsoni/orlix"
ARTIFACT_IDENTITY_V2_FORMAT = "artifact-identity-v2"
ARTIFACT_IDENTITY_V2_VERSION = 2
LEGACY_IDENTITY_FORMAT = "legacy-marker-sha256"
LEGACY_IDENTITY_VERSION = 1
BUILDSET_IDENTITY_V2_DOMAIN = "orlix.buildset.identity"
BUILDSET_IDENTITY_V2_FORMAT = "buildset-identity-v2"
BUILDSET_IDENTITY_V2_VERSION = 2
V2_PRODUCT_DIRECTORY = "product"
V2_MANIFEST_FILENAME = "artifact-identity-v2.json"
V2_DIGEST_FILENAME = "artifact-identity-v2.sha256"


class LockedBuildsetError(ValueError):
    pass


def require_sha256(digest: str) -> str:
    text = str(digest).strip()
    if len(text) != 64 or any(c not in "0123456789abcdef" for c in text):
        raise LockedBuildsetError(f"invalid sha256 digest: {digest!r}")
    return text


def require_component_name(name: str, schema: int | None = None) -> str:
    if not re.fullmatch(r"[a-z0-9]+(?:-[a-z0-9]+)*", name):
        raise LockedBuildsetError(f"invalid component name: {name!r}")
    if schema == SCHEMA2 and name.startswith("kernel-") and name not in KERNEL_COMPONENTS:
        raise LockedBuildsetError(f"unsupported Kernel component name: {name!r}")
    return name


def _safe_relative(path: object) -> bool:
    if not isinstance(path, str) or not path or path.startswith("/") or "\x00" in path:
        return False
    parts = path.split("/")
    return all(part not in ("", ".", "..") for part in parts)


def validate_artifact_identity(identity: dict, *, format: str | None = None) -> dict:
    if not isinstance(identity, dict):
        raise LockedBuildsetError("artifact identity must be a JSON object")
    identity_format = identity.get("format")
    version = identity.get("version")
    if not isinstance(identity_format, str) or not isinstance(version, int) or isinstance(version, bool):
        raise LockedBuildsetError("artifact identity format and version are invalid")
    if format is not None and identity_format != format:
        raise LockedBuildsetError(f"unexpected artifact identity format: {identity_format!r}")
    if identity_format == ARTIFACT_IDENTITY_V2_FORMAT:
        if version != ARTIFACT_IDENTITY_V2_VERSION:
            raise LockedBuildsetError("artifact-identity-v2 version is invalid")
        if set(identity) != {"format", "version", "digest"}:
            raise LockedBuildsetError("artifact-identity-v2 fields are invalid")
        return {
            "format": ARTIFACT_IDENTITY_V2_FORMAT,
            "version": ARTIFACT_IDENTITY_V2_VERSION,
            "digest": require_sha256(identity.get("digest")),
        }
    if identity_format == LEGACY_IDENTITY_FORMAT:
        if version != LEGACY_IDENTITY_VERSION:
            raise LockedBuildsetError("legacy-marker-sha256 version is invalid")
        if set(identity) != {"format", "version", "digest", "marker"}:
            raise LockedBuildsetError("legacy-marker-sha256 fields are invalid")
        marker = identity.get("marker")
        if not _safe_relative(marker):
            raise LockedBuildsetError("legacy artifact identity marker is invalid")
        return {
            "format": LEGACY_IDENTITY_FORMAT,
            "version": LEGACY_IDENTITY_VERSION,
            "digest": require_sha256(identity.get("digest")),
            "marker": marker,
        }
    raise LockedBuildsetError(f"unknown artifact identity format: {identity_format!r}")


def entry_artifact_identity(entry: dict) -> dict:
    identity = entry.get("artifact_identity")
    if identity is not None:
        return validate_artifact_identity(identity)
    return {
        "format": LEGACY_IDENTITY_FORMAT,
        "version": LEGACY_IDENTITY_VERSION,
        "digest": require_sha256(entry["unsigned_digest"]),
    }


def entry_artifact_digest(entry: dict) -> str:
    return entry_artifact_identity(entry)["digest"]


def validate_component(name: str, entry: dict, schema: int | None = None) -> dict:
    if not isinstance(entry, dict):
        raise LockedBuildsetError(f"{name} lock entry must be a JSON object")
    if schema is None:
        schema = SCHEMA2 if "artifact_identity" in entry else SCHEMA1
    if schema not in (SCHEMA1, SCHEMA2):
        raise LockedBuildsetError(f"unsupported buildset schema: {schema}")
    require_component_name(name, schema=schema)
    if schema == SCHEMA1:
        if name.startswith("kernel-"):
            raise LockedBuildsetError("Kernel components require buildset schema 2")
        unsigned = require_sha256(entry["unsigned_digest"])
    else:
        identity = validate_artifact_identity(entry.get("artifact_identity"))
        if name in KERNEL_COMPONENTS and identity["format"] != ARTIFACT_IDENTITY_V2_FORMAT:
            raise LockedBuildsetError(f"{name} requires artifact-identity-v2")
        unsigned_value = entry.get("unsigned_digest", identity["digest"])
        unsigned = require_sha256(unsigned_value)
        if unsigned != identity["digest"]:
            raise LockedBuildsetError(f"{name} unsigned digest differs from artifact identity")
    oci = entry.get("oci_digest")
    if not isinstance(oci, str) or not oci.startswith(OCI_PREFIX):
        raise LockedBuildsetError(f"{name} lock entry missing oci_digest")
    require_sha256(oci[len(OCI_PREFIX):])
    reference = entry.get("oci_reference") or ""
    expected = f"{GHCR_REPOSITORY}/{name}@{oci}"
    if reference != expected:
        raise LockedBuildsetError(f"{name} requires {expected}, not {reference!r}; mutable latest is forbidden")
    if schema == SCHEMA1:
        return {"unsigned_digest": unsigned, "oci_digest": oci, "oci_reference": reference}
    return {
        "unsigned_digest": unsigned,
        "artifact_identity": identity,
        "oci_digest": oci,
        "oci_reference": reference,
    }


def _buildset_digest_v1(components: dict) -> str:
    parts = [f"{name}:{entry['oci_digest']}" for name, entry in sorted(components.items())]
    return hashlib.sha256("\n".join(parts).encode("utf-8")).hexdigest()


def buildset_manifest_v2(components: dict) -> bytes:
    normalized = {}
    for name, entry in sorted(components.items()):
        require_component_name(name, schema=SCHEMA2)
        validated = validate_component(name, entry, schema=SCHEMA2)
        normalized[name] = {
            "artifact_identity": validated["artifact_identity"],
            "oci_digest": validated["oci_digest"],
        }
    payload = {
        "components": normalized,
        "domain": BUILDSET_IDENTITY_V2_DOMAIN,
        "format": BUILDSET_IDENTITY_V2_FORMAT,
        "version": BUILDSET_IDENTITY_V2_VERSION,
    }
    return (json.dumps(payload, ensure_ascii=True, sort_keys=True, separators=(",", ":")) + "\n").encode("ascii")


def buildset_digest_v2(components: dict) -> str:
    return hashlib.sha256(buildset_manifest_v2(components)).hexdigest()


def buildset_digest(components: dict, schema: int = SCHEMA1) -> str:
    if schema == SCHEMA1:
        return _buildset_digest_v1(components)
    if schema == SCHEMA2:
        return buildset_digest_v2(components)
    raise LockedBuildsetError(f"unsupported buildset schema: {schema}")


def _regular_file(path: Path) -> bool:
    try:
        return stat.S_ISREG(path.lstat().st_mode)
    except OSError:
        return False


def validate_v2_product(
    root: str | Path,
    identity: dict,
    component: str | None = None,
) -> dict:
    import sys

    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
    from content_digest import artifact_manifest_v2

    typed = validate_artifact_identity(identity, format=ARTIFACT_IDENTITY_V2_FORMAT)
    component_root = Path(root)
    try:
        if not stat.S_ISDIR(component_root.lstat().st_mode):
            raise LockedBuildsetError("v2 component root is not a real directory")
    except OSError as error:
        raise LockedBuildsetError("v2 component root is missing") from error
    product = component_root / V2_PRODUCT_DIRECTORY
    manifest_path = component_root / V2_MANIFEST_FILENAME
    digest_path = component_root / V2_DIGEST_FILENAME
    if not _regular_file(manifest_path) or not _regular_file(digest_path):
        raise LockedBuildsetError("v2 component identity files are missing")
    try:
        top_level = {path.name for path in component_root.iterdir()}
    except OSError as error:
        raise LockedBuildsetError("v2 component root is unreadable") from error
    if top_level != {
        V2_PRODUCT_DIRECTORY,
        V2_MANIFEST_FILENAME,
        V2_DIGEST_FILENAME,
    }:
        raise LockedBuildsetError("v2 component contains files outside its product boundary")
    try:
        manifest_bytes = manifest_path.read_bytes()
        manifest = json.loads(manifest_bytes)
        digest_text = digest_path.read_text(encoding="ascii").strip()
    except (OSError, UnicodeError, ValueError) as error:
        raise LockedBuildsetError("v2 component identity files are invalid") from error
    if not isinstance(manifest, dict) or not isinstance(manifest.get("entries"), list):
        raise LockedBuildsetError("v2 artifact identity manifest is invalid")
    if not isinstance(digest_text, str) or digest_text != typed["digest"]:
        raise LockedBuildsetError("v2 artifact identity digest file differs from the lock")
    try:
        if not stat.S_ISDIR(product.lstat().st_mode):
            raise LockedBuildsetError("v2 component product is not a real directory")
    except OSError as error:
        raise LockedBuildsetError("v2 component product is missing") from error

    selected: dict[str, Path] = {}
    for item in manifest["entries"]:
        if not isinstance(item, dict) or item.get("type") != "file":
            raise LockedBuildsetError("v2 artifact identity must contain file entries")
        if set(item) != {"content_sha256", "mode", "path", "type"}:
            raise LockedBuildsetError("v2 artifact identity entry fields are invalid")
        relative = item.get("path")
        if not _safe_relative(relative) or relative in selected:
            raise LockedBuildsetError("v2 artifact identity path is invalid")
        if not isinstance(item.get("mode"), int) or isinstance(item.get("mode"), bool):
            raise LockedBuildsetError("v2 artifact identity mode is invalid")
        require_sha256(item["content_sha256"])
        source = product.joinpath(*relative.split("/"))
        try:
            current = product
            for part in relative.split("/")[:-1]:
                current = current / part
                if stat.S_ISLNK(current.lstat().st_mode):
                    raise LockedBuildsetError("v2 artifact identity path escapes product")
        except OSError as error:
            raise LockedBuildsetError("v2 artifact identity product file is missing") from error
        if not _regular_file(source):
            raise LockedBuildsetError("v2 artifact identity product file is missing")
        selected[relative] = source

    actual_files: set[str] = set()
    for source in product.rglob("*"):
        try:
            mode = source.lstat().st_mode
        except OSError as error:
            raise LockedBuildsetError("v2 artifact identity product is unreadable") from error
        if stat.S_ISLNK(mode):
            raise LockedBuildsetError("v2 artifact identity product contains a symlink")
        if stat.S_ISREG(mode):
            actual_files.add(source.relative_to(product).as_posix())
        elif not stat.S_ISDIR(mode):
            raise LockedBuildsetError("v2 artifact identity product contains an unsupported entry")
    if actual_files != set(selected):
        raise LockedBuildsetError("v2 artifact identity product files differ from the manifest")
    if component in KERNEL_COMPONENTS and actual_files != KERNEL_PRODUCT_PATHS:
        raise LockedBuildsetError("Kernel product must contain the archive and both DTBs")

    try:
        canonical = artifact_manifest_v2(artifacts=selected)
    except (OSError, ValueError) as error:
        raise LockedBuildsetError("v2 artifact identity product cannot be serialized") from error
    if canonical != manifest_bytes:
        raise LockedBuildsetError("v2 artifact identity manifest is not canonical")
    actual_digest = hashlib.sha256(canonical).hexdigest()
    if actual_digest != typed["digest"]:
        raise LockedBuildsetError("v2 artifact identity does not match the lock")
    return {
        "product": product,
        "manifest": manifest_path,
        "digest": digest_path,
        "artifacts": selected,
        "manifest_bytes": canonical,
        "identity": typed,
    }


def required_components(schema: int) -> tuple[str, ...]:
    return REQUIRED + KERNEL_COMPONENTS if schema == SCHEMA2 else REQUIRED


def load_locked_buildset(
    path: str,
    required: tuple[str, ...] | None = None,
) -> dict:
    payload = json.loads(Path(path).read_text(encoding="utf-8"))
    if not isinstance(payload, dict):
        raise LockedBuildsetError("locked buildset must be a JSON object")
    schema = payload.get("schema", SCHEMA1)
    if not isinstance(schema, int) or isinstance(schema, bool) or schema not in (SCHEMA1, SCHEMA2):
        raise LockedBuildsetError(f"unsupported buildset schema: {schema!r}")
    buildset = payload.get("buildset")
    if not buildset:
        raise LockedBuildsetError("promoted mode requires a signed artifacts.lock.json buildset")
    buildset = require_sha256(str(buildset))
    components = payload.get("components") or {}
    if not isinstance(components, dict):
        raise LockedBuildsetError("locked buildset components must be a JSON object")
    required_names = required_components(schema) if required is None else required
    missing = [name for name in required_names if name not in components]
    if missing:
        raise LockedBuildsetError(f"lock missing required components: {missing}")
    locked: dict[str, dict] = {}
    for name, entry in components.items():
        locked[name] = validate_component(name, entry, schema=schema)
    if buildset != buildset_digest(locked, schema=schema):
        raise LockedBuildsetError("buildset digest does not match its component set")
    return {
        "schema": schema,
        "kind": "locked-buildset",
        "buildset": buildset,
        "components": locked,
    }


def write_locked_buildset(lock_path: str, out_path: str) -> dict:
    payload = load_locked_buildset(lock_path)
    Path(out_path).write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
    return payload


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--lock", required=True)
    parser.add_argument("--out", required=True)
    args = parser.parse_args(argv)
    payload = write_locked_buildset(args.lock, args.out)
    print(payload["buildset"])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
