ORLIX_BAZEL_CACHE_ROOT ?= $(HOME)/Library/Caches/Orlix/Bazel
ORLIX_BAZEL_VERSION ?= 9.2.0
ORLIX_XCODE_VERSION ?= 26.6
ORLIX_XCODE_BUILD ?= 17F113
ORLIX_BAZEL_DISK_CACHE ?= $(ORLIX_BAZEL_CACHE_ROOT)/disk-cache/bazel-$(ORLIX_BAZEL_VERSION)-xcode-$(ORLIX_XCODE_BUILD)
ORLIX_BAZEL_REPOSITORY_CACHE ?= $(ORLIX_BAZEL_CACHE_ROOT)/repository-cache
ORLIX_BAZEL_OUTPUT_BASE ?= $(ORLIX_BUILD_ROOT)/Bazel/output-base
ORLIX_KBUILD_PERSIST ?= $(ORLIX_BUILD_ROOT)/Bazel/kbuild-persist
ORLIX_BAZEL_TOOL_ROOT ?= $(HOME)/Library/Caches/Orlix/Tools/bazel
ORLIX_BAZEL ?= $(ORLIX_BAZEL_TOOL_ROOT)/$(ORLIX_BAZEL_VERSION)/bazel
ORLIX_PINNED_DEVELOPER_DIR ?= /Applications/Xcode-26.6.0.app/Contents/Developer
CCACHE_BASEDIR ?= $(CURDIR)
CCACHE_DIR ?= $(HOME)/Library/Caches/Orlix/ccache
CCACHE_MAXSIZE ?= 20G
CCACHE_COMPILERCHECK ?= content

export ORLIX_BAZEL_CACHE_ROOT
export ORLIX_BAZEL_VERSION
export ORLIX_XCODE_VERSION
export ORLIX_XCODE_BUILD
export ORLIX_BAZEL_DISK_CACHE
export ORLIX_BAZEL_REPOSITORY_CACHE
export ORLIX_BAZEL_OUTPUT_BASE
export ORLIX_KBUILD_PERSIST
export ORLIX_BAZEL_TOOL_ROOT
export CCACHE_BASEDIR
export CCACHE_DIR
export CCACHE_MAXSIZE
export CCACHE_COMPILERCHECK

.PHONY: __bazel-bootstrap __bazel-version-check __bazel-server-restart __bazel-module-lock-update __bazel-feasibility-bootstrap __bazel-apple-smoke
.PHONY: __bazel-apple-dependency-smoke __bazel-native-archives
.PHONY: __bazel-ghostty-archives __bazel-ssh-archives
.PHONY: __bazel-native-dependency-smoke __bazel-feasibility-xcodeproj
.PHONY: __bazel-kernel-uapi __bazel-kernel-uapi-variants __bazel-mlibc-from-uapi __bazel-live-activity-smoke __bazel-promote-uapi
.PHONY: __bazel-guest-package __bazel-coreutils __bazel-bash __bazel-grep __bazel-findutils __bazel-e2fsprogs
.PHONY: __bazel-getconf __bazel-getent __bazel-init __bazel-jq __bazel-curl __bazel-ncurses __bazel-zsh __bazel-rootfs
.PHONY: __bazel-xcode-cloud-project-check
.PHONY: __bazel-migration-inventory __bazel-migration-inventory-check
.PHONY: __bazel-matrix-check

__bazel-bootstrap:
	@ruby bazel/bootstrap.rb >/dev/null

__bazel-version-check: __bazel-bootstrap
	@test "$$($(ORLIX_BAZEL) --version)" = "bazel 9.2.0" || { echo "Bazel 9.2.0 is required" >&2; exit 1; }

__bazel-server-restart: __bazel-version-check
	@DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" shutdown

__bazel-module-lock-update: __bazel-version-check
	@mkdir -p "$(ORLIX_BAZEL_REPOSITORY_CACHE)" "$(ORLIX_BAZEL_OUTPUT_BASE)"
	@DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" mod deps --lockfile_mode=update --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)"

