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
ORLIX_RUBY ?= /usr/bin/ruby
ORLIX_PINNED_DEVELOPER_DIR ?= /Applications/Xcode-26.6.0.app/Contents/Developer
ORLIX_TCTI_ISA_PREPARED ?= $(ORLIX_BUILD_ROOT)/OrlixKernel/orlix-tcti-isa
ORLIX_TCTI_ISA_ARCHIVE ?= $(CURDIR)/OrlixKernel/Sources/ports/orlix/isa/prepared-tables.tar.gz
ORLIX_TCTI_ISA_ARCHIVE_SHA256 ?= $(CURDIR)/OrlixKernel/Sources/ports/orlix/isa/prepared-tables.sha256
ORLIX_TCTI_ISA_ARTIFACTS := \
	manifest \
	source_manifest.def \
	target_asl_availability.def \
	target_feature_applicability.def \
	target_feature_artifact.def \
	target_feature_field_domain_binding.def \
	target_instruction_artifact_generated.h \
	target_register_artifact.def \
	target_runtime_capability_cohort_artifact.def \
	target_system_accessor_reconciliation.def
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
export ORLIX_TCTI_ISA_PREPARED
export CCACHE_BASEDIR
export CCACHE_DIR
export CCACHE_MAXSIZE
export CCACHE_COMPILERCHECK

.PHONY: __bazel-bootstrap __bazel-version-check __bazel-server-restart __bazel-module-lock-update __bazel-feasibility-bootstrap __bazel-apple-smoke
.PHONY: __bazel-apple-dependency-smoke __bazel-native-archives
.PHONY: __bazel-ghostty-archives __bazel-ssh-archives
.PHONY: __bazel-native-dependency-smoke __bazel-feasibility-xcodeproj
.PHONY: __bazel-kernel-uapi __bazel-kernel-uapi-variants __bazel-mlibc-from-uapi __bazel-live-activity-smoke __bazel-promote-uapi __bazel-promote-mlibc __bazel-promote-rootfs __bazel-publish-uapi __bazel-publish-mlibc __bazel-publish-rootfs __bazel-lock-proposal __bazel-lock-from-signed __bazel-reconstruct __bazel-reconstruct-source __bazel-hostadapter __bazel-orlixos __bazel-orlix-app __bazel-orlix-archive __bazel-ios15-simulator-gate __bazel-product-composition __bazel-cache-equivalence __bazel-kernel-boot __bazel-proof-graph __bazel-apple-routing-check __bazel-prove-matrix __bazel-gc
.PHONY: __bazel-guest-package __bazel-coreutils __bazel-bash __bazel-grep __bazel-findutils __bazel-e2fsprogs
.PHONY: __bazel-getconf __bazel-getent __bazel-init __bazel-jq __bazel-curl __bazel-ncurses __bazel-zsh __bazel-rootfs
.PHONY: __bazel-xcode-cloud-project-check
.PHONY: __bazel-migration-inventory __bazel-migration-inventory-check
.PHONY: __bazel-matrix-check __tcti-isa-restore

__bazel-bootstrap:
	@$(ORLIX_RUBY) bazel/bootstrap.rb >/dev/null

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

