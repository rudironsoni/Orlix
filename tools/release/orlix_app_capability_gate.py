#!/usr/bin/env python3
"""Validate Orlix application capabilities and exported product identity."""

from __future__ import annotations

import argparse
import json
import plistlib
import subprocess
import sys
from pathlib import Path
from typing import Any


PLATFORMS = {"ios", "ipados", "macos"}
CAPABILITY_STATES = {"runtime_tested", "built_unverified", "blocked", "planned", "deferred"}
APP_SOURCE_SUFFIXES = {".swift", ".plist", ".entitlements", ".xcprivacy"}


class GateError(RuntimeError):
    pass


def fail(message: str) -> None:
    raise GateError(message)


def load_json(path: Path) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        fail(f"cannot read JSON {path}: {error}")
    if not isinstance(value, dict):
        fail(f"expected JSON object: {path}")
    return value


def load_plist(path: Path) -> dict[str, Any]:
    try:
        with path.open("rb") as handle:
            value = plistlib.load(handle)
    except (OSError, plistlib.InvalidFileException) as error:
        fail(f"cannot read plist {path}: {error}")
    if not isinstance(value, dict):
        fail(f"expected plist dictionary: {path}")
    return value


def load_plist_bytes(data: bytes, label: str) -> dict[str, Any]:
    try:
        value = plistlib.loads(data)
    except plistlib.InvalidFileException as error:
        fail(f"cannot read plist from {label}: {error}")
    if not isinstance(value, dict):
        fail(f"expected plist dictionary from {label}")
    return value


def require_nonempty_strings(value: Any, label: str) -> list[str]:
    if not isinstance(value, list) or not value or not all(isinstance(item, str) and item for item in value):
        fail(f"{label} must be a non-empty string array")
    return value


def require_platform_map(value: Any, label: str, value_type: type) -> dict[str, Any]:
    if not isinstance(value, dict) or set(value) != PLATFORMS:
        fail(f"{label} must define exactly ios, ipados, and macos")
    if not all(type(item) is value_type for item in value.values()):
        fail(f"{label} contains an invalid value type")
    return value


def scan_forbidden_sources(repo_root: Path, fragments: list[str]) -> None:
    candidates = [repo_root / "project.yml"]
    source_root = repo_root / "Orlix/App/Orlix"
    candidates.extend(
        path for path in source_root.rglob("*") if path.is_file() and path.suffix in APP_SOURCE_SUFFIXES
    )
    for path in candidates:
        try:
            text = path.read_text(encoding="utf-8")
        except (OSError, UnicodeDecodeError) as error:
            fail(f"cannot scan product input {path}: {error}")
        for fragment in fragments:
            if fragment in text:
                fail(f"forbidden product identity fragment {fragment!r} in {path.relative_to(repo_root)}")