__bazel-feasibility-bootstrap: __bazel-version-check __bazel-migration-inventory-check
	@test -d "$(ORLIX_PINNED_DEVELOPER_DIR)" || { echo "missing pinned Xcode developer directory: $(ORLIX_PINNED_DEVELOPER_DIR)" >&2; exit 1; }
	@PYTHONPATH="$(CURDIR)/bazel/config" ORLIX_XCODE_VERSION="$(ORLIX_XCODE_VERSION)" ORLIX_XCODE_BUILD="$(ORLIX_XCODE_BUILD)" ORLIX_BAZEL_DISK_CACHE="$(ORLIX_BAZEL_DISK_CACHE)" python3 -c 'import os, toolchain_pin as pin; pin.require_identity(os.environ["ORLIX_XCODE_VERSION"], os.environ["ORLIX_XCODE_BUILD"], os.environ["ORLIX_BAZEL_DISK_CACHE"])'
	@test "$$(DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" /usr/bin/xcodebuild -version | /usr/bin/sed -n '1p')" = "Xcode $(ORLIX_XCODE_VERSION)" || { echo "DEVELOPER_DIR is not Xcode $(ORLIX_XCODE_VERSION)" >&2; exit 1; }
	@test "$$(DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" /usr/bin/xcodebuild -version | /usr/bin/sed -n '2p')" = "Build version $(ORLIX_XCODE_BUILD)" || { echo "DEVELOPER_DIR is not Build version $(ORLIX_XCODE_BUILD)" >&2; exit 1; }
	@mkdir -p "$(ORLIX_BAZEL_DISK_CACHE)" "$(ORLIX_BAZEL_REPOSITORY_CACHE)" "$(ORLIX_BAZEL_OUTPUT_BASE)"
	@DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" mod deps --lockfile_mode=error --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)"
	@DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" build //bazel/config:all --config=release --config=source --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)"

__bazel-apple-smoke: __bazel-feasibility-bootstrap
	@DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" build //bazel/feasibility/apple:SmokeApp --compilation_mode=dbg --config=release --config=source --ios_multi_cpus=sim_arm64 --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)"

__bazel-apple-dependency-smoke: __bazel-feasibility-bootstrap
	@DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" build //bazel/feasibility/apple:DependencySmokeApp --compilation_mode=dbg --config=release --config=source --ios_multi_cpus=sim_arm64 --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)"

__bazel-native-archives: __bazel-feasibility-bootstrap
	@DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" build //bazel/feasibility/native:apple_archives --config=release --config=source --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)"

__bazel-ghostty-archives: __bazel-feasibility-bootstrap
	@DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" build //bazel/feasibility/native:ghostty_archives --config=release --config=source --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)"

__bazel-ssh-archives: __bazel-feasibility-bootstrap
	@DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" build //bazel/feasibility/native:ssh_archives --config=release --config=source --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)"

__bazel-native-dependency-smoke: __bazel-native-archives
	@DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" build //bazel/feasibility/apple:NativeDependencySmokeApp --compilation_mode=dbg --config=release --config=source --ios_multi_cpus=sim_arm64 --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)"
	@DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" build //bazel/feasibility/apple:NativeDependencySmokeApp --compilation_mode=dbg --config=release --config=source --ios_multi_cpus=arm64 --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)"
	@DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" build //bazel/feasibility/apple:NativeDependencyMacSmokeApp --compilation_mode=dbg --config=release --config=source --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)"

__bazel-kernel-uapi: __bazel-feasibility-bootstrap
	@mkdir -p "$(ORLIX_KBUILD_PERSIST)"
	@DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" build //bazel/feasibility/kernel:uapi --config=release --config=source --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_KBUILD_PERSIST="$(ORLIX_KBUILD_PERSIST)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)"
	@sh bazel/feasibility/kernel/uapi_contract_test.sh

__bazel-kernel-uapi-variants: __bazel-kernel-uapi
	@DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" build //bazel/feasibility/kernel:uapi_iphonesimulator_release //bazel/feasibility/kernel:uapi_iphonesimulator_development //bazel/feasibility/kernel:uapi_iphoneos_release //bazel/feasibility/kernel:uapi_iphoneos_development --config=release --config=source --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)"
	@rg -q '"destination": "iphonesimulator"' bazel-bin/bazel/feasibility/kernel/uapi_iphonesimulator_release/manifest.json
	@rg -q '"profile": "release"' bazel-bin/bazel/feasibility/kernel/uapi_iphonesimulator_release/manifest.json
	@rg -q '"destination": "iphonesimulator"' bazel-bin/bazel/feasibility/kernel/uapi_iphonesimulator_development/manifest.json
	@rg -q '"profile": "development"' bazel-bin/bazel/feasibility/kernel/uapi_iphonesimulator_development/manifest.json
	@rg -q '"destination": "iphoneos"' bazel-bin/bazel/feasibility/kernel/uapi_iphoneos_release/manifest.json
	@rg -q '"profile": "release"' bazel-bin/bazel/feasibility/kernel/uapi_iphoneos_release/manifest.json
	@rg -q '"destination": "iphoneos"' bazel-bin/bazel/feasibility/kernel/uapi_iphoneos_development/manifest.json
	@rg -q '"profile": "development"' bazel-bin/bazel/feasibility/kernel/uapi_iphoneos_development/manifest.json
	@test -s bazel-bin/bazel/feasibility/kernel/uapi_iphonesimulator_release/manifest.json
	@test -s bazel-bin/bazel/feasibility/kernel/uapi_iphonesimulator_development/manifest.json
	@test -s bazel-bin/bazel/feasibility/kernel/uapi_iphoneos_release/manifest.json
	@test -s bazel-bin/bazel/feasibility/kernel/uapi_iphoneos_development/manifest.json