define ORLIX_BAZEL_PROMOTE
__bazel-promote-$(1): __bazel-version-check
	@test -d "$(ORLIX_PINNED_DEVELOPER_DIR)" || { echo "missing pinned Xcode developer directory: $(ORLIX_PINNED_DEVELOPER_DIR)" >&2; exit 1; }
	@set -euo pipefail; \
	promote="$(ORLIX_BUILD_ROOT)/Bazel/promote/$(1)"; \
	for side in a b; do \
		if [ -d "$$$$promote/$$$$side/output-base" ]; then \
			DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$$$$promote/$$$$side/output-base" shutdown >/dev/null 2>&1 || true; \
		fi; \
	done; \
	/bin/chmod -R u+w "$$$$promote" 2>/dev/null || true; \
	rm -rf "$$$$promote"; \
	mkdir -p "$$$$promote/a/disk" "$$$$promote/b/disk" "$$$$promote/a/output-base" "$$$$promote/b/output-base"; \
	for side in a b; do \
		DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$$$$promote/$$$$side/output-base" build $(2) --nouse_action_cache --config=release --config=source --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_KBUILD_PERSIST= --disk_cache="$$$$promote/$$$$side/disk" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)"; \
		digest_file="$$$$(/usr/bin/find "$$$$promote/$$$$side/output-base" -path '*/$(3)' -print | /usr/bin/head -n 1)"; \
		test -n "$$$$digest_file" || { echo "missing $(3) for promote $(1) $$$$side" >&2; exit 1; }; \
		/bin/cp "$$$$digest_file" "$$$$promote/$$$$side/digest.sha256"; \
	done; \
	lock_before="$$$$(/usr/bin/shasum -a 256 "$(CURDIR)/artifacts.lock.json")"; \
	digest="$$$$(PYTHONPATH="$(CURDIR)/bazel/promotion" python3 "$(CURDIR)/bazel/promotion/compare.py" "$$$$promote/a/digest.sha256" "$$$$promote/b/digest.sha256" --component $(1) --proposal "$$$$promote/$(1)-proposal.json" --sbom "$$$$promote/$(1)-sbom.json" --in-toto "$$$$promote/$(1)-in-toto.json" --lock-proposal "$$$$promote/$(1)-lock-proposal.json" --lock "$(CURDIR)/artifacts.lock.json")"; \
	test "$$$${#digest}" -eq 64 || { echo "promote compare did not print a 64-hex digest" >&2; exit 1; }; \
	rg -q '"signed": false' "$$$$promote/$(1)-proposal.json"; \
	rg -q '"oci_digest": null' "$$$$promote/$(1)-proposal.json"; \
	if rg -q '"signed": true' "$$$$promote/$(1)-proposal.json"; then echo "promotion must not invent a Cosign signature" >&2; exit 1; fi; \
	rg -F "$$$$digest" "$$$$promote/$(1)-sbom.json"; \
	rg -q '"component": "$(1)"' "$$$$promote/$(1)-sbom.json"; \
	rg -F "$$$$digest" "$$$$promote/$(1)-in-toto.json"; \
	rg -q '"signed": false' "$$$$promote/$(1)-in-toto.json"; \
	rg -q '"oci_digest": null' "$$$$promote/$(1)-in-toto.json"; \
	rg -q '"signed": false' "$$$$promote/$(1)-lock-proposal.json"; \
	rg -q '"buildset": null' "$$$$promote/$(1)-lock-proposal.json"; \
	python3 -c 'import json,sys; p=json.load(open(sys.argv[1])); assert p.get("signed") is False and p.get("oci_digest") is None, p' "$$$$promote/$(1)-proposal.json"; \
	lock_after="$$$$(/usr/bin/shasum -a 256 "$(CURDIR)/artifacts.lock.json")"; \
	test "$$$$lock_before" = "$$$$lock_after" || { echo "unsigned promote mutated artifacts.lock.json" >&2; exit 1; }; \
	if PYTHONPATH="$(CURDIR)/bazel/promotion" python3 "$(CURDIR)/bazel/promotion/sign.py" --component $(1) --digest "$$$$digest" --proposal "$$$$promote/$(1)-signed.json"; then echo "unsigned promote must not Cosign-sign" >&2; exit 1; fi; \
	test ! -e "$$$$promote/$(1)-signed.json"
endef
$(eval $(call ORLIX_BAZEL_PROMOTE,uapi,//bazel/feasibility/kernel:uapi,feasibility/kernel/uapi/uapi.sha256))
$(eval $(call ORLIX_BAZEL_PROMOTE,mlibc,//bazel/feasibility/mlibc:sysroot,feasibility/mlibc/sysroot/sysroot.sha256))
$(eval $(call ORLIX_BAZEL_PROMOTE,rootfs,//bazel/feasibility/rootfs:rootfs,feasibility/rootfs/rootfs/source-input.sha256))

define ORLIX_BAZEL_PUBLISH
__bazel-publish-$(1): __bazel-version-check
	@set -euo pipefail; \
	test -n "$$$$ORLIX_COSIGN_KEY" || { echo "ORLIX_COSIGN_KEY is required to publish $(1)" >&2; exit 1; }; \
	promote="$(ORLIX_BUILD_ROOT)/Bazel/promote/$(1)"; \
	digest_file="$$$$promote/a/digest.sha256"; \
	test -s "$$$$digest_file" || { echo "missing unsigned digest $$$$digest_file; run make __bazel-promote-$(1) first" >&2; exit 1; }; \
	digest="$$$$(tr -d '[:space:]' < "$$$$digest_file")"; \
	PYTHONPATH="$(CURDIR)/bazel/promotion" python3 "$(CURDIR)/bazel/promotion/sign.py" --component $(1) --digest "$$$$digest" --artifact "$$$$digest_file" --proposal "$$$$promote/$(1)-signed.json"; \
	python3 -c 'import json,sys; p=json.load(open(sys.argv[1])); assert p.get("signed") is True and p.get("oci_digest","").startswith("sha256:"), p' "$$$$promote/$(1)-signed.json"; \
	PYTHONPATH="$(CURDIR)/bazel/promotion" python3 "$(CURDIR)/bazel/promotion/publish.py" --proposal "$$$$promote/$(1)-signed.json"
endef
$(eval $(call ORLIX_BAZEL_PUBLISH,uapi))
$(eval $(call ORLIX_BAZEL_PUBLISH,mlibc))
$(eval $(call ORLIX_BAZEL_PUBLISH,rootfs))

__bazel-lock-proposal: __bazel-version-check
	@set -euo pipefail; \
	lock_before="$$(/usr/bin/shasum -a 256 "$(CURDIR)/artifacts.lock.json")"; \
	mkdir -p "$(ORLIX_BUILD_ROOT)/Bazel/promote"; \
	PYTHONPATH="$(CURDIR)/bazel/promotion" python3 "$(CURDIR)/bazel/promotion/lock_proposal.py" \
		--out "$(ORLIX_BUILD_ROOT)/Bazel/promote/buildset-lock-proposal.json" \
		--signed "$(ORLIX_BUILD_ROOT)/Bazel/promote/uapi/uapi-signed.json" \
		--signed "$(ORLIX_BUILD_ROOT)/Bazel/promote/mlibc/mlibc-signed.json" \
		--signed "$(ORLIX_BUILD_ROOT)/Bazel/promote/rootfs/rootfs-signed.json"; \
	python3 -c 'import json,sys; p=json.load(open(sys.argv[1])); assert p.get("signed") is True and p.get("buildset") and set(p["components"])=={"uapi","mlibc","rootfs"}, p' "$(ORLIX_BUILD_ROOT)/Bazel/promote/buildset-lock-proposal.json"; \
	lock_after="$$(/usr/bin/shasum -a 256 "$(CURDIR)/artifacts.lock.json")"; \
	test "$$lock_before" = "$$lock_after" || { echo "lock proposal mutated artifacts.lock.json" >&2; exit 1; }

__bazel-lock-from-signed: __bazel-lock-proposal
	@set -euo pipefail; \
	PYTHONPATH="$(CURDIR)/bazel/promotion" python3 "$(CURDIR)/bazel/promotion/lock_proposal.py" \
		--out "$(ORLIX_BUILD_ROOT)/Bazel/promote/buildset-lock-proposal.json" \
		--signed "$(ORLIX_BUILD_ROOT)/Bazel/promote/uapi/uapi-signed.json" \
		--signed "$(ORLIX_BUILD_ROOT)/Bazel/promote/mlibc/mlibc-signed.json" \
		--signed "$(ORLIX_BUILD_ROOT)/Bazel/promote/rootfs/rootfs-signed.json" \
		--apply-lock "$(CURDIR)/artifacts.lock.json"; \
	python3 -c 'import json,sys; lock=json.load(open(sys.argv[1])); assert lock.get("buildset"); assert set(lock["components"])=={"uapi","mlibc","rootfs"}, lock' "$(CURDIR)/artifacts.lock.json"

__bazel-reconstruct: __bazel-version-check
	@set -euo pipefail; \
	test -n "$$ORLIX_COSIGN_KEY" || { echo "ORLIX_COSIGN_KEY is required to reconstruct" >&2; exit 1; }; \
	PYTHONPATH="$(CURDIR)/bazel/promotion" python3 "$(CURDIR)/bazel/promotion/reconstruct.py" --lock "$(CURDIR)/artifacts.lock.json"

__bazel-reconstruct-source: __bazel-version-check
	@set -euo pipefail; \
	python3 -c 'import json,sys; lock=json.load(open(sys.argv[1])); assert lock.get("buildset") and lock.get("components",{}).get("uapi",{}).get("unsigned_digest"), lock' "$(CURDIR)/artifacts.lock.json"; \
	cold="$(ORLIX_BUILD_ROOT)/Bazel/promote/uapi/cold"; \
	rm -rf "$$cold"; \
	mkdir -p "$$cold/disk"; \
	DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$$cold/output-base" build //bazel/feasibility/kernel:uapi --nouse_action_cache --config=release --config=source --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_KBUILD_PERSIST= --disk_cache="$$cold/disk" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)"; \
	digest_file="$$(/usr/bin/find "$$cold/output-base" -path '*/feasibility/kernel/uapi/uapi.sha256' -print | /usr/bin/head -n 1)"; \
	test -n "$$digest_file" || { echo "missing cache-cold uapi.sha256" >&2; exit 1; }; \
	digest="$$(/usr/bin/tr -d '[:space:]' < "$$digest_file")"; \
	test "$${#digest}" -eq 64 || { echo "cache-cold uapi digest is not 64 hex" >&2; exit 1; }; \
	PYTHONPATH="$(CURDIR)/bazel/promotion" python3 -c 'import json,sys; lock=json.load(open(sys.argv[1])); expected=lock["components"]["uapi"]["unsigned_digest"]; got=sys.argv[2]; assert expected==got, (expected, got)' "$(CURDIR)/artifacts.lock.json" "$$digest"; \
	echo "$$digest"

__bazel-mlibc-from-uapi: __bazel-kernel-uapi
	@mkdir -p "$(ORLIX_KBUILD_PERSIST)"
	@DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" build //bazel/feasibility/mlibc:sysroot --config=release --config=source --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_KBUILD_PERSIST="$(ORLIX_KBUILD_PERSIST)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)"
	@aquery_out="$$(mktemp -t orlix-mlibc-aquery)"; \
	DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" aquery 'mnemonic("OrlixMLibCSysroot", //bazel/feasibility/mlibc:sysroot)' --output=text --config=release --config=source --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_KBUILD_PERSIST="$(ORLIX_KBUILD_PERSIST)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)" > "$$aquery_out"; \
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
	@sysroot_digest="$$(/usr/bin/tr -d '[:space:]' < bazel-bin/bazel/feasibility/mlibc/sysroot/sysroot.sha256)"; \
	test "$${#sysroot_digest}" -eq 64 || { echo "mlibc sysroot missing 64-hex sysroot.sha256" >&2; exit 1; }; \
	rg -q '"sysroot_digest": "'"$$sysroot_digest"'"' bazel-bin/bazel/feasibility/mlibc/sysroot/manifest.json

__bazel-guest-package: __bazel-mlibc-from-uapi
	@DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" build //bazel/feasibility/packages:true --config=release --config=source --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_KBUILD_PERSIST="$(ORLIX_KBUILD_PERSIST)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)"
	@aquery_out="$$(mktemp -t orlix-package-aquery)"; \
	DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" aquery 'mnemonic("OrlixGuestPackage", //bazel/feasibility/packages:true)' --output=text --config=release --config=source --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_KBUILD_PERSIST="$(ORLIX_KBUILD_PERSIST)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)" > "$$aquery_out"; \
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
	@DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" build //bazel/feasibility/packages:coreutils --config=release --config=source --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_KBUILD_PERSIST="$(ORLIX_KBUILD_PERSIST)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)"
	@aquery_out="$$(mktemp -t orlix-coreutils-aquery)"; \
	DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" aquery 'mnemonic("OrlixGuestPackage", //bazel/feasibility/packages:coreutils)' --output=text --config=release --config=source --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_KBUILD_PERSIST="$(ORLIX_KBUILD_PERSIST)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)" > "$$aquery_out"; \
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
	@DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" build //bazel/feasibility/packages:bash --config=release --config=source --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_KBUILD_PERSIST="$(ORLIX_KBUILD_PERSIST)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)"
	@aquery_out="$$(mktemp -t orlix-bash-aquery)"; \
	DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" aquery 'mnemonic("OrlixGuestPackage", //bazel/feasibility/packages:bash)' --output=text --config=release --config=source --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_KBUILD_PERSIST="$(ORLIX_KBUILD_PERSIST)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)" > "$$aquery_out"; \
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
	@DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" build //bazel/feasibility/packages:$(1) --config=release --config=source --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_KBUILD_PERSIST="$(ORLIX_KBUILD_PERSIST)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)"
	@aquery_out="$$$$(mktemp -t orlix-$(1)-aquery)"; \
	DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" aquery 'mnemonic("OrlixGuestPackage", //bazel/feasibility/packages:$(1))' --output=text --config=release --config=source --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_KBUILD_PERSIST="$(ORLIX_KBUILD_PERSIST)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)" > "$$$$aquery_out"; \
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
	@DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" build //bazel/feasibility/rootfs:rootfs --config=release --config=source --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_KBUILD_PERSIST="$(ORLIX_KBUILD_PERSIST)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)"
	@aquery_out="$$(mktemp -t orlix-rootfs-aquery)"; \
	DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" aquery 'mnemonic("OrlixRootfs", //bazel/feasibility/rootfs:rootfs)' --output=text --config=release --config=source --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_KBUILD_PERSIST="$(ORLIX_KBUILD_PERSIST)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)" > "$$aquery_out"; \
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
	@$(ORLIX_RUBY) bazel/migration/inventory.rb --write

