#!/usr/bin/env python3

from __future__ import annotations

import copy
import json
import plistlib
import sys
import tempfile
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO_ROOT / "tools/release"))

import orlix_app_capability_gate as gate  # noqa: E402


MANIFEST = REPO_ROOT / "docs/sources/release/orlix-app-release-inputs.json"


class CapabilityGateTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.temp = Path(self.temporary.name)
        self.manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def write_manifest(self, value: dict, name: str = "manifest.json") -> Path:
        path = self.temp / name
        path.write_text(json.dumps(value), encoding="utf-8")
        return path

    def write_plist(self, path: Path, value: dict) -> None:
        path.parent.mkdir(parents=True, exist_ok=True)
        with path.open("wb") as handle:
            plistlib.dump(value, handle)

    def expected_entitlements(self) -> dict:
        return {
            "application-identifier": "TESTTEAM.com.rudironsoni.Orlix",
            "keychain-access-groups": ["TESTTEAM.com.rudironsoni.Orlix"],
            "com.apple.developer.icloud-container-identifiers": ["iCloud.com.rudironsoni.Orlix"],
            "com.apple.developer.icloud-services": ["CloudKit"],
            "com.apple.developer.ubiquity-kvstore-identifier": "TESTTEAM.com.rudironsoni.Orlix",
            "com.apple.security.network.client": True,
            "com.apple.security.network.server": True,
            "aps-environment": "production",
        }

    def make_exported_app(self) -> tuple[Path, Path, Path]:
        app = self.temp / "Orlix.app"
        self.write_plist(
            app / "Info.plist",
            {
                "CFBundleIdentifier": "com.rudironsoni.Orlix",
                "CFBundleDisplayName": "Orlix",
                "ITSAppUsesNonExemptEncryption": False,
            },
        )
        privacy_source = REPO_ROOT / self.manifest["exported_product"]["privacy_manifest_source"]
        (app / "PrivacyInfo.xcprivacy").write_bytes(privacy_source.read_bytes())
        (app / "ORLIX_APP_RELEASE_INPUTS.json").write_bytes(MANIFEST.read_bytes())

        extension = app / "PlugIns/OrlixLiveActivity.appex"
        self.write_plist(
            extension / "Info.plist",
            {
                "CFBundleIdentifier": "com.rudironsoni.Orlix.liveactivity",
                "CFBundleExecutable": "OrlixLiveActivity",
            },
        )
        (extension / "OrlixLiveActivity").write_bytes(b"com.rudironsoni.Orlix.liveactivity")

        entitlements = self.temp / "entitlements.plist"
        profile = self.temp / "profile.plist"
        self.write_plist(entitlements, self.expected_entitlements())
        profile_entitlements = self.expected_entitlements()
        profile_entitlements.pop("com.apple.security.network.client")
        profile_entitlements.pop("com.apple.security.network.server")
        self.write_plist(profile, {"Entitlements": profile_entitlements})
        return app, entitlements, profile

    def validate_app(self, app: Path, entitlements: Path, profile: Path, *, public: bool = False) -> None:
        gate.validate_exported_app(app, MANIFEST, REPO_ROOT, entitlements, profile, public)

    def test_repository_manifest_is_valid(self) -> None:
        gate.validate_manifest(MANIFEST, REPO_ROOT)

    def test_missing_required_capability_fails(self) -> None:
        value = copy.deepcopy(self.manifest)
        value["capabilities"].pop()
        with self.assertRaisesRegex(gate.GateError, "capability inventory mismatch"):
            gate.validate_manifest(self.write_manifest(value), REPO_ROOT)

    def test_package_resolution_hash_drift_fails(self) -> None:
        value = copy.deepcopy(self.manifest)
        value["swift_package_resolution"]["sha256"] = "0" * 64
        with self.assertRaisesRegex(gate.GateError, "package resolution SHA-256 differs"):
            gate.validate_manifest(self.write_manifest(value), REPO_ROOT)

    def test_privacy_required_reason_union_drift_fails(self) -> None:
        value = copy.deepcopy(self.manifest)
        value["exported_product"]["privacy_required_reason_union"].pop(
            "NSPrivacyAccessedAPICategoryFileTimestamp"
        )
        with self.assertRaisesRegex(gate.GateError, "required-reason declarations differ"):
            gate.validate_manifest(self.write_manifest(value), REPO_ROOT)

    def test_forbidden_identity_in_product_input_fails(self) -> None:
        value = copy.deepcopy(self.manifest)
        value["exported_product"]["forbidden_identity_fragments"].append("OrlixApp")
        with self.assertRaisesRegex(gate.GateError, "forbidden product identity fragment"):
            gate.validate_manifest(self.write_manifest(value), REPO_ROOT)

    def test_valid_exported_app_passes(self) -> None:
        self.validate_app(*self.make_exported_app())

    def test_encryption_declaration_mismatch_fails(self) -> None:
        app, entitlements, profile = self.make_exported_app()
        info = plistlib.loads((app / "Info.plist").read_bytes())
        info["ITSAppUsesNonExemptEncryption"] = True
        self.write_plist(app / "Info.plist", info)
        with self.assertRaisesRegex(gate.GateError, "encryption declaration differs"):
            self.validate_app(app, entitlements, profile)

    def test_missing_privacy_manifest_fails(self) -> None:
        app, entitlements, profile = self.make_exported_app()
        (app / "PrivacyInfo.xcprivacy").unlink()
        with self.assertRaisesRegex(gate.GateError, "cannot read plist"):
            self.validate_app(app, entitlements, profile)

    def test_provisioning_mismatch_fails(self) -> None:
        app, entitlements, profile = self.make_exported_app()
        bad_profile = {"Entitlements": self.expected_entitlements()}
        bad_profile["Entitlements"].pop("com.apple.security.network.client")
        bad_profile["Entitlements"].pop("com.apple.security.network.server")
        bad_profile["Entitlements"]["aps-environment"] = "development"
        self.write_plist(profile, bad_profile)
        with self.assertRaisesRegex(gate.GateError, "aps-environment differs"):
            self.validate_app(app, entitlements, profile)

    def test_public_gate_fails_without_recorded_approvals(self) -> None:
        app, entitlements, profile = self.make_exported_app()
        with self.assertRaisesRegex(gate.GateError, "public distribution approval is not recorded"):
            self.validate_app(app, entitlements, profile, public=True)


if __name__ == "__main__":
    unittest.main()