__bazel-promote-uapi: __bazel-version-check
	@test -d "$(ORLIX_PINNED_DEVELOPER_DIR)" || { echo "missing pinned Xcode developer directory: $(ORLIX_PINNED_DEVELOPER_DIR)" >&2; exit 1; }
	@set -euo pipefail; \
	promote="$(ORLIX_BUILD_ROOT)/Bazel/promote"; \
	for side in a b; do \
		if [ -d "$$promote/$$side/output-base" ]; then \
			DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$$promote/$$side/output-base" shutdown >/dev/null 2>&1 || true; \
		fi; \
	done; \
	/bin/chmod -R u+w "$$promote" 2>/dev/null || true; \
	rm -rf "$$promote/a" "$$promote/b" "$$promote/uapi-proposal.json" "$$promote/uapi-sbom.json" "$$promote/uapi-in-toto.json" "$$promote/uapi-lock-proposal.json"; \
	mkdir -p "$$promote/a/disk" "$$promote/b/disk" "$$promote/a/output-base" "$$promote/b/output-base"; \
	for side in a b; do \
		DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$$promote/$$side/output-base" build //bazel/feasibility/kernel:uapi --nouse_action_cache --config=release --config=source --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_KBUILD_PERSIST= --disk_cache="$$promote/$$side/disk" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)"; \
		digest_file="$$(/usr/bin/find "$$promote/$$side/output-base" -path '*/feasibility/kernel/uapi/uapi.sha256' -print | /usr/bin/head -n 1)"; \
		test -n "$$digest_file" || { echo "missing uapi.sha256 for promote $$side" >&2; exit 1; }; \
		/bin/cp "$$digest_file" "$$promote/$$side/uapi.sha256"; \
	done; \
	digest="$$(PYTHONPATH="$(CURDIR)/bazel/promotion" python3 "$(CURDIR)/bazel/promotion/compare.py" "$$promote/a/uapi.sha256" "$$promote/b/uapi.sha256" --component uapi --proposal "$$promote/uapi-proposal.json" --sbom "$$promote/uapi-sbom.json" --in-toto "$$promote/uapi-in-toto.json" --lock-proposal "$$promote/uapi-lock-proposal.json" --lock "$(CURDIR)/artifacts.lock.json")"; \
	test "$${#digest}" -eq 64 || { echo "promote compare did not print a 64-hex digest" >&2; exit 1; }; \
	rg -q '"signed": false' "$$promote/uapi-proposal.json"; \
	rg -q '"oci_digest": null' "$$promote/uapi-proposal.json"; \
	if rg -q '"signed": true' "$$promote/uapi-proposal.json"; then echo "promotion must not invent a Cosign signature" >&2; exit 1; fi; \
	rg -F "$$digest" "$$promote/uapi-sbom.json"; \
	rg -q '"component": "uapi"' "$$promote/uapi-sbom.json"; \
	rg -F "$$digest" "$$promote/uapi-in-toto.json"; \
	rg -q '"signed": false' "$$promote/uapi-in-toto.json"; \
	rg -q '"oci_digest": null' "$$promote/uapi-in-toto.json"; \
	rg -q '"signed": false' "$$promote/uapi-lock-proposal.json"; \
	rg -q '"buildset": null' "$$promote/uapi-lock-proposal.json"; \
	python3 -c 'import json,sys; lock=json.load(open(sys.argv[1])); assert lock.get("buildset") is None and lock.get("components")=={}, lock' "$(CURDIR)/artifacts.lock.json"