__bazel-migration-inventory-check:
	@$(ORLIX_RUBY) bazel/migration/inventory.rb --check

__bazel-kernel-boot: __bazel-feasibility-bootstrap
	@DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" build //OrlixKernel/Sources:OrlixKernelBoot --compilation_mode=dbg --config=release --config=source --apple_platform_type=ios --ios_multi_cpus=sim_arm64 --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)"
	@boot_lo="$$(/usr/bin/find "$(ORLIX_BAZEL_OUTPUT_BASE)/execroot/_main/bazel-out" -path '*/bin/OrlixKernel/Sources/libOrlixKernelBoot.lo' ! -path '*/runfiles/*' -print | /usr/bin/head -n 1)"; \
	test -n "$$boot_lo" && test -s "$$boot_lo" || { echo "missing OrlixKernelBoot .lo" >&2; exit 1; }; \
	/usr/bin/nm -gU "$$boot_lo" | /usr/bin/grep -E -q '[[:space:]]T[[:space:]]_OrlixBoot$$' || { echo "OrlixKernelBoot archive missing defined _OrlixBoot" >&2; exit 1; }
	@DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" build //bazel/feasibility/kernel:macho --config=release --config=source --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_TCTI_ISA_PREPARED="$(ORLIX_TCTI_ISA_PREPARED)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)"
	@macho="$$(/usr/bin/find "$(ORLIX_BAZEL_OUTPUT_BASE)/execroot/_main/bazel-out" -path '*/bin/bazel/feasibility/kernel/macho/OrlixKernel.a' ! -path '*/runfiles/*' -print | /usr/bin/head -n 1)"; \
	test -n "$$macho" && test -s "$$macho" || { echo "missing Mach-O OrlixKernel.a" >&2; exit 1; }; \
	/usr/bin/grep -E '[[:space:]]T[[:space:]]+_arch_boot_entry' "$${macho%/*}/symbols.txt" >/dev/null || /usr/bin/nm -gU "$$macho" | /usr/bin/grep -E '[[:space:]]T[[:space:]]+_arch_boot_entry' >/dev/null || { echo "OrlixKernel.a missing defined _arch_boot_entry" >&2; exit 1; }

