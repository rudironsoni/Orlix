# Release validation implementation. Public workflows enter through the root Makefile.

ORLIX_RELEASE_SUPPORT_DIR := $(CURDIR)/make/release
ORLIX_RELEASE_MANIFEST ?= $(CURDIR)/docs/sources/release/orlix-app-release-inputs.json
ORLIX_EXPORTED_APP ?= $(ORLIX_BETA_ARCHIVE_PATH)/Products/Applications/Orlix.app
ORLIX_RELEASE_IDENTITY_PATH ?= $(ORLIX_BETA_ARCHIVE_DIR)/release-identity.json
ORLIX_RELEASE_REPORT_PATH ?= $(ORLIX_BUILD_ROOT)/AgentHarness/orlix-release/release.json
ORLIX_RELEASE_REPORT_INPUT ?= $(ORLIX_RELEASE_REPORT_PATH)
ORLIX_RELEASE_REPORT_SELECTION ?= $(ORLIX_BUILD_ROOT)/AgentHarness/orlix-release/selection.json
ORLIX_RELEASE_REPOSITORY ?=
ORLIX_RELEASE_TAG ?=
ORLIX_RELEASE_COMMIT ?=
ORLIX_RELEASE_RUN_ID ?=
ORLIX_RELEASE_XCODE_VERSION ?=
ORLIX_RELEASE_UPLOAD_BACKEND ?= appstore-api
ORLIX_TESTFLIGHT_INTERNAL_GROUP ?=
ORLIX_STORE_METADATA_PATH ?= $(CURDIR)/fastlane/metadata
ORLIX_STORE_SCREENSHOTS_PATH ?= $(CURDIR)/fastlane/screenshots
ORLIX_REQUIRE_PUBLIC_DISTRIBUTION ?= false

.PHONY: __release-manifest-check __release-inputs-check __exported-app-check __release-public-approval-check __release-tag-check __release-metadata-check __release-report-write __release-report-check __release-tests __release-workflow-tests

__release-manifest-check:
	@PYTHONPATH="$(ORLIX_RELEASE_SUPPORT_DIR)" \
		ORLIX_RELEASE_MANIFEST="$(ORLIX_RELEASE_MANIFEST)" \
		ORLIX_REPO_ROOT="$(CURDIR)" \
		python3 -c 'import os; from pathlib import Path; import orlix_app_capability_gate as gate; gate.validate_manifest(Path(os.environ["ORLIX_RELEASE_MANIFEST"]).resolve(), Path(os.environ["ORLIX_REPO_ROOT"]).resolve()); print("pass: Orlix application capability manifest")'

__exported-app-check:
	@PYTHONPATH="$(ORLIX_RELEASE_SUPPORT_DIR)" \
		ORLIX_EXPORTED_APP="$(ORLIX_EXPORTED_APP)" \
		ORLIX_RELEASE_MANIFEST="$(ORLIX_RELEASE_MANIFEST)" \
		ORLIX_REPO_ROOT="$(CURDIR)" \
		ORLIX_REQUIRE_PUBLIC_DISTRIBUTION="$(ORLIX_REQUIRE_PUBLIC_DISTRIBUTION)" \
		python3 -c 'import os; from pathlib import Path; import orlix_app_capability_gate as gate; app = Path(os.environ["ORLIX_EXPORTED_APP"]).resolve(); required = os.environ["ORLIX_REQUIRE_PUBLIC_DISTRIBUTION"].lower() == "true"; gate.validate_exported_app(app, Path(os.environ["ORLIX_RELEASE_MANIFEST"]).resolve(), Path(os.environ["ORLIX_REPO_ROOT"]).resolve(), None, None, required); print(f"pass: exported Orlix application {app}")'

__release-public-approval-check:
	@PYTHONPATH="$(ORLIX_RELEASE_SUPPORT_DIR)" python3 -m orlix_release_ci approval-check \
		--manifest "$(ORLIX_RELEASE_MANIFEST)" \
		--repo-root "$(CURDIR)"