__bazel-mlibc-from-uapi: __bazel-kernel-uapi
	@DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" build //bazel/feasibility/mlibc:sysroot --config=release --config=source --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)"
	@aquery_out="$$(mktemp -t orlix-mlibc-aquery)"; \
	DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" aquery 'mnemonic("OrlixMLibCSysroot", //bazel/feasibility/mlibc:sysroot)' --output=text --config=release --config=source --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)" > "$$aquery_out"; \
	rg -q 'Mnemonic: OrlixMLibCSysroot' "$$aquery_out" || { echo "missing OrlixMLibCSysroot action" >&2; rm -f "$$aquery_out"; exit 1; }; \
	rg -q 'uapi.sha256' "$$aquery_out" || { echo "mlibc sysroot must consume the UAPI digest" >&2; rm -f "$$aquery_out"; exit 1; }; \
	rg -q 'feasibility/kernel/uapi' "$$aquery_out" || { echo "mlibc sysroot must consume installed UAPI headers" >&2; rm -f "$$aquery_out"; exit 1; }; \
	if rg -q 'kbuild-archive.tar' "$$aquery_out"; then echo "mlibc sysroot must not consume the Kernel Kbuild archive" >&2; rm -f "$$aquery_out"; exit 1; fi; \
	if rg -q 'OrlixKernelAppleProductInfo|xcframework' "$$aquery_out"; then echo "mlibc sysroot must not consume an Apple product provider" >&2; rm -f "$$aquery_out"; exit 1; fi; \
	rm -f "$$aquery_out"
	@test -s bazel-bin/bazel/feasibility/mlibc/sysroot/libraries/libc.a
	@test -d bazel-bin/bazel/feasibility/mlibc/sysroot/headers
	@rg -q '"linux_input": "OrlixInstalledUapiInfo"' bazel-bin/bazel/feasibility/mlibc/sysroot/manifest.json
	@digest="$$(/usr/bin/sed -n 's/.*"consumed_uapi_digest": "\([^"]*\)".*/\1/p' bazel-bin/bazel/feasibility/mlibc/sysroot/manifest.json | /usr/bin/head -n 1)"; \
	test "$${#digest}" -eq 64 || { echo "mlibc sysroot missing consumed UAPI digest" >&2; exit 1; }; \
	test "$$digest" = "$$(/usr/bin/tr -d '[:space:]' < bazel-bin/bazel/feasibility/kernel/uapi/uapi.sha256)"

__bazel-guest-package: __bazel-mlibc-from-uapi
	@DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" build //bazel/feasibility/packages:true --config=release --config=source --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)"
	@aquery_out="$$(mktemp -t orlix-package-aquery)"; \
	DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" aquery 'mnemonic("OrlixGuestPackage", //bazel/feasibility/packages:true)' --output=text --config=release --config=source --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)" > "$$aquery_out"; \
	rg -q 'Mnemonic: OrlixGuestPackage' "$$aquery_out" || { echo "missing OrlixGuestPackage action" >&2; rm -f "$$aquery_out"; exit 1; }; \
	rg -q 'packages/true/configure' "$$aquery_out" || { echo "guest package must consume Autotools configure" >&2; rm -f "$$aquery_out"; exit 1; }; \
	if rg -q 'kbuild-archive.tar' "$$aquery_out"; then echo "guest package must not consume the Kernel Kbuild archive" >&2; rm -f "$$aquery_out"; exit 1; fi; \
	if rg -q 'OrlixKernelAppleProductInfo|xcframework' "$$aquery_out"; then echo "guest package must not consume an Apple product provider" >&2; rm -f "$$aquery_out"; exit 1; fi; \
	rm -f "$$aquery_out"
	@test -x bazel-bin/bazel/feasibility/packages/true/install/usr/bin/true
	@rg -q 'usr/bin/true' bazel-bin/bazel/feasibility/packages/true/file-manifest.txt
	@rg -q 'engine=configure-make-destdir' bazel-bin/bazel/feasibility/packages/true/package-metadata.txt
	@test -s bazel-bin/bazel/feasibility/packages/true/source-input.sha256

__bazel-coreutils: __bazel-mlibc-from-uapi
	@DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" build //bazel/feasibility/packages:coreutils --config=release --config=source --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)"
	@aquery_out="$$(mktemp -t orlix-coreutils-aquery)"; \
	DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" aquery 'mnemonic("OrlixGuestPackage", //bazel/feasibility/packages:coreutils)' --output=text --config=release --config=source --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)" > "$$aquery_out"; \
	rg -q 'Mnemonic: OrlixGuestPackage' "$$aquery_out" || { echo "missing OrlixGuestPackage action" >&2; rm -f "$$aquery_out"; exit 1; }; \
	rg -q 'configure' "$$aquery_out" || { echo "coreutils must consume Autotools configure" >&2; rm -f "$$aquery_out"; exit 1; }; \
	if rg -q 'kbuild-archive.tar' "$$aquery_out"; then echo "coreutils must not consume the Kernel Kbuild archive" >&2; rm -f "$$aquery_out"; exit 1; fi; \
	if rg -q 'OrlixKernelAppleProductInfo|xcframework' "$$aquery_out"; then echo "coreutils must not consume an Apple product provider" >&2; rm -f "$$aquery_out"; exit 1; fi; \
	rm -f "$$aquery_out"
	@rg -q 'name=coreutils' bazel-bin/bazel/feasibility/packages/coreutils/package-metadata.txt
	@rg -q 'version=9.11' bazel-bin/bazel/feasibility/packages/coreutils/package-metadata.txt
	@test -x bazel-bin/bazel/feasibility/packages/coreutils/install/usr/bin/true
	@test -x bazel-bin/bazel/feasibility/packages/coreutils/install/usr/bin/ls
	@programs="$$(/usr/bin/sed -n 's/^ORLIX_COREUTILS_PROGRAMS := //p' OrlixCoreUtils/Sources/make/config.mk)"; \
	for program in $$programs; do \
		test -x "bazel-bin/bazel/feasibility/packages/coreutils/install/usr/bin/$$program" || { echo "missing coreutils program: $$program" >&2; exit 1; }; \
		/usr/bin/file "bazel-bin/bazel/feasibility/packages/coreutils/install/usr/bin/$$program" | /usr/bin/grep -F -q 'ELF 64-bit LSB pie executable, ARM aarch64' || { /usr/bin/file "bazel-bin/bazel/feasibility/packages/coreutils/install/usr/bin/$$program" >&2; exit 1; }; \
	done

__bazel-bash: __bazel-mlibc-from-uapi
	@DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" build //bazel/feasibility/packages:bash --config=release --config=source --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)"
	@aquery_out="$$(mktemp -t orlix-bash-aquery)"; \
	DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" aquery 'mnemonic("OrlixGuestPackage", //bazel/feasibility/packages:bash)' --output=text --config=release --config=source --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)" > "$$aquery_out"; \
	rg -q 'Mnemonic: OrlixGuestPackage' "$$aquery_out" || { echo "missing OrlixGuestPackage action" >&2; rm -f "$$aquery_out"; exit 1; }; \
	rg -q 'configure' "$$aquery_out" || { echo "bash must consume Autotools configure" >&2; rm -f "$$aquery_out"; exit 1; }; \
	if rg -q 'kbuild-archive.tar' "$$aquery_out"; then echo "bash must not consume the Kernel Kbuild archive" >&2; rm -f "$$aquery_out"; exit 1; fi; \
	if rg -q 'OrlixKernelAppleProductInfo|xcframework' "$$aquery_out"; then echo "bash must not consume an Apple product provider" >&2; rm -f "$$aquery_out"; exit 1; fi; \
	rm -f "$$aquery_out"
	@rg -q 'name=bash' bazel-bin/bazel/feasibility/packages/bash/package-metadata.txt
	@rg -q 'version=5.3' bazel-bin/bazel/feasibility/packages/bash/package-metadata.txt
	@test -x bazel-bin/bazel/feasibility/packages/bash/install/usr/bin/bash
	@/usr/bin/file bazel-bin/bazel/feasibility/packages/bash/install/usr/bin/bash | /usr/bin/grep -F -q 'ELF 64-bit LSB pie executable, ARM aarch64' || { /usr/bin/file bazel-bin/bazel/feasibility/packages/bash/install/usr/bin/bash >&2; exit 1; }

define ORLIX_BAZEL_PACKAGE_PROOF
__bazel-$(1): __bazel-mlibc-from-uapi
	@DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" build //bazel/feasibility/packages:$(1) --config=release --config=source --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)"
	@aquery_out="$$$$(mktemp -t orlix-$(1)-aquery)"; \
	DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" aquery 'mnemonic("OrlixGuestPackage", //bazel/feasibility/packages:$(1))' --output=text --config=release --config=source --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)" > "$$$$aquery_out"; \
	rg -q 'Mnemonic: OrlixGuestPackage' "$$$$aquery_out" || { echo "missing OrlixGuestPackage action for $(1)" >&2; rm -f "$$$$aquery_out"; exit 1; }; \
	rg '^  Inputs:' "$$$$aquery_out" | rg -q 'feasibility/kernel/uapi' || { echo "$(1) must consume installed UAPI" >&2; rm -f "$$$$aquery_out"; exit 1; }; \
	rg '^  Inputs:' "$$$$aquery_out" | rg -q 'feasibility/mlibc' || { echo "$(1) must consume the mlibc sysroot" >&2; rm -f "$$$$aquery_out"; exit 1; }; \
	if rg '^  Inputs:' "$$$$aquery_out" | rg -q 'kbuild-archive.tar'; then echo "$(1) must not consume the Kernel Kbuild archive" >&2; rm -f "$$$$aquery_out"; exit 1; fi; \
	if rg -q 'OrlixKernelAppleProductInfo|xcframework' "$$$$aquery_out"; then echo "$(1) must not consume an Apple product provider" >&2; rm -f "$$$$aquery_out"; exit 1; fi; \
	if rg '^  Inputs:' "$$$$aquery_out" | rg -q 'OrlixOS/Sources/make'; then echo "$(1) must not consume OrlixOS wrapper Make" >&2; rm -f "$$$$aquery_out"; exit 1; fi; \
	if rg '^  Inputs:' "$$$$aquery_out" | rg -q 'OrlixCoreUtils/Makefile'; then echo "$(1) must not consume OrlixCoreUtils Makefile" >&2; rm -f "$$$$aquery_out"; exit 1; fi; \
	rm -f "$$$$aquery_out"
	@rg -q 'name=$(1)' bazel-bin/bazel/feasibility/packages/$(1)/package-metadata.txt
	@rg -q 'version=$(3)' bazel-bin/bazel/feasibility/packages/$(1)/package-metadata.txt
	@install="bazel-bin/bazel/feasibility/packages/$(1)/install"; \
	for rel in $(2); do \
		test -e "$$$$install/$$$$rel" || { echo "missing $(1) output: $$$$rel" >&2; exit 1; }; \
		case "$$$$rel" in \
		  *.a) test -s "$$$$install/$$$$rel" || { echo "empty $(1) archive: $$$$rel" >&2; exit 1; } ;; \
		  *) \
		    test -x "$$$$install/$$$$rel" || { echo "not executable: $$$$rel" >&2; exit 1; }; \
		    /usr/bin/file "$$$$install/$$$$rel" | /usr/bin/grep -F -q 'ELF 64-bit LSB' || { /usr/bin/file "$$$$install/$$$$rel" >&2; exit 1; }; \
		    /usr/bin/file "$$$$install/$$$$rel" | /usr/bin/grep -F -q 'ARM aarch64' || { /usr/bin/file "$$$$install/$$$$rel" >&2; exit 1; }; \
		    ;; \
		esac; \
	done
	@if [ "$(1)" = "init" ]; then /usr/bin/file bazel-bin/bazel/feasibility/packages/init/install/sbin/init | /usr/bin/grep -F -q 'pie executable' || { /usr/bin/file bazel-bin/bazel/feasibility/packages/init/install/sbin/init >&2; exit 1; }; fi