__bazel-proof-graph: __bazel-kernel-uapi
	@test -s bazel-bin/bazel/feasibility/kernel/uapi/uapi.sha256
	@mkdir -p "$(ORLIX_BUILD_ROOT)/Bazel/proof"
	@toolchain_digest="$$(/usr/bin/shasum -a 256 bazel/config/toolchain-pin.json | /usr/bin/awk '{print $$1}')"; \
	extra=""; \
	if [ -s bazel-bin/bazel/feasibility/mlibc/sysroot/sysroot.sha256 ]; then extra="$$extra --mlibc-digest bazel-bin/bazel/feasibility/mlibc/sysroot/sysroot.sha256"; fi; \
	if [ -s bazel-bin/bazel/feasibility/rootfs/rootfs/source-input.sha256 ]; then extra="$$extra --rootfs-digest bazel-bin/bazel/feasibility/rootfs/rootfs/source-input.sha256"; fi; \
	buildset="$$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1])).get("buildset") or "")' "$(CURDIR)/artifacts.lock.json")"; \
	if [ -n "$$buildset" ]; then extra="$$extra --buildset-digest $$buildset"; fi; \
	if [ -s "$(ORLIX_BUILD_ROOT)/Bazel/proof/kernel-dependency.evidence" ]; then extra="$$extra --evidence kernel-dependency=$(ORLIX_BUILD_ROOT)/Bazel/proof/kernel-dependency.evidence"; fi; \
	if [ -s "$(ORLIX_BUILD_ROOT)/Bazel/proof/kunit.evidence" ]; then extra="$$extra --evidence kunit=$(ORLIX_BUILD_ROOT)/Bazel/proof/kunit.evidence"; fi; \
	if [ -s "$(ORLIX_BUILD_ROOT)/Bazel/proof/kselftest.evidence" ]; then extra="$$extra --evidence kselftest=$(ORLIX_BUILD_ROOT)/Bazel/proof/kselftest.evidence"; fi; \
	if [ -s "$(ORLIX_BUILD_ROOT)/Bazel/proof/mlibc.evidence" ]; then extra="$$extra --evidence orlixmlibc=$(ORLIX_BUILD_ROOT)/Bazel/proof/mlibc.evidence"; fi; \
	if [ -s "$(ORLIX_BUILD_ROOT)/Bazel/proof/syscall-uapi.evidence" ]; then extra="$$extra --evidence syscall-uapi=$(ORLIX_BUILD_ROOT)/Bazel/proof/syscall-uapi.evidence"; fi; \
	if [ -s "$(ORLIX_BUILD_ROOT)/Bazel/proof/posix-shell.evidence" ]; then extra="$$extra --evidence posix-shell=$(ORLIX_BUILD_ROOT)/Bazel/proof/posix-shell.evidence"; fi; \
	PYTHONPATH="$(CURDIR)/bazel/proof:$(CURDIR)/bazel/promotion" python3 "$(CURDIR)/bazel/proof/graph.py" \
		--out "$(ORLIX_BUILD_ROOT)/Bazel/proof" \
		--lock "$(CURDIR)/artifacts.lock.json" \
		--uapi-digest bazel-bin/bazel/feasibility/kernel/uapi/uapi.sha256 \
		--toolchain-digest "$$toolchain_digest" \
		--profile "$(PROFILE)" \
		--destination iphonesimulator \
		$$extra
	@test -s "$(ORLIX_BUILD_ROOT)/Bazel/proof/index.json"
	@rg -q '"complete": false' "$(ORLIX_BUILD_ROOT)/Bazel/proof/index.json"
	@if [ -s "$(ORLIX_BUILD_ROOT)/Bazel/proof/kernel-dependency.evidence" ]; then \
		rg -q 'kernel-dependency:pass' "$(ORLIX_BUILD_ROOT)/Bazel/proof/index.json"; \
		if [ -s "$(ORLIX_BUILD_ROOT)/Bazel/proof/kunit.evidence" ]; then \
			rg -q 'kunit:pass' "$(ORLIX_BUILD_ROOT)/Bazel/proof/index.json"; \
			if [ -s "$(ORLIX_BUILD_ROOT)/Bazel/proof/kselftest.evidence" ]; then \
				rg -q 'kselftest:pass' "$(ORLIX_BUILD_ROOT)/Bazel/proof/index.json"; \
				if [ -s "$(ORLIX_BUILD_ROOT)/Bazel/proof/mlibc.evidence" ]; then \
					rg -q 'orlixmlibc:pass' "$(ORLIX_BUILD_ROOT)/Bazel/proof/index.json"; \
					if [ -s "$(ORLIX_BUILD_ROOT)/Bazel/proof/syscall-uapi.evidence" ]; then \
						rg -q 'syscall-uapi:pass' "$(ORLIX_BUILD_ROOT)/Bazel/proof/index.json"; \
						if [ -s "$(ORLIX_BUILD_ROOT)/Bazel/proof/posix-shell.evidence" ]; then \
							rg -q 'posix-shell:pass' "$(ORLIX_BUILD_ROOT)/Bazel/proof/index.json"; \
							rg -q 'jq:blocked' "$(ORLIX_BUILD_ROOT)/Bazel/proof/index.json"; \
						else \
							rg -q 'posix-shell:blocked' "$(ORLIX_BUILD_ROOT)/Bazel/proof/index.json"; \
						fi; \
					else \
						rg -q 'syscall-uapi:blocked' "$(ORLIX_BUILD_ROOT)/Bazel/proof/index.json"; \
					fi; \
				else \
					rg -q 'orlixmlibc:blocked' "$(ORLIX_BUILD_ROOT)/Bazel/proof/index.json"; \
				fi; \
			else \
				rg -q 'kselftest:blocked' "$(ORLIX_BUILD_ROOT)/Bazel/proof/index.json"; \
			fi; \
		else \
			rg -q 'kunit:blocked' "$(ORLIX_BUILD_ROOT)/Bazel/proof/index.json"; \
		fi; \
	else \
		rg -q 'kernel-dependency:blocked' "$(ORLIX_BUILD_ROOT)/Bazel/proof/index.json"; \
	fi
	@if rg -q 'jq:pass|curl:pass|zsh:pass|product-integration:pass' "$(ORLIX_BUILD_ROOT)/Bazel/proof/index.json"; then echo "proof graph must not invent a later ADR 0017 pass" >&2; exit 1; fi; \
	if [ ! -s "$(ORLIX_BUILD_ROOT)/Bazel/proof/mlibc.evidence" ] && rg -q 'orlixmlibc:pass' "$(ORLIX_BUILD_ROOT)/Bazel/proof/index.json"; then echo "proof graph must not invent orlixmlibc pass" >&2; exit 1; fi; \
	if [ ! -s "$(ORLIX_BUILD_ROOT)/Bazel/proof/syscall-uapi.evidence" ] && rg -q 'syscall-uapi:pass' "$(ORLIX_BUILD_ROOT)/Bazel/proof/index.json"; then echo "proof graph must not invent syscall-uapi pass" >&2; exit 1; fi; \
	if [ ! -s "$(ORLIX_BUILD_ROOT)/Bazel/proof/posix-shell.evidence" ] && rg -q 'posix-shell:pass' "$(ORLIX_BUILD_ROOT)/Bazel/proof/index.json"; then echo "proof graph must not invent posix-shell pass" >&2; exit 1; fi
	@python3 -c 'import json,sys; lock=json.load(open(sys.argv[1])); idx=json.load(open(sys.argv[2])); buildset=lock.get("buildset"); assert (not buildset) or idx.get("buildset_digest")==buildset, (buildset, idx.get("buildset_digest"))' "$(CURDIR)/artifacts.lock.json" "$(ORLIX_BUILD_ROOT)/Bazel/proof/index.json"

