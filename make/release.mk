# Release validation implementation. Public workflows enter through the root Makefile.

ORLIX_RELEASE_SUPPORT_DIR := $(CURDIR)/make/release
ORLIX_RELEASE_MANIFEST ?= $(CURDIR)/docs/sources/release/orlix-app-release-inputs.json
ORLIX_EXPORTED_APP ?= $(ORLIX_BETA_ARCHIVE_PATH)/Products/Applications/Orlix.app

.PHONY: __release-manifest-check __release-inputs-check __exported-app-check __release-tests

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
		python3 -c 'import os; from pathlib import Path; import orlix_app_capability_gate as gate; app = Path(os.environ["ORLIX_EXPORTED_APP"]).resolve(); gate.validate_exported_app(app, Path(os.environ["ORLIX_RELEASE_MANIFEST"]).resolve(), Path(os.environ["ORLIX_REPO_ROOT"]).resolve(), None, None, False); print(f"pass: exported Orlix application {app}")'

__release-inputs-check: __release-manifest-check
	@set -euo pipefail; \
	for command in cmp jq python3 shasum xcodegen; do \
		command -v "$$command" >/dev/null 2>&1 || { echo "missing required command: $$command" >&2; exit 1; }; \
	done; \
	manifest="$(ORLIX_RELEASE_MANIFEST)"; \
	[ -f "$$manifest" ] || { echo "missing release-input manifest: $$manifest" >&2; exit 1; }; \
	jq -e '.schema_version == 1' "$$manifest" >/dev/null || { echo "unsupported release-input manifest schema" >&2; exit 1; }; \
	jq -e '(.swift_packages | length > 0) and all(.swift_packages[]; (.revision | test("^[0-9a-f]{40}$$")) and (.url | type == "string" and length > 0))' "$$manifest" >/dev/null || { echo "every Swift package must use a full commit and URL" >&2; exit 1; }; \
	tmp="$$(mktemp -d "$${TMPDIR:-/tmp}/orlix-release-inputs.XXXXXX")"; \
	trap 'rm -rf "$$tmp"' EXIT; \
	xcodegen dump --spec project.yml --type json > "$$tmp/project.json"; \
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