endef
$(eval $(call ORLIX_BAZEL_PACKAGE_PROOF,grep,usr/bin/grep,3.12))
$(eval $(call ORLIX_BAZEL_PACKAGE_PROOF,findutils,usr/bin/find usr/bin/xargs,4.10.0))
$(eval $(call ORLIX_BAZEL_PACKAGE_PROOF,e2fsprogs,usr/bin/mke2fs usr/bin/mkfs.ext4 usr/bin/debugfs usr/bin/e2fsck,1.47.1))
$(eval $(call ORLIX_BAZEL_PACKAGE_PROOF,getconf,usr/bin/getconf,orlix))
$(eval $(call ORLIX_BAZEL_PACKAGE_PROOF,getent,usr/bin/getent,orlix))
$(eval $(call ORLIX_BAZEL_PACKAGE_PROOF,init,sbin/init,orlix))
$(eval $(call ORLIX_BAZEL_PACKAGE_PROOF,jq,usr/bin/jq,1.7.1))
$(eval $(call ORLIX_BAZEL_PACKAGE_PROOF,curl,usr/bin/curl,8.20.0))
$(eval $(call ORLIX_BAZEL_PACKAGE_PROOF,ncurses,usr/lib/libtinfo.a,6.6))
$(eval $(call ORLIX_BAZEL_PACKAGE_PROOF,zsh,usr/bin/zsh,5.9))

