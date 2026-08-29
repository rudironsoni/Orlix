from __future__ import annotations

import copy
import json
import tempfile
import unittest
from pathlib import Path
from unittest import mock


REPO_ROOT = Path(__file__).resolve().parents[2]

import orlix_release_ci as release_ci  # noqa: E402


MANIFEST = REPO_ROOT / "docs/sources/release/orlix-app-release-inputs.json"


class ReleaseCITests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.temp = Path(self.temporary.name)

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def test_repository_project_identity_is_valid(self) -> None:
        identity = release_ci.read_project_identity(REPO_ROOT / "project.yml")
        self.assertEqual(identity["marketing_version"], "0.1")
        self.assertRegex(identity["project_build_number"], r"^[1-9][0-9]*$")

    def test_release_tag_must_match_marketing_version_and_build_number(self) -> None:
        release_ci.validate_release_tag("ios-v0.1-b42", "0.1", "42")
        for tag in ("ios-v0.1", "ios-v0.2-b42", "ios-v0.1-b43"):
            with self.subTest(tag=tag), self.assertRaisesRegex(
                release_ci.ReleaseCIError, "release tag must be"
            ):
                release_ci.validate_release_tag(tag, "0.1", "42")

    def test_build_number_must_be_positive_integer(self) -> None:
        self.assertEqual(release_ci.validate_build_number("43"), 43)
        for value in ("", "0", "-1", "1.2"):
            with self.subTest(value=value), self.assertRaises(release_ci.ReleaseCIError):
                release_ci.validate_build_number(value)

    def test_repository_public_distribution_is_blocked(self) -> None:
        value = json.loads(MANIFEST.read_text(encoding="utf-8"))
        with mock.patch.object(release_ci.capability_gate, "validate_manifest", return_value=value):
            with self.assertRaisesRegex(release_ci.ReleaseCIError, "approval is not recorded"):
                release_ci.validate_public_distribution_approval(MANIFEST, REPO_ROOT)

    def test_approved_distribution_requires_all_statuses(self) -> None:
        value = json.loads(MANIFEST.read_text(encoding="utf-8"))
        value["public_distribution_approved"] = True
        path = self.temp / "manifest.json"
        path.write_text(json.dumps(value), encoding="utf-8")
        with mock.patch.object(release_ci.capability_gate, "validate_manifest", return_value=value):
            with self.assertRaisesRegex(release_ci.ReleaseCIError, "statuses are incomplete"):
                release_ci.validate_public_distribution_approval(path, REPO_ROOT)

        approved = copy.deepcopy(value)
        for field in release_ci.REQUIRED_APPROVAL_STATUSES:
            approved["exported_product"][field] = "approved"
        path.write_text(json.dumps(approved), encoding="utf-8")
        with mock.patch.object(release_ci.capability_gate, "validate_manifest", return_value=approved):
            release_ci.validate_public_distribution_approval(path, REPO_ROOT)

    def test_store_metadata_requires_text_and_screenshot(self) -> None:
        metadata = self.temp / "metadata/en-US"
        screenshots = self.temp / "screenshots/en-US"
        metadata.mkdir(parents=True)
        screenshots.mkdir(parents=True)
        for filename in release_ci.REQUIRED_METADATA_FILES:
            (metadata / filename).write_text(filename, encoding="utf-8")
        with self.assertRaisesRegex(release_ci.ReleaseCIError, "no App Store screenshots"):
            release_ci.validate_store_metadata(metadata.parent, screenshots.parent)
        (screenshots / "01-terminal.png").write_bytes(b"png")
        release_ci.validate_store_metadata(metadata.parent, screenshots.parent)

    def test_release_report_binds_exact_beta(self) -> None:
        ipa = self.temp / "Orlix.ipa"
        ipa.write_bytes(b"signed archive")
        report = release_ci.build_release_report(
            repository="rudironsoni/Orlix",
            release_tag="ios-v0.1-b43",
            commit="a" * 40,
            run_id="123",
            marketing_version="0.1",
            build_number="43",
            ipa_path=ipa,
            tester_group="Internal Testers",
            xcode_version="26.6",
            upload_backend="appstore-api",
        )
        path = self.temp / "release.json"
        release_ci.write_json(path, report)
        verified = release_ci.validate_release_report(
            path,
            repository="rudironsoni/Orlix",
            release_tag="ios-v0.1-b43",
            commit="a" * 40,
            run_id="123",
        )
        self.assertEqual(verified["build_number"], "43")
        with self.assertRaisesRegex(release_ci.ReleaseCIError, "commit differs"):
            release_ci.validate_release_report(
                path,
                repository="rudironsoni/Orlix",
                release_tag="ios-v0.1-b43",
                commit="b" * 40,
                run_id="123",
            )


if __name__ == "__main__":
    unittest.main()