__bazel-prove-matrix: __bazel-orlix-app __bazel-apple-smoke __bazel-live-activity-smoke __bazel-native-dependency-smoke
	@mkdir -p "$(ORLIX_BUILD_ROOT)/Bazel/proof"
	@DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" build //bazel/feasibility/apple:SmokeApp --compilation_mode=opt --config=release --config=source --ios_multi_cpus=sim_arm64 --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)"
	@set -euo pipefail; \
	map="$(ORLIX_BUILD_ROOT)/Bazel/proof/matrix-evidence.json"; \
	PYTHONPATH="$(CURDIR)/bazel/migration" python3 "$(CURDIR)/bazel/migration/matrix_evidence.py" \
		--output-base "$(ORLIX_BAZEL_OUTPUT_BASE)" \
		--build-root "$(ORLIX_BUILD_ROOT)" \
		--repo "$(CURDIR)" \
		--out "$$map"; \
	args=(); \
	while IFS= read -r line; do args+=(--evidence "$$line"); done < <(python3 -c 'import json,sys; [print("%s=%s" % item) for item in json.load(open(sys.argv[1])).items()]' "$$map"); \
	PYTHONPATH="$(CURDIR)/bazel/migration" python3 "$(CURDIR)/bazel/migration/prove_matrix.py" \
		--matrix "$(CURDIR)/bazel/migration/apple-build-matrix.json" \
		--out "$(ORLIX_BUILD_ROOT)/Bazel/proof/matrix-prove.json" \
		"$${args[@]}"; \
	PYTHONPATH="$(CURDIR)/bazel/promotion" python3 -c 'import lock_proposal; lock_proposal.assert_unsigned_lock_proposals(["$(ORLIX_BUILD_ROOT)/Bazel/promote/uapi/uapi-lock-proposal.json","$(ORLIX_BUILD_ROOT)/Bazel/promote/mlibc/mlibc-lock-proposal.json","$(ORLIX_BUILD_ROOT)/Bazel/promote/rootfs/rootfs-lock-proposal.json"], "$(CURDIR)/artifacts.lock.json")'
	@rg -q '"complete": true' "$(ORLIX_BUILD_ROOT)/Bazel/proof/matrix-prove.json"
	@rg -q 'ios-15.5-ci-runtime' "$(ORLIX_BUILD_ROOT)/Bazel/proof/matrix-prove.json"
	@rg -q 'signing-distribution' "$(ORLIX_BUILD_ROOT)/Bazel/proof/matrix-prove.json"

__bazel-apple-routing-check:
	@rg -q '^ORLIX_BAZEL_AUTHORITY \?= 1$$' Makefile
	@rg -F -q '__bazel-orlix-app' Makefile
	@rg -A2 '^__bazel-orlix-app:' make/bazel-migration.mk | rg -F -q -- '--config=promoted'
	@rg -F -q '//bazel/promotion:locked_buildset' Orlix/BUILD.bazel
	@rg -F -q 'ORLIX_DEVELOPMENT_TEAM ?= ZQ3L7M567L' Makefile
	@rg -F -q 'ios15_simulator_gate' make/bazel-migration.mk
	@rg -A3 '^ios15-simulator-gate:' Makefile | rg -F -q '__bazel-ios15-simulator-gate'
	@$(MAKE) -n ios15-simulator-gate ORLIX_IOS15_SIMULATOR_ID=00000000-0000-0000-0000-000000000000 | rg -q '__bazel-ios15-simulator-gate'
	@rg -A3 '^beta-archive:' Makefile | rg -F -q '__bazel-orlix-archive'
	@rg -F -q 'name = "OrlixUITests"' Orlix/BUILD.bazel
	@rg -F -q '__bazel-feasibility-xcodeproj' Makefile
	@rg -F -q '__bazel-kernel-uapi' Makefile
	@rg -A2 '^test:' Makefile | rg -F -q '__bazel-matrix-check'
	@rg -A3 '^rebuild:' Makefile | rg -F -q '__bazel-orlix-app'
	@rg -q '^runtime-tests: xcodeproj$$' Makefile
	@rg -F -q 'ORLIX_BAZEL_AUTHORITY),1' Makefile
	@rg -q '^common --repository_cache=~/Library/Caches/Orlix/Bazel/repository-cache$$' .bazelrc
	@if rg -n '^ORLIX_BAZEL_AUTHORITY \?= 0$$' Makefile; then echo "Makefile must default Bazel authority on after cutover" >&2; exit 1; fi