__bazel-zsh: __bazel-ncurses

__bazel-rootfs: __bazel-coreutils __bazel-bash __bazel-grep __bazel-findutils __bazel-e2fsprogs __bazel-getconf __bazel-getent __bazel-init __bazel-jq __bazel-curl __bazel-zsh
	@DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" build //bazel/feasibility/rootfs:rootfs --config=release --config=source --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)"
	@aquery_out="$$(mktemp -t orlix-rootfs-aquery)"; \
	DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" aquery 'mnemonic("OrlixRootfs", //bazel/feasibility/rootfs:rootfs)' --output=text --config=release --config=source --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)" > "$$aquery_out"; \
	rg -q 'Mnemonic: OrlixRootfs' "$$aquery_out" || { echo "missing OrlixRootfs action" >&2; rm -f "$$aquery_out"; exit 1; }; \
	rg -q 'gen_init_cpio.c' "$$aquery_out" || { echo "rootfs must compile upstream Linux gen_init_cpio.c" >&2; rm -f "$$aquery_out"; exit 1; }; \
	if rg '^  Inputs:' "$$aquery_out" | rg -q 'kbuild-archive.tar'; then echo "rootfs must not consume the Kernel Kbuild archive" >&2; rm -f "$$aquery_out"; exit 1; fi; \
	if rg '^  Inputs:' "$$aquery_out" | rg -q 'OrlixOS/Sources/make'; then echo "rootfs must not consume OrlixOS wrapper Make" >&2; rm -f "$$aquery_out"; exit 1; fi; \
	rm -f "$$aquery_out"
	@test -x bazel-bin/bazel/feasibility/rootfs/rootfs/base-tree/bin/true
	@test -x bazel-bin/bazel/feasibility/rootfs/rootfs/base-tree/bin/ls
	@test -x bazel-bin/bazel/feasibility/rootfs/rootfs/base-tree/usr/bin/true
	@test -x bazel-bin/bazel/feasibility/rootfs/rootfs/base-tree/usr/bin/ls
	@test -x bazel-bin/bazel/feasibility/rootfs/rootfs/base-tree/bin/bash
	@test -x bazel-bin/bazel/feasibility/rootfs/rootfs/base-tree/usr/bin/bash
	@test -L bazel-bin/bazel/feasibility/rootfs/rootfs/base-tree/bin/sh
	@test "$$(/usr/bin/readlink bazel-bin/bazel/feasibility/rootfs/rootfs/base-tree/bin/sh)" = "bash"
	@test -x bazel-bin/bazel/feasibility/rootfs/rootfs/base-tree/bin/grep
	@test -x bazel-bin/bazel/feasibility/rootfs/rootfs/base-tree/bin/find
	@test -x bazel-bin/bazel/feasibility/rootfs/rootfs/base-tree/bin/xargs
	@test -x bazel-bin/bazel/feasibility/rootfs/rootfs/base-tree/bin/mke2fs
	@test -x bazel-bin/bazel/feasibility/rootfs/rootfs/base-tree/bin/mkfs.ext4
	@test -x bazel-bin/bazel/feasibility/rootfs/rootfs/base-tree/bin/debugfs
	@test -x bazel-bin/bazel/feasibility/rootfs/rootfs/base-tree/bin/e2fsck
	@test -x bazel-bin/bazel/feasibility/rootfs/rootfs/base-tree/usr/bin/getconf
	@test -x bazel-bin/bazel/feasibility/rootfs/rootfs/base-tree/usr/bin/getent
	@test -x bazel-bin/bazel/feasibility/rootfs/rootfs/base-tree/sbin/init
	@test -x bazel-bin/bazel/feasibility/rootfs/rootfs/base-tree/usr/bin/jq
	@test -x bazel-bin/bazel/feasibility/rootfs/rootfs/base-tree/usr/bin/curl
	@test -x bazel-bin/bazel/feasibility/rootfs/rootfs/base-tree/usr/bin/zsh
	@rg -q 'base_packages=bash coreutils grep findutils e2fsprogs jq curl zsh' bazel-bin/bazel/feasibility/rootfs/rootfs/payload-metadata.txt
	@test -s bazel-bin/bazel/feasibility/rootfs/rootfs/initramfs.cpio.gz
	@test -s bazel-bin/bazel/feasibility/rootfs/rootfs/base.ext4
	@test -s bazel-bin/bazel/feasibility/rootfs/rootfs/state.ext4
	@rg -q 'init=/bin/true' bazel-bin/bazel/feasibility/rootfs/rootfs/payload-metadata.txt
	@/usr/bin/gzip -t bazel-bin/bazel/feasibility/rootfs/rootfs/initramfs.cpio.gz