def validate_manifest(manifest_path: Path, repo_root: Path) -> dict[str, Any]:
    manifest = load_json(manifest_path)
    if manifest.get("schema_version") != 1:
        fail("unsupported release-input manifest schema")

    required = require_nonempty_strings(manifest.get("capability_requirements"), "capability_requirements")
    if len(required) != len(set(required)):
        fail("capability_requirements contains duplicate IDs")

    capabilities = manifest.get("capabilities")
    if not isinstance(capabilities, list) or not capabilities:
        fail("capabilities must be a non-empty array")

    ids: list[str] = []
    ledger_ids: list[int] = []
    for index, capability in enumerate(capabilities):
        label = f"capabilities[{index}]"
        if not isinstance(capability, dict):
            fail(f"{label} must be an object")
        capability_id = capability.get("id")
        if not isinstance(capability_id, str) or not capability_id:
            fail(f"{label}.id must be a non-empty string")
        ids.append(capability_id)
        for field in ("title", "category", "action"):
            if not isinstance(capability.get(field), str) or not capability[field]:
                fail(f"{label}.{field} must be a non-empty string")
        require_nonempty_strings(capability.get("scope"), f"{label}.scope")
        evidence = require_nonempty_strings(capability.get("source_evidence"), f"{label}.source_evidence")
        require_nonempty_strings(capability.get("approval_requirements"), f"{label}.approval_requirements")

        feature_ids = capability.get("feature_ledger_ids")
        if not isinstance(feature_ids, list) or not all(type(item) is int for item in feature_ids):
            fail(f"{label}.feature_ledger_ids must be an integer array")
        ledger_ids.extend(feature_ids)

        states = require_platform_map(capability.get("platform_states"), f"{label}.platform_states", str)
        if not set(states.values()).issubset(CAPABILITY_STATES):
            fail(f"{label}.platform_states contains an unsupported state")
        internal = require_platform_map(
            capability.get("internal_invocation_allowed"), f"{label}.internal_invocation_allowed", bool
        )
        advertising = require_platform_map(
            capability.get("public_advertising_allowed"), f"{label}.public_advertising_allowed", bool
        )
        for platform in PLATFORMS:
            if internal[platform] and states[platform] in {"planned", "deferred"}:
                fail(f"{capability_id} allows internal {platform} invocation while {states[platform]}")
            if advertising[platform] and states[platform] != "runtime_tested":
                fail(f"{capability_id} allows public {platform} advertising without runtime_tested state")

        for relative_path in evidence:
            if Path(relative_path).is_absolute() or not (repo_root / relative_path).exists():
                fail(f"{capability_id} has missing repository evidence: {relative_path}")

    if len(ids) != len(set(ids)):
        fail("capabilities contains duplicate IDs")
    if set(ids) != set(required):
        missing = sorted(set(required) - set(ids))
        unexpected = sorted(set(ids) - set(required))
        fail(f"capability inventory mismatch; missing={missing}, unexpected={unexpected}")
    if len(ledger_ids) != len(set(ledger_ids)) or sorted(ledger_ids) != list(range(1, 32)):
        fail("feature_ledger_ids must cover 1 through 31 exactly once")

    public_approved = manifest.get("public_distribution_approved")
    if type(public_approved) is not bool:
        fail("public_distribution_approved must be a boolean")
    if not public_approved and any(
        allowed
        for capability in capabilities
        for allowed in capability["public_advertising_allowed"].values()
    ):
        fail("public advertising is enabled without public distribution approval")

    exported = manifest.get("exported_product")
    if not isinstance(exported, dict):
        fail("exported_product must be an object")
    for field in (
        "main_bundle_id",
        "display_name",
        "live_activity_bundle_id",
        "live_activity_widget_kind",
        "capability_manifest_relative_path",
        "privacy_manifest_relative_path",
        "privacy_manifest_source",
        "encryption_classification_status",
        "provisioning_status",
        "cloudkit_production_schema_status",
    ):
        if not isinstance(exported.get(field), str) or not exported[field]:
            fail(f"exported_product.{field} must be a non-empty string")
    if type(exported.get("declared_non_exempt_encryption")) is not bool:
        fail("exported_product.declared_non_exempt_encryption must be a boolean")
    fragments = require_nonempty_strings(
        exported.get("forbidden_identity_fragments"), "exported_product.forbidden_identity_fragments"
    )
    expected_entitlements = exported.get("expected_entitlements")
    if not isinstance(expected_entitlements, dict):
        fail("exported_product.expected_entitlements must be an object")
    for field in (
        "aps_environment",
        "icloud_container",
        "icloud_service",
        "application_identifier_suffix",
        "keychain_group_suffix",
        "ubiquity_kvstore_suffix",
    ):
        if not isinstance(expected_entitlements.get(field), str) or not expected_entitlements[field]:
            fail(f"exported_product.expected_entitlements.{field} must be a non-empty string")
    for field in ("network_client", "network_server"):
        if type(expected_entitlements.get(field)) is not bool:
            fail(f"exported_product.expected_entitlements.{field} must be a boolean")
    privacy_source = repo_root / exported["privacy_manifest_source"]
    if not privacy_source.is_file():
        fail(f"missing privacy manifest source: {exported['privacy_manifest_source']}")
    scan_forbidden_sources(repo_root, fragments)
    return manifest


def run_plist_command(command: list[str], label: str) -> dict[str, Any]:
    try:
        result = subprocess.run(command, check=True, capture_output=True)
    except (OSError, subprocess.CalledProcessError) as error:
        fail(f"cannot read {label}: {error}")
    return load_plist_bytes(result.stdout, label)


def list_contains_suffix(value: Any, suffix: str) -> bool:
    return isinstance(value, list) and any(isinstance(item, str) and item.endswith(suffix) for item in value)


def require_suffix(value: Any, suffix: str, label: str) -> None:
    if not isinstance(value, str) or not value.endswith(suffix):
        fail(f"{label} must end with {suffix}")


def validate_entitlements(
    entitlements: dict[str, Any], expected: dict[str, Any], label: str, *, require_network: bool
) -> None:
    require_suffix(entitlements.get("application-identifier"), expected["application_identifier_suffix"], f"{label} application-identifier")
    if not list_contains_suffix(entitlements.get("keychain-access-groups"), expected["keychain_group_suffix"]):
        fail(f"{label} keychain-access-groups does not include {expected['keychain_group_suffix']}")
    if expected["icloud_container"] not in entitlements.get("com.apple.developer.icloud-container-identifiers", []):
        fail(f"{label} is missing expected iCloud container")
    if expected["icloud_service"] not in entitlements.get("com.apple.developer.icloud-services", []):
        fail(f"{label} is missing expected iCloud service")
    require_suffix(
        entitlements.get("com.apple.developer.ubiquity-kvstore-identifier"),
        expected["ubiquity_kvstore_suffix"],
        f"{label} ubiquity key-value store identifier",
    )
    values = [("aps-environment", expected["aps_environment"])]
    if require_network:
        values.extend(
            [
                ("com.apple.security.network.client", expected["network_client"]),
                ("com.apple.security.network.server", expected["network_server"]),
            ]
        )
    for key, expected_value in values:
        if entitlements.get(key) != expected_value:
            fail(f"{label} {key} differs expected value")