__release-tag-check:
	@set -euo pipefail; \
	[ -n "$(ORLIX_RELEASE_TAG)" ] || { echo "ORLIX_RELEASE_TAG is required" >&2; exit 1; }; \
	[ -n "$(ORLIX_RELEASE_COMMIT)" ] || { echo "ORLIX_RELEASE_COMMIT is required" >&2; exit 1; }; \
	PYTHONPATH="$(ORLIX_RELEASE_SUPPORT_DIR)" python3 -m orlix_release_ci tag-check --project project.yml --tag "$(ORLIX_RELEASE_TAG)" --commit "$(ORLIX_RELEASE_COMMIT)"; \
	[ "$$(git rev-parse HEAD)" = "$(ORLIX_RELEASE_COMMIT)" ] || { echo "checked out commit differs from ORLIX_RELEASE_COMMIT" >&2; exit 1; }; \
	[ "$$(git rev-list -n 1 "refs/tags/$(ORLIX_RELEASE_TAG)")" = "$(ORLIX_RELEASE_COMMIT)" ] || { echo "release tag does not point to ORLIX_RELEASE_COMMIT" >&2; exit 1; }; \
	git show-ref --verify --quiet refs/remotes/origin/main || { echo "origin/main is required to validate the release tag" >&2; exit 1; }; \
	git merge-base --is-ancestor "$(ORLIX_RELEASE_COMMIT)" refs/remotes/origin/main || { echo "release commit is not on origin/main" >&2; exit 1; }; \
	echo "pass: protected release tag $(ORLIX_RELEASE_TAG)"

__release-metadata-check:
	@PYTHONPATH="$(ORLIX_RELEASE_SUPPORT_DIR)" python3 -m orlix_release_ci metadata-check \
		--metadata "$(ORLIX_STORE_METADATA_PATH)" \
		--screenshots "$(ORLIX_STORE_SCREENSHOTS_PATH)"

__release-report-write:
	@set -euo pipefail; \
	[ -f "$(ORLIX_RELEASE_IDENTITY_PATH)" ] || { echo "missing release identity: $(ORLIX_RELEASE_IDENTITY_PATH)" >&2; exit 1; }; \
	[ -f "$(ORLIX_BETA_BUILD_NUMBER_FILE)" ] || { echo "missing beta build number: $(ORLIX_BETA_BUILD_NUMBER_FILE)" >&2; exit 1; }; \
	[ -n "$(ORLIX_RELEASE_REPOSITORY)" ] || { echo "ORLIX_RELEASE_REPOSITORY is required" >&2; exit 1; }; \
	[ -n "$(ORLIX_RELEASE_TAG)" ] || { echo "ORLIX_RELEASE_TAG is required" >&2; exit 1; }; \
	[ -n "$(ORLIX_RELEASE_COMMIT)" ] || { echo "ORLIX_RELEASE_COMMIT is required" >&2; exit 1; }; \
	[ -n "$(ORLIX_RELEASE_RUN_ID)" ] || { echo "ORLIX_RELEASE_RUN_ID is required" >&2; exit 1; }; \
	[ -n "$(ORLIX_RELEASE_XCODE_VERSION)" ] || { echo "ORLIX_RELEASE_XCODE_VERSION is required" >&2; exit 1; }; \
	[ -n "$(ORLIX_TESTFLIGHT_INTERNAL_GROUP)" ] || { echo "ORLIX_TESTFLIGHT_INTERNAL_GROUP is required" >&2; exit 1; }; \
	marketing="$$(jq -r '.marketing_version' "$(ORLIX_RELEASE_IDENTITY_PATH)")"; \
	build="$$(tr -d '[:space:]' < "$(ORLIX_BETA_BUILD_NUMBER_FILE)")"; \
	PYTHONPATH="$(ORLIX_RELEASE_SUPPORT_DIR)" python3 -m orlix_release_ci report-write \
		--output "$(ORLIX_RELEASE_REPORT_PATH)" --repository "$(ORLIX_RELEASE_REPOSITORY)" \
		--tag "$(ORLIX_RELEASE_TAG)" --commit "$(ORLIX_RELEASE_COMMIT)" --run-id "$(ORLIX_RELEASE_RUN_ID)" \
		--marketing-version "$$marketing" --build-number "$$build" --ipa "$(ORLIX_BETA_IPA_PATH)" \
		--tester-group "$(ORLIX_TESTFLIGHT_INTERNAL_GROUP)" --xcode-version "$(ORLIX_RELEASE_XCODE_VERSION)" \
		--upload-backend "$(ORLIX_RELEASE_UPLOAD_BACKEND)"