__bazel-live-activity-smoke: __bazel-feasibility-bootstrap
	@DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" build //bazel/feasibility/apple:LiveActivitySmoke --compilation_mode=dbg --config=release --config=source --ios_multi_cpus=sim_arm64 --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)"

__bazel-feasibility-xcodeproj: __bazel-feasibility-bootstrap
	@mkdir -p Build/XcodeProjects
	@DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" PATH="$(ORLIX_BAZEL_TOOL_ROOT)/$(ORLIX_BAZEL_VERSION):$(HOME)/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" run //xcode:feasibility --compilation_mode=dbg --config=release --config=source --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)"

__bazel-xcode-cloud-project-check:
	@test -d OrlixCloud.xcodeproj
	@test -f OrlixCloud.xcodeproj/xcshareddata/xcschemes/OrlixCloud.xcscheme
	@rg -q '__bazel-feasibility-bootstrap' OrlixCloud.xcodeproj/project.pbxproj
	@rg -q 'PRODUCT_BUNDLE_IDENTIFIER = com.rudironsoni.orlix;' OrlixCloud.xcodeproj/project.pbxproj
	@rg -q 'DEVELOPMENT_TEAM = ZQ3L7M567L;' OrlixCloud.xcodeproj/project.pbxproj
	@rg -q 'CODE_SIGN_STYLE = Automatic;' OrlixCloud.xcodeproj/project.pbxproj
	@rg -q 'IPHONEOS_DEPLOYMENT_TARGET = 15.0;' OrlixCloud.xcodeproj/project.pbxproj
	@DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" /usr/bin/xcodebuild -list -project OrlixCloud.xcodeproj | rg -q 'OrlixCloud'

