from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
from pathlib import Path
from typing import Any

import orlix_app_capability_gate as capability_gate


REPORT_SCHEMA_VERSION = 1
APPROVED_STATUS = "approved"
REQUIRED_APPROVAL_STATUSES = (
    "encryption_classification_status",
    "provisioning_status",
    "cloudkit_production_schema_status",
    "privacy_manifest_union_status",
    "package_license_notice_status",
)
REQUIRED_METADATA_FILES = (
    "name.txt",
    "subtitle.txt",
    "description.txt",
    "keywords.txt",
    "support_url.txt",
    "marketing_url.txt",
    "privacy_url.txt",
    "release_notes.txt",
)
SCREENSHOT_SUFFIXES = {".png", ".jpg", ".jpeg"}


class ReleaseCIError(ValueError):
    pass


def fail(message: str) -> None:
    raise ReleaseCIError(message)


def read_project_identity(project_path: Path) -> dict[str, str]:
    text = project_path.read_text(encoding="utf-8")
    values: dict[str, str] = {}
    for key in ("MARKETING_VERSION", "CURRENT_PROJECT_VERSION"):
        match = re.search(rf"^\s*{key}:\s*[\"']?([^\"'\s]+)[\"']?\s*$", text, re.MULTILINE)
        if match is None:
            fail(f"{key} is missing from {project_path}")
        values[key] = match.group(1)
    validate_build_number(values["CURRENT_PROJECT_VERSION"])
    return {
        "marketing_version": values["MARKETING_VERSION"],
        "project_build_number": values["CURRENT_PROJECT_VERSION"],
    }


def validate_build_number(value: str) -> int:
    if not re.fullmatch(r"[1-9][0-9]*", value):
        fail(f"build number must be a positive integer, got: {value}")
    return int(value)


def validate_release_tag(tag: str, marketing_version: str, build_number: str) -> None:
    validate_build_number(build_number)
    expected = f"ios-v{marketing_version}-b{build_number}"
    if tag != expected:
        fail(f"release tag must be {expected}, got: {tag}")


def validate_commit(value: str) -> None:
    if not re.fullmatch(r"[0-9a-f]{40}", value):
        fail(f"release commit must be a full lowercase Git SHA, got: {value}")


def validate_public_distribution_approval(manifest_path: Path, repo_root: Path) -> dict[str, Any]:
    manifest = capability_gate.validate_manifest(manifest_path, repo_root)
    if not manifest["public_distribution_approved"]:
        fail("public distribution approval is not recorded")
    exported = manifest["exported_product"]
    incomplete = [
        field
        for field in REQUIRED_APPROVAL_STATUSES
        if exported.get(field) != APPROVED_STATUS
    ]
    if incomplete:
        fail(f"public distribution approval statuses are incomplete: {', '.join(incomplete)}")
    return manifest


def validate_store_metadata(metadata_root: Path, screenshots_root: Path) -> None:
    locale_root = metadata_root / "en-US"
    missing = [
        str(locale_root / filename)
        for filename in REQUIRED_METADATA_FILES
        if not (locale_root / filename).is_file()
        or not (locale_root / filename).read_text(encoding="utf-8").strip()
    ]
    if missing:
        fail(f"missing required App Store metadata: {', '.join(missing)}")
    screenshots = [
        path
        for path in screenshots_root.rglob("*")
        if path.is_file() and path.suffix.lower() in SCREENSHOT_SUFFIXES
    ]
    if not screenshots:
        fail(f"no App Store screenshots found under {screenshots_root}")


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def build_release_report(
    *,
    repository: str,
    release_tag: str,
    commit: str,
    run_id: str,
    marketing_version: str,
    build_number: str,
    ipa_path: Path,
    tester_group: str,
    xcode_version: str,
    upload_backend: str,
) -> dict[str, Any]:
    validate_release_tag(release_tag, marketing_version, build_number)
    validate_commit(commit)
    validate_build_number(build_number)
    if not repository or "/" not in repository:
        fail("repository must use owner/name form")
    if not run_id.isdigit() or int(run_id) <= 0:
        fail(f"run ID must be a positive integer, got: {run_id}")
    if not ipa_path.is_file():
        fail(f"missing exported IPA: {ipa_path}")
    if not tester_group.strip():
        fail("TestFlight internal group is required")
    if not xcode_version.strip():
        fail("Xcode version is required")
    if not upload_backend.strip():
        fail("TestFlight upload backend is required")
    return {
        "schema_version": REPORT_SCHEMA_VERSION,
        "repository": repository,
        "release_tag": release_tag,
        "commit": commit,
        "github_run_id": int(run_id),
        "marketing_version": marketing_version,
        "build_number": build_number,
        "bundle_identifier": "com.rudironsoni.Orlix",
        "ipa": {
            "name": ipa_path.name,
            "sha256": sha256(ipa_path),
        },
        "xcode_version": xcode_version,
        "testflight": {
            "processing_state": "processed",
            "internal_group": tester_group,
            "upload_backend": upload_backend,
        },
    }