def validate_exported_app(
    app: Path,
    manifest_path: Path,
    repo_root: Path,
    entitlements_path: Path | None,
    profile_path: Path | None,
    require_public_approval: bool,
) -> None:
    manifest = validate_manifest(manifest_path, repo_root)
    exported = manifest["exported_product"]
    if not app.is_dir():
        fail(f"missing exported application: {app}")

    info = load_plist(app / "Info.plist")
    if info.get("CFBundleIdentifier") != exported["main_bundle_id"]:
        fail("exported application bundle identifier differs release inputs")
    if info.get("CFBundleDisplayName") != exported["display_name"]:
        fail("exported application display name differs release inputs")
    if info.get("ITSAppUsesNonExemptEncryption") is not exported["declared_non_exempt_encryption"]:
        fail("exported application encryption declaration differs release inputs")

    privacy_path = app / exported["privacy_manifest_relative_path"]
    if load_plist(privacy_path) != load_plist(repo_root / exported["privacy_manifest_source"]):
        fail("exported privacy manifest differs source release input")
    bundled_manifest = app / exported["capability_manifest_relative_path"]
    if load_json(bundled_manifest) != manifest:
        fail("exported capability manifest differs source release input")

    extensions = list((app / "PlugIns").glob("*.appex"))
    matching_extensions = [
        extension
        for extension in extensions
        if load_plist(extension / "Info.plist").get("CFBundleIdentifier") == exported["live_activity_bundle_id"]
    ]
    if len(matching_extensions) != 1:
        fail("exported application must contain exactly one expected Live Activity extension")

    executable_name = load_plist(matching_extensions[0] / "Info.plist").get("CFBundleExecutable")
    executable = matching_extensions[0] / executable_name if isinstance(executable_name, str) else None
    if executable is None or not executable.is_file():
        fail("Live Activity extension executable is missing")
    if exported["live_activity_widget_kind"].encode() not in executable.read_bytes():
        fail("Live Activity extension executable does not contain expected widget kind")

    fragments = exported["forbidden_identity_fragments"]
    for plist_path in [app / "Info.plist", matching_extensions[0] / "Info.plist"]:
        raw = plist_path.read_bytes()
        for fragment in fragments:
            if fragment.encode() in raw:
                fail(f"forbidden product identity fragment {fragment!r} in {plist_path}")

    entitlements = (
        load_plist(entitlements_path)
        if entitlements_path
        else run_plist_command(["codesign", "-d", "--entitlements", ":-", str(app)], "signed app entitlements")
    )
    validate_entitlements(entitlements, exported["expected_entitlements"], "signed app", require_network=True)

    embedded_profile = app / "embedded.mobileprovision"
    profile = (
        load_plist(profile_path)
        if profile_path
        else run_plist_command(["security", "cms", "-D", "-i", str(embedded_profile)], "embedded provisioning profile")
    )
    profile_entitlements = profile.get("Entitlements")
    if not isinstance(profile_entitlements, dict):
        fail("provisioning profile has no Entitlements dictionary")
    validate_entitlements(
        profile_entitlements,
        exported["expected_entitlements"],
        "provisioning profile",
        require_network=False,
    )

    if require_public_approval:
        if not manifest["public_distribution_approved"]:
            fail("public distribution approval is not recorded")
        statuses = (
            exported["encryption_classification_status"],
            exported["provisioning_status"],
            exported["cloudkit_production_schema_status"],
        )
        if any(status != "approved" for status in statuses):
            fail("encryption, provisioning, and CloudKit production approvals are required")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(dest="command", required=True)

    manifest_parser = subparsers.add_parser("validate-manifest")
    manifest_parser.add_argument("--manifest", type=Path, required=True)
    manifest_parser.add_argument("--repo-root", type=Path, required=True)

    app_parser = subparsers.add_parser("validate-exported-app")
    app_parser.add_argument("--app", type=Path, required=True)
    app_parser.add_argument("--manifest", type=Path, required=True)
    app_parser.add_argument("--repo-root", type=Path, required=True)
    app_parser.add_argument("--entitlements-plist", type=Path)
    app_parser.add_argument("--profile-plist", type=Path)
    app_parser.add_argument("--require-public-approval", action="store_true")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        if args.command == "validate-manifest":
            validate_manifest(args.manifest.resolve(), args.repo_root.resolve())
            print("pass: Orlix application capability manifest")
        else:
            validate_exported_app(
                args.app.resolve(),
                args.manifest.resolve(),
                args.repo_root.resolve(),
                args.entitlements_plist.resolve() if args.entitlements_plist else None,
                args.profile_plist.resolve() if args.profile_plist else None,
                args.require_public_approval,
            )
            print(f"pass: exported Orlix application {args.app}")
    except GateError as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