__bazel-migration-inventory:
	@ruby bazel/migration/inventory.rb --write

__bazel-migration-inventory-check:
	@ruby bazel/migration/inventory.rb --check

__bazel-matrix-check: __bazel-version-check
	@PYTHONPATH="$(CURDIR)/bazel/config" python3 -m unittest test_toolchain_pin
	@PYTHONPATH="$(CURDIR)/bazel/migration" python3 -m unittest test_apple_build_matrix
	@PYTHONPATH="$(CURDIR)/bazel/extensions" python3 -m unittest test_native_sources
	@PYTHONPATH="$(CURDIR)/bazel/feasibility/kernel" python3 -m unittest test_kbuild_persist
	@PYTHONPATH="$(CURDIR)/bazel/promotion" python3 -m unittest test_compare
	@PYTHONPATH="$(CURDIR)/bazel/promotion" python3 -m unittest test_sbom
	@PYTHONPATH="$(CURDIR)/bazel/promotion" python3 -m unittest test_in_toto
	@PYTHONPATH="$(CURDIR)/bazel/promotion" python3 -m unittest test_lock_proposal
	@PYTHONPATH="$(CURDIR)/bazel/config" ORLIX_XCODE_VERSION="$(ORLIX_XCODE_VERSION)" ORLIX_XCODE_BUILD="$(ORLIX_XCODE_BUILD)" ORLIX_BAZEL_DISK_CACHE="$(ORLIX_BAZEL_DISK_CACHE)" python3 -c 'import os, toolchain_pin as pin; pin.require_identity(os.environ["ORLIX_XCODE_VERSION"], os.environ["ORLIX_XCODE_BUILD"], os.environ["ORLIX_BAZEL_DISK_CACHE"]); print("pass: disk-cache namespace", pin.namespace_for(os.environ["ORLIX_XCODE_VERSION"], os.environ["ORLIX_XCODE_BUILD"]))'
	@DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" test //bazel/feasibility/analysis:all --config=release --config=source --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)" --test_output=errors