def validate_release_report(
    report_path: Path,
    *,
    repository: str,
    release_tag: str,
    commit: str,
    run_id: str,
) -> dict[str, Any]:
    report = json.loads(report_path.read_text(encoding="utf-8"))
    if report.get("schema_version") != REPORT_SCHEMA_VERSION:
        fail("unsupported release report schema")
    expected = {
        "repository": repository,
        "release_tag": release_tag,
        "commit": commit,
        "bundle_identifier": "com.rudironsoni.Orlix",
    }
    for field, value in expected.items():
        if report.get(field) != value:
            fail(f"release report {field} differs expected value")
    if report.get("github_run_id") != validate_build_number(run_id):
        fail("release report GitHub run ID differs expected value")
    validate_release_tag(
        report["release_tag"],
        report.get("marketing_version", ""),
        str(report.get("build_number", "")),
    )
    validate_commit(report["commit"])
    validate_build_number(str(report.get("build_number", "")))
    if report.get("testflight", {}).get("processing_state") != "processed":
        fail("release report does not record processed TestFlight state")
    if not report.get("testflight", {}).get("internal_group"):
        fail("release report does not record an internal TestFlight group")
    ipa_sha = report.get("ipa", {}).get("sha256", "")
    if not re.fullmatch(r"[0-9a-f]{64}", ipa_sha):
        fail("release report IPA SHA-256 is invalid")
    return report


def write_json(path: Path, value: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Orlix release CI policy")
    subparsers = parser.add_subparsers(dest="command", required=True)

    identity = subparsers.add_parser("project-identity")
    identity.add_argument("--project", type=Path, required=True)
    identity.add_argument("--output", type=Path, required=True)

    tag = subparsers.add_parser("tag-check")
    tag.add_argument("--project", type=Path, required=True)
    tag.add_argument("--tag", required=True)
    tag.add_argument("--commit", required=True)

    approval = subparsers.add_parser("approval-check")
    approval.add_argument("--manifest", type=Path, required=True)
    approval.add_argument("--repo-root", type=Path, required=True)

    metadata = subparsers.add_parser("metadata-check")
    metadata.add_argument("--metadata", type=Path, required=True)
    metadata.add_argument("--screenshots", type=Path, required=True)

    report_write = subparsers.add_parser("report-write")
    report_write.add_argument("--output", type=Path, required=True)
    report_write.add_argument("--repository", required=True)
    report_write.add_argument("--tag", required=True)
    report_write.add_argument("--commit", required=True)
    report_write.add_argument("--run-id", required=True)
    report_write.add_argument("--marketing-version", required=True)
    report_write.add_argument("--build-number", required=True)
    report_write.add_argument("--ipa", type=Path, required=True)
    report_write.add_argument("--tester-group", required=True)
    report_write.add_argument("--xcode-version", required=True)
    report_write.add_argument("--upload-backend", required=True)

    report_verify = subparsers.add_parser("report-verify")
    report_verify.add_argument("--report", type=Path, required=True)
    report_verify.add_argument("--repository", required=True)
    report_verify.add_argument("--tag", required=True)
    report_verify.add_argument("--commit", required=True)
    report_verify.add_argument("--run-id", required=True)
    report_verify.add_argument("--output", type=Path)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        if args.command == "project-identity":
            write_json(args.output, read_project_identity(args.project))
        elif args.command == "tag-check":
            identity = read_project_identity(args.project)
            validate_release_tag(
                args.tag,
                identity["marketing_version"],
                identity["project_build_number"],
            )
            validate_commit(args.commit)
        elif args.command == "approval-check":
            validate_public_distribution_approval(args.manifest, args.repo_root)
        elif args.command == "metadata-check":
            validate_store_metadata(args.metadata, args.screenshots)
        elif args.command == "report-write":
            write_json(
                args.output,
                build_release_report(
                    repository=args.repository,
                    release_tag=args.tag,
                    commit=args.commit,
                    run_id=args.run_id,
                    marketing_version=args.marketing_version,
                    build_number=args.build_number,
                    ipa_path=args.ipa,
                    tester_group=args.tester_group,
                    xcode_version=args.xcode_version,
                    upload_backend=args.upload_backend,
                ),
            )
        elif args.command == "report-verify":
            report = validate_release_report(
                args.report,
                repository=args.repository,
                release_tag=args.tag,
                commit=args.commit,
                run_id=args.run_id,
            )
            if args.output:
                write_json(
                    args.output,
                    {
                        "marketing_version": report["marketing_version"],
                        "build_number": report["build_number"],
                    },
                )
        return 0
    except (OSError, json.JSONDecodeError, ReleaseCIError, capability_gate.GateError) as error:
        print(f"release CI check failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