__release-report-check:
	@PYTHONPATH="$(ORLIX_RELEASE_SUPPORT_DIR)" python3 -m orlix_release_ci report-verify \
		--report "$(ORLIX_RELEASE_REPORT_INPUT)" --repository "$(ORLIX_RELEASE_REPOSITORY)" \
		--tag "$(ORLIX_RELEASE_TAG)" --commit "$(ORLIX_RELEASE_COMMIT)" --run-id "$(ORLIX_RELEASE_RUN_ID)" \
		--output "$(ORLIX_RELEASE_REPORT_SELECTION)"

__release-inputs-check: __release-manifest-check
	@set -euo pipefail; \
	for command in cmp jq python3 shasum xcodegen; do \
		command -v "$$command" >/dev/null 2>&1 || { echo "missing required command: $$command" >&2; exit 1; }; \
	done; \
	manifest="$(ORLIX_RELEASE_MANIFEST)"; \
	[ -f "$$manifest" ] || { echo "missing release-input manifest: $$manifest" >&2; exit 1; }; \
	jq -e '.schema_version == 2' "$$manifest" >/dev/null || { echo "unsupported release-input manifest schema" >&2; exit 1; }; \
	jq -e '(.swift_packages | length > 0) and all(.swift_packages[]; (.revision | test("^[0-9a-f]{40}$$")) and (.url | type == "string" and length > 0))' "$$manifest" >/dev/null || { echo "every Swift package must use a full commit and URL" >&2; exit 1; }; \
	tmp="$$(mktemp -d "$${TMPDIR:-/tmp}/orlix-release-inputs.XXXXXX")"; \
	trap 'rm -rf "$$tmp"' EXIT; \
	xcodegen dump --spec project.yml --type json > "$$tmp/project.json"; \
	jq -e '.options.deploymentTarget.iOS == "15.0"' "$$tmp/project.json" >/dev/null || { echo "Orlix global iOS deployment target must be 15.0" >&2; exit 1; }; \
	for target in OrlixKernel OrlixMLibC OrlixCoreUtils OrlixOS Orlix OrlixTestApp OrlixOSTestApp; do \
		jq -e --arg target "$$target" '.targets[$$target].settings.base.IPHONEOS_DEPLOYMENT_TARGET == "15.0"' "$$tmp/project.json" >/dev/null || { echo "$$target deployment target must be 15.0" >&2; exit 1; }; \
	done; \
	jq -e '.targets.OrlixLiveActivity.settings.base.IPHONEOS_DEPLOYMENT_TARGET == "16.1"' "$$tmp/project.json" >/dev/null || { echo "Orlix Live Activity deployment target must be 16.1" >&2; exit 1; }; \
	jq -S '.packages' "$$tmp/project.json" > "$$tmp/project-packages.json"; \
	jq -S '.swift_packages' "$$manifest" > "$$tmp/manifest-packages.json"; \
	if ! cmp -s "$$tmp/project-packages.json" "$$tmp/manifest-packages.json"; then \
		echo "project.yml package definitions differ from immutable release inputs" >&2; \
		diff -u "$$tmp/manifest-packages.json" "$$tmp/project-packages.json" >&2 || true; \
		exit 1; \
	fi; \
	ghostty_commit="$$(jq -r '.native_sources.ghostty.commit' "$$manifest")"; \
	openssl_version="$$(jq -r '.native_sources.openssl.version' "$$manifest")"; \
	openssl_sha256="$$(jq -r '.native_sources.openssl.archive_sha256' "$$manifest")"; \
	libssh2_version="$$(jq -r '.native_sources.libssh2.version' "$$manifest")"; \
	libssh2_sha256="$$(jq -r '.native_sources.libssh2.archive_sha256' "$$manifest")"; \
	[ "$$(tr -d '[:space:]' < Orlix/Vendor/libghostty/VERSION)" = "$$ghostty_commit" ] || { echo "Ghostty VERSION does not match immutable release inputs" >&2; exit 1; }; \
	grep -Fq 'GHOSTTY_REF="$$$${GHOSTTY_REF:-' Orlix/make/vendor.mk && grep -Fq "$$ghostty_commit" Orlix/make/vendor.mk || { echo "Ghostty build default is not pinned" >&2; exit 1; }; \
	grep -Fq "OPENSSL_VERSION=\"$$openssl_version\"" Orlix/make/vendor.mk || { echo "OpenSSL build version differs from release inputs" >&2; exit 1; }; \
	grep -Fq "OPENSSL_SHA256=\"$$openssl_sha256\"" Orlix/make/vendor.mk || { echo "OpenSSL source hash differs from release inputs" >&2; exit 1; }; \
	grep -Fq "LIBSSH2_VERSION=\"$$libssh2_version\"" Orlix/make/vendor.mk || { echo "libssh2 build version differs from release inputs" >&2; exit 1; }; \
	grep -Fq "LIBSSH2_SHA256=\"$$libssh2_sha256\"" Orlix/make/vendor.mk || { echo "libssh2 source hash differs from release inputs" >&2; exit 1; }; \
	while IFS=$$'\t' read -r path expected_sha256; do \
		[ -f "$$path" ] || { echo "missing vendored artifact: $$path" >&2; exit 1; }; \
		actual_sha256="$$(shasum -a 256 "$$path" | awk '{print $$1}')"; \
		[ "$$actual_sha256" = "$$expected_sha256" ] || { echo "vendored artifact hash mismatch: $$path" >&2; exit 1; }; \
	done < <(jq -r '.vendor_artifacts | to_entries[] | [.key, .value] | @tsv' "$$manifest"); \
	while IFS= read -r path; do \
		[ -f "$$path" ] || { echo "missing required release evidence: $$path" >&2; exit 1; }; \
	done < <(jq -r '.required_evidence[]' "$$manifest"); \
	echo "pass: Orlix application release inputs"