__bazel-gc: __bazel-version-check
	@test -d "$(ORLIX_PINNED_DEVELOPER_DIR)" || { echo "missing pinned Xcode developer directory: $(ORLIX_PINNED_DEVELOPER_DIR)" >&2; exit 1; }
	@PYTHONPATH="$(CURDIR)/bazel/config" ORLIX_XCODE_VERSION="$(ORLIX_XCODE_VERSION)" ORLIX_XCODE_BUILD="$(ORLIX_XCODE_BUILD)" ORLIX_BAZEL_DISK_CACHE="$(ORLIX_BAZEL_DISK_CACHE)" python3 -c 'import os, toolchain_pin as pin; pin.require_identity(os.environ["ORLIX_XCODE_VERSION"], os.environ["ORLIX_XCODE_BUILD"], os.environ["ORLIX_BAZEL_DISK_CACHE"])'
	@mkdir -p "$(ORLIX_BAZEL_DISK_CACHE)" "$(ORLIX_BAZEL_REPOSITORY_CACHE)"
	@PYTHONPATH="$(CURDIR)/bazel/config" python3 "$(CURDIR)/bazel/config/cache_gc.py" --root "$(ORLIX_BAZEL_DISK_CACHE)" --namespace "bazel-$(ORLIX_BAZEL_VERSION)-xcode-$(ORLIX_XCODE_BUILD)"
	@DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" shutdown >/dev/null 2>&1 || true

__bazel-matrix-check: __bazel-version-check __bazel-apple-routing-check
	@PYTHONPATH="$(CURDIR)/bazel/config" python3 -m unittest test_toolchain_pin
	@PYTHONPATH="$(CURDIR)/bazel/config" python3 -m unittest test_cache_gc
	@PYTHONPATH="$(CURDIR)/bazel/migration" python3 -m unittest test_apple_build_matrix
	@PYTHONPATH="$(CURDIR)/bazel/migration" python3 -m unittest test_prove_matrix
	@PYTHONPATH="$(CURDIR)/bazel/migration" python3 -m unittest test_workflow_policy
	@PYTHONPATH="$(CURDIR)/bazel/migration" python3 -m unittest test_tcti_isa_pin
	@PYTHONPATH="$(CURDIR)/bazel/extensions" python3 -m unittest test_native_sources
	@PYTHONPATH="$(CURDIR)/bazel/feasibility/kernel" python3 -m unittest test_kbuild_persist
	@PYTHONPATH="$(CURDIR)/bazel/promotion" python3 -m unittest test_compare
	@PYTHONPATH="$(CURDIR)/bazel/promotion" python3 -m unittest test_sbom
	@PYTHONPATH="$(CURDIR)/bazel/promotion" python3 -m unittest test_in_toto
	@PYTHONPATH="$(CURDIR)/bazel/promotion" python3 -m unittest test_lock_proposal
	@PYTHONPATH="$(CURDIR)/bazel/promotion" python3 -m unittest test_sign
	@PYTHONPATH="$(CURDIR)/bazel/promotion" python3 -m unittest test_publish
	@PYTHONPATH="$(CURDIR)/bazel/promotion" python3 -m unittest test_reconstruct
	@PYTHONPATH="$(CURDIR)/bazel/promotion" python3 -m unittest test_locked_buildset
	@PYTHONPATH="$(CURDIR)/bazel/proof" python3 -m unittest test_bind
	@PYTHONPATH="$(CURDIR)/bazel/proof" python3 -m unittest test_graph
	@PYTHONPATH="$(CURDIR)/bazel/config" ORLIX_XCODE_VERSION="$(ORLIX_XCODE_VERSION)" ORLIX_XCODE_BUILD="$(ORLIX_XCODE_BUILD)" ORLIX_BAZEL_DISK_CACHE="$(ORLIX_BAZEL_DISK_CACHE)" python3 -c 'import os, toolchain_pin as pin; pin.require_identity(os.environ["ORLIX_XCODE_VERSION"], os.environ["ORLIX_XCODE_BUILD"], os.environ["ORLIX_BAZEL_DISK_CACHE"]); print("pass: disk-cache namespace", pin.namespace_for(os.environ["ORLIX_XCODE_VERSION"], os.environ["ORLIX_XCODE_BUILD"]))'
	@DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" test //bazel/feasibility/analysis:all --config=release --config=source --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)" --test_output=errors

__bazel-orlixos: __bazel-feasibility-bootstrap
	@DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" build //OrlixOS/Sources/Session:OrlixOS --compilation_mode=dbg --config=release --config=source --apple_platform_type=ios --ios_multi_cpus=sim_arm64 --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)"
	@test -s bazel-bin/OrlixOS/Sources/Session/libOrlixOS.a