__release-tests:
	@PYTHONPATH="$(ORLIX_RELEASE_SUPPORT_DIR)" python3 -m unittest discover -s "$(ORLIX_RELEASE_SUPPORT_DIR)" -p 'test_*.py'
	@$(MAKE) --no-print-directory __release-inputs-check
	@set -euo pipefail; \
	tmp="$$(mktemp -d "$${TMPDIR:-/tmp}/orlix-release-inputs-test.XXXXXX")"; \
	trap 'rm -rf "$$tmp"' EXIT; \
	jq '.swift_packages.OpenTelemetrySwift.revision = "0000000000000000000000000000000000000000"' "$(ORLIX_RELEASE_MANIFEST)" > "$$tmp/bad-manifest.json"; \
	if $(MAKE) --no-print-directory __release-inputs-check ORLIX_RELEASE_MANIFEST="$$tmp/bad-manifest.json" > "$$tmp/stdout" 2> "$$tmp/stderr"; then \
		echo "expected mismatched package manifest to fail" >&2; exit 1; \
	fi; \
	grep -Fq "project.yml package definitions differ from immutable release inputs" "$$tmp/stderr"; \
	if GHOSTTY_REF=custom-io $(MAKE) --no-print-directory -f Orlix/Makefile build type=vendor vendor=ghostty > "$$tmp/stdout" 2> "$$tmp/stderr"; then \
		echo "expected branch-only Ghostty ref to fail" >&2; exit 1; \
	fi; \
	grep -Fq "GHOSTTY_REF must be a full 40-character commit" "$$tmp/stdout"; \
	echo "pass: release-input checks"

__release-workflow-tests:
	@PYTHONPATH="$(ORLIX_RELEASE_SUPPORT_DIR)" python3 -m unittest discover -s "$(ORLIX_RELEASE_SUPPORT_DIR)" -p 'test_*.py'
	@command -v actionlint >/dev/null 2>&1 || { echo "actionlint is required; run: brew bundle --file Brewfile" >&2; exit 1; }
	@actionlint .github/workflows/testflight-beta.yml .github/workflows/app-store-review.yml
	@rg -q 'actions/checkout@[0-9a-f]{40}' .github/workflows/testflight-beta.yml
	@rg -q 'automatic_release: false' fastlane/Fastfile
	@echo "pass: release workflow policy"