__bazel-orlix-app: __bazel-feasibility-bootstrap
	@DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" build //Orlix:Orlix //bazel/product:kernel_composition --compilation_mode=dbg --config=release --config=promoted --apple_platform_type=ios --ios_multi_cpus=sim_arm64 --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_TCTI_ISA_PREPARED="$(ORLIX_TCTI_ISA_PREPARED)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)"
	@ipa="$$(/usr/bin/find "$(ORLIX_BAZEL_OUTPUT_BASE)/execroot/_main/bazel-out" -path '*/bin/Orlix/Orlix.ipa' ! -path '*/runfiles/*' -print | /usr/bin/head -n 1)"; \
	test -n "$$ipa" && test -s "$$ipa" || { echo "missing //Orlix:Orlix ipa" >&2; exit 1; }; \
	/usr/bin/unzip -l "$$ipa" | /usr/bin/grep -F 'Payload/Orlix.app/Info.plist'; \
	/usr/bin/unzip -p "$$ipa" Payload/Orlix.app/Info.plist | /usr/bin/grep -F -q '15.0'; \
	ipa_work="$$(mktemp -d "$${TMPDIR:-/tmp}/orlix-ipa.XXXXXX")"; \
	/usr/bin/unzip -q "$$ipa" -d "$$ipa_work"; \
	test -x "$$ipa_work/Payload/Orlix.app/Orlix"; \
	/usr/bin/nm -gU "$$ipa_work/Payload/Orlix.app/Orlix" | /usr/bin/grep -E '[[:space:]]T[[:space:]]+_OrlixBoot' >/dev/null || { echo "missing defined _OrlixBoot in //Orlix:Orlix" >&2; /usr/bin/nm -gU "$$ipa_work/Payload/Orlix.app/Orlix" | /usr/bin/grep OrlixBoot >&2 || true; rm -rf "$$ipa_work"; exit 1; }; \
	if /usr/bin/nm "$$ipa_work/Payload/Orlix.app/Orlix" | /usr/bin/grep -E '[[:space:]]U[[:space:]]+_OrlixBoot' >/dev/null; then echo "_OrlixBoot must not remain undefined" >&2; rm -rf "$$ipa_work"; exit 1; fi; \
	/usr/bin/nm -gU "$$ipa_work/Payload/Orlix.app/Orlix" | /usr/bin/grep -E '[[:space:]]T[[:space:]]+_arch_boot_entry' >/dev/null || { echo "missing defined _arch_boot_entry in //Orlix:Orlix" >&2; /usr/bin/nm "$$ipa_work/Payload/Orlix.app/Orlix" | /usr/bin/grep arch_boot_entry >&2 || true; rm -rf "$$ipa_work"; exit 1; }; \
	if /usr/bin/nm "$$ipa_work/Payload/Orlix.app/Orlix" | /usr/bin/grep -E '[[:space:]]U[[:space:]]+_arch_boot_entry' >/dev/null; then echo "_arch_boot_entry must not remain undefined" >&2; rm -rf "$$ipa_work"; exit 1; fi; \
	lock_buildset="$$(python3 -c 'import json; print(json.load(open("$(CURDIR)/artifacts.lock.json"))["buildset"])')"; \
	test "$${#lock_buildset}" -eq 64 || { echo "artifacts.lock.json missing buildset" >&2; rm -rf "$$ipa_work"; exit 1; }; \
	stamp="$$(/usr/bin/find "$$ipa_work/Payload/Orlix.app" -name 'locked-buildset.json' -print | /usr/bin/head -n 1)"; \
	test -s "$$stamp" || { echo "promoted //Orlix:Orlix must embed locked-buildset.json" >&2; rm -rf "$$ipa_work"; exit 1; }; \
	rg -F -q "$$lock_buildset" "$$stamp" || { echo "IPA lock stamp does not match artifacts.lock.json" >&2; rm -rf "$$ipa_work"; exit 1; }; \
	if rg -q ':latest' "$$stamp"; then echo "locked-buildset.json must not use mutable latest" >&2; rm -rf "$$ipa_work"; exit 1; fi; \
	rg -F -q "$$lock_buildset" bazel-bin/bazel/product/kernel_composition/composition.json || { echo "promoted kernel composition must record the locked buildset" >&2; rm -rf "$$ipa_work"; exit 1; }; \
	PYTHONPATH="$(CURDIR)/make" python3 -c "from pathlib import Path; import ios15_simulator_gate as gate; gate.validate_simulator_app(Path('$$ipa_work/Payload/Orlix.app'))"; \
	mkdir -p "$(ORLIX_BUILD_ROOT)/Bazel/proof"; \
	/usr/bin/nm -gU "$$ipa_work/Payload/Orlix.app/Orlix" | /usr/bin/grep -E '[[:space:]]T[[:space:]]+_OrlixBoot|[[:space:]]T[[:space:]]+_arch_boot_entry' > "$(ORLIX_BUILD_ROOT)/Bazel/proof/kernel-dependency.evidence"; \
	test -s "$(ORLIX_BUILD_ROOT)/Bazel/proof/kernel-dependency.evidence" || { echo "missing kernel-dependency evidence from IPA" >&2; rm -rf "$$ipa_work"; exit 1; }; \
	rm -rf "$$ipa_work"

__bazel-orlix-archive: __bazel-version-check
	@test -n "$(ORLIX_DEVELOPMENT_TEAM)" || { echo "ORLIX_DEVELOPMENT_TEAM is required to archive for TestFlight" >&2; exit 1; }
	@test -d "$(ORLIX_PINNED_DEVELOPER_DIR)" || { echo "missing pinned Xcode developer directory: $(ORLIX_PINNED_DEVELOPER_DIR)" >&2; exit 1; }
	@DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" build //Orlix:Orlix --compilation_mode=opt --config=release --config=promoted --apple_platform_type=ios --ios_multi_cpus=arm64 --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_TCTI_ISA_PREPARED="$(ORLIX_TCTI_ISA_PREPARED)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)"
	@ipa="$$(/usr/bin/find "$(ORLIX_BAZEL_OUTPUT_BASE)/execroot/_main/bazel-out" -path '*/bin/Orlix/Orlix.ipa' ! -path '*/runfiles/*' -print | /usr/bin/head -n 1)"; \
	test -n "$$ipa" && test -s "$$ipa" || { echo "missing device //Orlix:Orlix ipa" >&2; exit 1; }; \
	mkdir -p "$(ORLIX_BETA_ARCHIVE_PATH)/Products/Applications" "$(ORLIX_BETA_EXPORT_DIR)"; \
	rm -rf "$(ORLIX_BETA_ARCHIVE_PATH)/Products/Applications/Orlix.app"; \
	/usr/bin/unzip -q "$$ipa" -d "$(ORLIX_BETA_ARCHIVE_DIR)/ipa-work"; \
	mv "$(ORLIX_BETA_ARCHIVE_DIR)/ipa-work/Payload/Orlix.app" "$(ORLIX_BETA_ARCHIVE_PATH)/Products/Applications/Orlix.app"; \
	rm -rf "$(ORLIX_BETA_ARCHIVE_DIR)/ipa-work"; \
	/bin/cp "$$ipa" "$(ORLIX_BETA_IPA_PATH)"

__tcti-isa-restore:
	@set -euo pipefail; \
	archive="$(ORLIX_TCTI_ISA_ARCHIVE)"; \
	pin_file="$(ORLIX_TCTI_ISA_ARCHIVE_SHA256)"; \
	dest="$(ORLIX_TCTI_ISA_PREPARED)"; \
	test -s "$$archive" || { echo "missing pinned ISA archive: $$archive" >&2; exit 1; }; \
	test -s "$$pin_file" || { echo "missing ISA archive pin: $$pin_file" >&2; exit 1; }; \
	expected="$$(/usr/bin/awk '{print $$1; exit}' "$$pin_file")"; \
	test "$${#expected}" -eq 64 || { echo "invalid ISA archive pin" >&2; exit 1; }; \
	actual="$$(/usr/bin/shasum -a 256 "$$archive" | /usr/bin/awk '{print $$1}')"; \
	test "$$actual" = "$$expected" || { echo "ISA archive digest mismatch: $$actual != $$expected" >&2; exit 1; }; \
	mkdir -p "$$dest"; \
	if [ ! -s "$$dest/source_manifest.def" ]; then \
		/usr/bin/tar -C "$$dest" -xzf "$$archive"; \
	fi; \
	for isa_name in $(ORLIX_TCTI_ISA_ARTIFACTS); do \
		test -s "$$dest/$$isa_name" || { echo "missing prepared ISA artifact: $$dest/$$isa_name" >&2; exit 1; }; \
	done

__bazel-ios15-simulator-gate: __bazel-feasibility-bootstrap __tcti-isa-restore
	@set -euo pipefail; \
	test -n "$(ORLIX_IOS15_SIMULATOR_ID)" || { echo "ORLIX_IOS15_SIMULATOR_ID is required" >&2; exit 1; }; \
	runtime="$$(xcrun simctl list devices -j | jq -r --arg id "$(ORLIX_IOS15_SIMULATOR_ID)" '.devices | to_entries[] | select(.key | contains("iOS-15-5")) | .value[] | select(.udid == $$id and .isAvailable == true) | .udid')"; \
	test "$$runtime" = "$(ORLIX_IOS15_SIMULATOR_ID)" || { echo "the selected simulator is not an available iOS 15.5 device" >&2; exit 1; }; \
	device_name="$$(xcrun simctl list devices -j | jq -r --arg id "$(ORLIX_IOS15_SIMULATOR_ID)" '.devices | to_entries[] | select(.key | contains("iOS-15-5")) | .value[] | select(.udid == $$id) | .name')"; \
	test -n "$$device_name" || { echo "missing iOS 15.5 simulator name for $(ORLIX_IOS15_SIMULATOR_ID)" >&2; exit 1; }; \
	xcrun simctl bootstatus "$(ORLIX_IOS15_SIMULATOR_ID)" -b; \
	PYTHONPATH="$(CURDIR)/make" python3 -m unittest test_ios15_simulator_gate; \
	result_dir="$(ORLIX_BUILD_ROOT)/iOS15"; \
	result_log="$$result_dir/Orlix-iOS15.log"; \
	mkdir -p "$$result_dir"; \
	DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" test //Orlix:OrlixUITests --compilation_mode=dbg --config=release --config=promoted --apple_platform_type=ios --ios_multi_cpus=sim_arm64 --ios_simulator_version=15.5 --ios_simulator_device="$$device_name" --test_filter=AppLaunchSmokeUITests/testLaunchCapturesScreenshot --test_output=errors --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_TCTI_ISA_PREPARED="$(ORLIX_TCTI_ISA_PREPARED)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)" 2>&1 | tee "$$result_log"; \
	ipa="$$(/usr/bin/find "$(ORLIX_BAZEL_OUTPUT_BASE)/execroot/_main/bazel-out" -path '*/bin/Orlix/Orlix.ipa' ! -path '*/runfiles/*' -print | /usr/bin/head -n 1)"; \
	test -n "$$ipa" && test -s "$$ipa" || { echo "missing //Orlix:Orlix ipa after iOS 15 UI tests" >&2; exit 1; }; \
	ipa_work="$$(mktemp -d "$${TMPDIR:-/tmp}/orlix-ios15.XXXXXX")"; \
	/usr/bin/unzip -q "$$ipa" -d "$$ipa_work"; \
	PYTHONPATH="$(CURDIR)/make" python3 -c "from pathlib import Path; import ios15_simulator_gate as gate; gate.validate_simulator_app(Path('$$ipa_work/Payload/Orlix.app')); print('pass: iOS 15 app does not required-load AppIntents or ActivityKit')"; \
	rm -rf "$$ipa_work"

__bazel-hostadapter: __bazel-feasibility-bootstrap
	@DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" build //OrlixHostAdapter/Sources:OrlixHostAdapter --compilation_mode=dbg --config=release --config=source --apple_platform_type=ios --ios_multi_cpus=sim_arm64 --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)"

__bazel-product-composition: __bazel-kernel-uapi __bazel-hostadapter
	@DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" build //bazel/product:kernel_composition --config=release --config=source --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_TCTI_ISA_PREPARED="$(ORLIX_TCTI_ISA_PREPARED)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)"
	@test -s bazel-bin/bazel/product/kernel_composition/composition.json
	@rg -q '"linux_archive"' bazel-bin/bazel/product/kernel_composition/composition.json
	@rg -q '"hostadapter"' bazel-bin/bazel/product/kernel_composition/composition.json
	@rg -q '"boot"' bazel-bin/bazel/product/kernel_composition/composition.json
	@rg -q '"linked_symbol": "_arch_boot_entry"' bazel-bin/bazel/product/kernel_composition/composition.json
	@rg -F -q '"undefined_kernel_symbols": []' bazel-bin/bazel/product/kernel_composition/composition.json
	@rg -q '"xcframework": null' bazel-bin/bazel/product/kernel_composition/composition.json
	@if rg -q 'Makefile' bazel-bin/bazel/product/kernel_composition/composition.json; then echo "composition must not invoke wrapper Makefiles" >&2; exit 1; fi

__bazel-cache-equivalence: __bazel-kernel-uapi
	@set -euo pipefail; \
	left="$$(/usr/bin/tr -d '[:space:]' < bazel-bin/bazel/feasibility/kernel/uapi/uapi.sha256)"; \
	test "$${#left}" -eq 64; \
	DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" build //bazel/feasibility/kernel:uapi --nouse_action_cache --config=release --config=source --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_KBUILD_PERSIST= --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)"; \
	right="$$(/usr/bin/tr -d '[:space:]' < bazel-bin/bazel/feasibility/kernel/uapi/uapi.sha256)"; \
	test "$$left" = "$$right" || { echo "cache-on vs cache-off UAPI digest mismatch: $$left != $$right" >&2; exit 1; }
