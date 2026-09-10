ORLIX_BAZEL_CACHE_ROOT ?= $(HOME)/Library/Caches/Orlix/Bazel
ORLIX_BAZEL_VERSION ?= 9.2.0
ORLIX_XCODE_VERSION ?= 26.6
ORLIX_XCODE_BUILD ?= 17F113
ORLIX_BAZEL_DISK_CACHE ?= $(ORLIX_BAZEL_CACHE_ROOT)/disk-cache/bazel-$(ORLIX_BAZEL_VERSION)-xcode-$(ORLIX_XCODE_BUILD)
ORLIX_BAZEL_REPOSITORY_CACHE ?= $(ORLIX_BAZEL_CACHE_ROOT)/repository-cache
ORLIX_BAZEL_OUTPUT_BASE ?= $(ORLIX_BUILD_ROOT)/Bazel/output-base
ORLIX_BAZEL_DESTINATION ?= iphonesimulator
ORLIX_BAZEL_COMPILATION_MODE ?= dbg
ORLIX_BAZEL_COMPONENT_MODE ?= promoted
ORLIX_BAZEL_IOS_CPU = $(if $(filter iphoneos,$(ORLIX_BAZEL_DESTINATION)),arm64,sim_arm64)
ORLIX_BAZEL_KERNEL_FLAGS = --compilation_mode=$(ORLIX_BAZEL_COMPILATION_MODE) --config=$(PROFILE) --config=source --apple_platform_type=ios --ios_multi_cpus=$(ORLIX_BAZEL_IOS_CPU) --platforms=@build_bazel_apple_support//platforms:ios_$(ORLIX_BAZEL_IOS_CPU)
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
.PHONY: __bazel-kernel-uapi __bazel-kernel-uapi-variants __bazel-mlibc-from-uapi __bazel-live-activity-smoke __bazel-promote-uapi __bazel-promote-mlibc __bazel-promote-rootfs __bazel-publish-uapi __bazel-publish-mlibc __bazel-publish-rootfs __bazel-lock-proposal __bazel-lock-from-signed __bazel-reconstruct __bazel-reconstruct-source __bazel-substitute-promoted __bazel-hostadapter __bazel-orlixos __bazel-orlix-app __bazel-orlix-archive __bazel-ios15-simulator-gate __bazel-product-composition __bazel-cache-equivalence __bazel-kernel-boot __bazel-proof-graph __bazel-apple-routing-check __bazel-prove-matrix __bazel-gc
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
	@case "$(ORLIX_BAZEL_DESTINATION)" in iphoneos|iphonesimulator) ;; *) echo "unsupported Bazel destination: $(ORLIX_BAZEL_DESTINATION)" >&2; exit 1 ;; esac
	@case "$(ORLIX_BAZEL_COMPONENT_MODE)" in source|promoted) ;; *) echo "unsupported Bazel component mode: $(ORLIX_BAZEL_COMPONENT_MODE)" >&2; exit 1 ;; esac
	@case "$(ORLIX_BAZEL_COMPILATION_MODE)" in dbg|opt) ;; *) echo "unsupported Bazel compilation mode: $(ORLIX_BAZEL_COMPILATION_MODE)" >&2; exit 1 ;; esac
	@test -d "$(ORLIX_PINNED_DEVELOPER_DIR)" || { echo "missing pinned Xcode developer directory: $(ORLIX_PINNED_DEVELOPER_DIR)" >&2; exit 1; }
	@PYTHONPATH="$(CURDIR)/bazel/config" ORLIX_XCODE_VERSION="$(ORLIX_XCODE_VERSION)" ORLIX_XCODE_BUILD="$(ORLIX_XCODE_BUILD)" ORLIX_BAZEL_DISK_CACHE="$(ORLIX_BAZEL_DISK_CACHE)" python3 -c 'import os, toolchain_pin as pin; pin.require_identity(os.environ["ORLIX_XCODE_VERSION"], os.environ["ORLIX_XCODE_BUILD"], os.environ["ORLIX_BAZEL_DISK_CACHE"])'
	@test "$$(DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" /usr/bin/xcodebuild -version | /usr/bin/sed -n '1p')" = "Xcode $(ORLIX_XCODE_VERSION)" || { echo "DEVELOPER_DIR is not Xcode $(ORLIX_XCODE_VERSION)" >&2; exit 1; }
	@test "$$(DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" /usr/bin/xcodebuild -version | /usr/bin/sed -n '2p')" = "Build version $(ORLIX_XCODE_BUILD)" || { echo "DEVELOPER_DIR is not Build version $(ORLIX_XCODE_BUILD)" >&2; exit 1; }
	@PYTHONPATH="$(CURDIR)/bazel/config" python3 -c 'import toolchain_pin; toolchain_pin.capture_manifest("$(ORLIX_PINNED_DEVELOPER_DIR)", "$(ORLIX_BAZEL)", "$(ORLIX_BUILD_ROOT)/Bazel/toolchain.json")'
	@mkdir -p "$(ORLIX_BAZEL_DISK_CACHE)" "$(ORLIX_BAZEL_REPOSITORY_CACHE)" "$(ORLIX_BAZEL_OUTPUT_BASE)"
	@DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" mod deps --lockfile_mode=error --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)"
	@DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" build //bazel/config:all --config=release --config=source --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)"

__bazel-apple-smoke: __bazel-feasibility-bootstrap
	@DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" build //bazel/feasibility/apple:SmokeApp --compilation_mode=dbg --config=release --config=source --ios_multi_cpus=sim_arm64 --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)"

__bazel-apple-dependency-smoke: __bazel-feasibility-bootstrap
	@DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" build //bazel/feasibility/apple:DependencySmokeApp --compilation_mode=dbg --config=release --config=source --ios_multi_cpus=sim_arm64 --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)"

__bazel-native-archives: __bazel-feasibility-bootstrap
	@DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" build //bazel/feasibility/native:apple_archives --config=release --config=source --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)"

__bazel-ghostty-archives: __bazel-feasibility-bootstrap
	@DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" build //bazel/feasibility/native:ghostty_ios_simulator //bazel/feasibility/native:ghostty_ios_device //bazel/feasibility/native:ghostty_macos --config=release --config=source --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)"

__bazel-ssh-archives: __bazel-feasibility-bootstrap
	@DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" build //bazel/feasibility/native:ssh_archives --config=release --config=source --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)"

__bazel-native-dependency-smoke: __bazel-native-archives
	@DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" build //bazel/feasibility/apple:NativeDependencySmokeApp --compilation_mode=dbg --config=release --config=source --ios_multi_cpus=sim_arm64 --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)"
	@DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" build //bazel/feasibility/apple:NativeDependencySmokeApp --compilation_mode=dbg --config=release --config=source --ios_multi_cpus=arm64 --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)"
	@DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" build //bazel/feasibility/apple:NativeDependencyMacSmokeApp --compilation_mode=dbg --config=release --config=source --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)"

__bazel-kernel-uapi: __bazel-feasibility-bootstrap
	@mkdir -p "$(ORLIX_KBUILD_PERSIST)"
	@DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" build //bazel/feasibility/kernel:uapi --config=release --config=source --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_KBUILD_PERSIST="$(ORLIX_KBUILD_PERSIST)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)"
	@sh bazel/feasibility/kernel/uapi_contract_test.sh

__bazel-kernel-uapi-variants: __bazel-kernel-uapi
	@DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" build //bazel/feasibility/kernel:uapi_iphonesimulator_release //bazel/feasibility/kernel:uapi_iphonesimulator_development //bazel/feasibility/kernel:uapi_iphoneos_release //bazel/feasibility/kernel:uapi_iphoneos_development --config=release --config=source --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)"
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
		DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$$$$promote/$$$$side/output-base" build $(2) --nouse_action_cache --remote_cache= --remote_executor= --config=release --config=source --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_KBUILD_PERSIST= --disk_cache="$$$$promote/$$$$side/disk" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)"; \
		digest_file="$$$$(/usr/bin/find "$$$$promote/$$$$side/output-base" -path '*/$(3)' -print | /usr/bin/head -n 1)"; \
		test -n "$$$$digest_file" || { echo "missing $(3) for promote $(1) $$$$side" >&2; exit 1; }; \
		/bin/cp "$$$$digest_file" "$$$$promote/$$$$side/digest.sha256"; \
		if [ "$$$$side" = a ]; then first_tree="$$$$(/usr/bin/dirname "$$$$digest_file")"; else second_tree="$$$$(/usr/bin/dirname "$$$$digest_file")"; fi; \
	done; \
	lock_before="$$$$(/usr/bin/shasum -a 256 "$(CURDIR)/artifacts.lock.json")"; \
	digest="$$$$(PYTHONPATH="$(CURDIR)/bazel/promotion" python3 "$(CURDIR)/bazel/promotion/compare.py" "$$$$promote/a/digest.sha256" "$$$$promote/b/digest.sha256" --first-tree "$$$$first_tree" --second-tree "$$$$second_tree" --component $(1) --proposal "$$$$promote/$(1)-proposal.json" --sbom "$$$$promote/$(1)-sbom.json" --in-toto "$$$$promote/$(1)-in-toto.json" --lock-proposal "$$$$promote/$(1)-lock-proposal.json" --lock "$(CURDIR)/artifacts.lock.json")"; \
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
	if env -u ORLIX_COSIGN_KEY -u ORLIX_PROMOTE_ARTIFACT PYTHONPATH="$(CURDIR)/bazel/promotion" python3 "$(CURDIR)/bazel/promotion/sign.py" --component $(1) --digest "$$$$digest" --proposal "$$$$promote/$(1)-signed.json"; then echo "unsigned promote must not Cosign-sign" >&2; exit 1; fi; \
	test ! -e "$$$$promote/$(1)-signed.json"
endef
$(eval $(call ORLIX_BAZEL_PROMOTE,uapi,//bazel/feasibility/kernel:uapi,feasibility/kernel/uapi/uapi.sha256))
$(eval $(call ORLIX_BAZEL_PROMOTE,mlibc,//bazel/feasibility/mlibc:sysroot,feasibility/mlibc/sysroot/sysroot.sha256))
$(eval $(call ORLIX_BAZEL_PROMOTE,rootfs,//bazel/feasibility/rootfs:rootfs,feasibility/rootfs/rootfs/source-input.sha256))

define ORLIX_BAZEL_PUBLISH
__bazel-publish-$(1): __bazel-version-check
	@set -euo pipefail; \
	test -n "$$$${ORLIX_COSIGN_KEY:-}" || { echo "ORLIX_COSIGN_KEY is required to publish $(1)" >&2; exit 1; }; \
	if [ -n "$$$${ORLIX_COSIGN_KEY_PASSWORD:-}" ]; then export COSIGN_PASSWORD="$$$$ORLIX_COSIGN_KEY_PASSWORD"; fi; \
	promote="$(ORLIX_BUILD_ROOT)/Bazel/promote/$(1)"; \
	digest_file="$$$$promote/a/digest.sha256"; \
	test -s "$$$$digest_file" || { echo "missing unsigned digest $$$$digest_file; run make __bazel-promote-$(1) first" >&2; exit 1; }; \
	digest="$$$$(tr -d '[:space:]' < "$$$$digest_file")"; \
	tree="$$$$(/usr/bin/find "$$$$promote/a/output-base" -path '*/$(2)' -print | /usr/bin/head -n 1)"; \
	test -n "$$$$tree" || { echo "missing $(2) tree for publish $(1)" >&2; exit 1; }; \
	artifact="$$$$(dirname "$$$$tree")"; \
	test -d "$$$$artifact" || { echo "missing component directory $$$$artifact" >&2; exit 1; }; \
	PYTHONPATH="$(CURDIR)/bazel/promotion" python3 -c 'import json,sys; from pathlib import Path; import compare; p=json.load(open(sys.argv[1])); assert p["signed"] is False and p["component"] == sys.argv[3] and p["unsigned_digest"] == sys.argv[4]; assert p["output_tree_digest"] == compare.tree_digest(Path(sys.argv[2])), "component changed after dual-build comparison"' "$$$$promote/$(1)-proposal.json" "$$$$artifact" "$(1)" "$$$$digest"; \
	PYTHONPATH="$(CURDIR)/bazel/promotion" python3 "$(CURDIR)/bazel/promotion/sign.py" --component $(1) --digest "$$$$digest" --artifact "$$$$artifact" --proposal "$$$$promote/$(1)-signed.json"; \
	python3 -c 'import json,sys; p=json.load(open(sys.argv[1])); assert p.get("signed") is True and p.get("oci_digest","").startswith("sha256:"), p' "$$$$promote/$(1)-signed.json"; \
	PYTHONPATH="$(CURDIR)/bazel/promotion" python3 "$(CURDIR)/bazel/promotion/publish.py" --proposal "$$$$promote/$(1)-signed.json"
endef
$(eval $(call ORLIX_BAZEL_PUBLISH,uapi,feasibility/kernel/uapi/uapi.sha256))
$(eval $(call ORLIX_BAZEL_PUBLISH,mlibc,feasibility/mlibc/sysroot/sysroot.sha256))
$(eval $(call ORLIX_BAZEL_PUBLISH,rootfs,feasibility/rootfs/rootfs/source-input.sha256))

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
	command -v oras >/dev/null || { echo "oras is required to reconstruct" >&2; exit 1; }; \
	command -v cosign >/dev/null || { echo "cosign is required to reconstruct" >&2; exit 1; }; \
	test -n "$${ORLIX_COSIGN_PUB:-}$${ORLIX_COSIGN_KEY:-}" || { echo "ORLIX_COSIGN_PUB is required to reconstruct" >&2; exit 1; }; \
	if [ -n "$${ORLIX_COSIGN_KEY_PASSWORD:-}" ]; then export COSIGN_PASSWORD="$$ORLIX_COSIGN_KEY_PASSWORD"; fi; \
	PYTHONPATH="$(CURDIR)/bazel/promotion" python3 "$(CURDIR)/bazel/promotion/reconstruct.py" --lock "$(CURDIR)/artifacts.lock.json" --out-dir "$(ORLIX_BUILD_ROOT)/Bazel/reconstruct"

__bazel-substitute-promoted: __bazel-reconstruct
	@set -euo pipefail; \
	lock_before="$$(/usr/bin/shasum -a 256 "$(CURDIR)/artifacts.lock.json")"; \
	mkdir -p "$(ORLIX_BUILD_ROOT)/Bazel/proof"; \
	PYTHONPATH="$(CURDIR)/bazel/promotion" python3 "$(CURDIR)/bazel/promotion/substitute.py" \
		--lock "$(CURDIR)/artifacts.lock.json" \
		--reconstruct-dir "$(ORLIX_BUILD_ROOT)/Bazel/reconstruct" \
		--out "$(ORLIX_BUILD_ROOT)/Bazel/proof/promoted-components.json" \
		--stage "$(CURDIR)/bazel/promotion/imported"; \
	python3 -c 'import json,sys; p=json.load(open(sys.argv[1])); assert p.get("kind")=="promoted-components"; assert set(p.get("components",{}))=={"uapi","mlibc","rootfs"}, p' "$(ORLIX_BUILD_ROOT)/Bazel/proof/promoted-components.json"; \
	if rg -q ':latest' "$(ORLIX_BUILD_ROOT)/Bazel/proof/promoted-components.json"; then echo "promoted-components.json must not use mutable latest" >&2; exit 1; fi; \
	lock_after="$$(/usr/bin/shasum -a 256 "$(CURDIR)/artifacts.lock.json")"; \
	test "$$lock_before" = "$$lock_after" || { echo "promoted substitute mutated artifacts.lock.json" >&2; exit 1; }

__bazel-reconstruct-source: __bazel-feasibility-bootstrap __bazel-reconstruct
	@set -euo pipefail; \
	cold="$$(/usr/bin/mktemp -d "$(ORLIX_BUILD_ROOT)/Bazel/reconstruct-source.XXXXXX")"; \
	DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$$cold/output-base" build //bazel/feasibility/rootfs:rootfs --nouse_action_cache --config=release --config=source --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_KBUILD_PERSIST= --action_env=ORLIX_COMPILER_LAUNCHER= --action_env=CCACHE_DISABLE=1 --disk_cache= --remote_cache= --remote_executor= --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)"; \
	buildset="$$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["buildset"])' "$(CURDIR)/artifacts.lock.json")"; \
	for component in uapi mlibc rootfs; do \
		case "$$component" in \
			uapi) label=//bazel/feasibility/kernel:uapi; marker=uapi.sha256 ;; \
			mlibc) label=//bazel/feasibility/mlibc:sysroot; marker=sysroot.sha256 ;; \
			rootfs) label=//bazel/feasibility/rootfs:rootfs; marker=source-input.sha256 ;; \
		esac; \
		rel="$$(DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$$cold/output-base" cquery "$$label" --config=release --config=source --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_KBUILD_PERSIST= --action_env=ORLIX_COMPILER_LAUNCHER= --action_env=CCACHE_DISABLE=1 --output=files | awk -v marker="$$marker" 'substr($$0,length($$0)-length(marker)) == "/" marker {path=$$0; count++} END {if(count != 1) exit 1; print path}')"; \
		tree="$$(/usr/bin/dirname "$$cold/output-base/execroot/_main/$$rel")"; \
		PYTHONPATH="$(CURDIR)/bazel/promotion" python3 -c 'import compare,sys; print(sys.argv[1], compare.compare_trees(sys.argv[2], sys.argv[3]))' "$$component" "$$tree" "$(ORLIX_BUILD_ROOT)/Bazel/reconstruct/$$buildset/$$component"; \
	done

__bazel-mlibc-from-uapi: __bazel-kernel-uapi
	@mkdir -p "$(ORLIX_KBUILD_PERSIST)"
	@DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" build //bazel/feasibility/mlibc:sysroot --config=release --config=source --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_KBUILD_PERSIST="$(ORLIX_KBUILD_PERSIST)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)"
	@aquery_out="$$(mktemp -t orlix-mlibc-aquery)"; \
	DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" aquery 'mnemonic("OrlixMLibCSysroot", //bazel/feasibility/mlibc:sysroot)' --output=text --config=release --config=source --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_KBUILD_PERSIST="$(ORLIX_KBUILD_PERSIST)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)" > "$$aquery_out"; \
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
	@DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" build //bazel/feasibility/packages:true --config=release --config=source --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_KBUILD_PERSIST="$(ORLIX_KBUILD_PERSIST)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)"
	@aquery_out="$$(mktemp -t orlix-package-aquery)"; \
	DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" aquery 'mnemonic("OrlixGuestPackage", //bazel/feasibility/packages:true)' --output=text --config=release --config=source --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_KBUILD_PERSIST="$(ORLIX_KBUILD_PERSIST)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)" > "$$aquery_out"; \
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
	@DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" build //bazel/feasibility/packages:coreutils --config=release --config=source --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_KBUILD_PERSIST="$(ORLIX_KBUILD_PERSIST)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)"
	@aquery_out="$$(mktemp -t orlix-coreutils-aquery)"; \
	DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" aquery 'mnemonic("OrlixGuestPackage", //bazel/feasibility/packages:coreutils)' --output=text --config=release --config=source --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_KBUILD_PERSIST="$(ORLIX_KBUILD_PERSIST)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)" > "$$aquery_out"; \
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
	@DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" build //bazel/feasibility/packages:bash --config=release --config=source --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_KBUILD_PERSIST="$(ORLIX_KBUILD_PERSIST)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)"
	@aquery_out="$$(mktemp -t orlix-bash-aquery)"; \
	DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" aquery 'mnemonic("OrlixGuestPackage", //bazel/feasibility/packages:bash)' --output=text --config=release --config=source --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_KBUILD_PERSIST="$(ORLIX_KBUILD_PERSIST)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)" > "$$aquery_out"; \
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
	@DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" build //bazel/feasibility/packages:$(1) --config=release --config=source --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_KBUILD_PERSIST="$(ORLIX_KBUILD_PERSIST)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)"
	@aquery_out="$$$$(mktemp -t orlix-$(1)-aquery)"; \
	DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" aquery 'mnemonic("OrlixGuestPackage", //bazel/feasibility/packages:$(1))' --output=text --config=release --config=source --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_KBUILD_PERSIST="$(ORLIX_KBUILD_PERSIST)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)" > "$$$$aquery_out"; \
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
	@DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" build //bazel/feasibility/rootfs:rootfs --config=release --config=source --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_KBUILD_PERSIST="$(ORLIX_KBUILD_PERSIST)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)"
	@aquery_out="$$(mktemp -t orlix-rootfs-aquery)"; \
	DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" aquery 'mnemonic("OrlixRootfs", //bazel/feasibility/rootfs:rootfs)' --output=text --config=release --config=source --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_KBUILD_PERSIST="$(ORLIX_KBUILD_PERSIST)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)" > "$$aquery_out"; \
	rg -q 'Mnemonic: OrlixRootfs' "$$aquery_out" || { echo "missing OrlixRootfs action" >&2; rm -f "$$aquery_out"; exit 1; }; \
	rg -q 'gen_init_cpio.c' "$$aquery_out" || { echo "rootfs must compile upstream Linux gen_init_cpio.c" >&2; rm -f "$$aquery_out"; exit 1; }; \
	if rg '^  Inputs:' "$$aquery_out" | rg -q 'kbuild-archive.tar'; then echo "rootfs must not consume the Kernel Kbuild archive" >&2; rm -f "$$aquery_out"; exit 1; fi; \
	if rg '^  Inputs:' "$$aquery_out" | rg -q 'OrlixOS/Sources/make'; then echo "rootfs must not consume OrlixOS wrapper Make" >&2; rm -f "$$aquery_out"; exit 1; fi; \
	rm -f "$$aquery_out"
	@rg -q 'base_packages=bash coreutils grep findutils e2fsprogs jq curl zsh' bazel-bin/bazel/feasibility/rootfs/rootfs/payload-metadata.txt
	@test -s bazel-bin/bazel/feasibility/rootfs/rootfs/initramfs.cpio.gz
	@test -s bazel-bin/bazel/feasibility/rootfs/rootfs/base.ext4
	@test -s bazel-bin/bazel/feasibility/rootfs/rootfs/state.ext4
	@rg -q 'init=/init' bazel-bin/bazel/feasibility/rootfs/rootfs/payload-metadata.txt
	@/usr/bin/gzip -t bazel-bin/bazel/feasibility/rootfs/rootfs/initramfs.cpio.gz

__bazel-live-activity-smoke: __bazel-feasibility-bootstrap
	@DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" build //bazel/feasibility/apple:LiveActivitySmoke --compilation_mode=dbg --config=release --config=source --ios_multi_cpus=sim_arm64 --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)"

__bazel-feasibility-xcodeproj: __bazel-feasibility-bootstrap
	@mkdir -p Build/XcodeProjects
	@DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" PATH="$(ORLIX_BAZEL_TOOL_ROOT)/$(ORLIX_BAZEL_VERSION):$(HOME)/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" run //xcode:feasibility --compilation_mode=dbg --config=release --config=source --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)"

__bazel-xcode-cloud-project-check:
	@test -d OrlixCloud.xcodeproj
	@test -f OrlixCloud.xcodeproj/xcshareddata/xcschemes/OrlixCloud.xcscheme
	@rg -q '__bazel-feasibility-bootstrap' OrlixCloud.xcodeproj/project.pbxproj
	@rg -q 'PRODUCT_BUNDLE_IDENTIFIER = com.rudironsoni.Orlix;' OrlixCloud.xcodeproj/project.pbxproj
	@rg -q 'DEVELOPMENT_TEAM = ZQ3L7M567L;' OrlixCloud.xcodeproj/project.pbxproj
	@rg -q 'CODE_SIGN_STYLE = Automatic;' OrlixCloud.xcodeproj/project.pbxproj
	@rg -q 'IPHONEOS_DEPLOYMENT_TARGET = 15.0;' OrlixCloud.xcodeproj/project.pbxproj
	@DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" /usr/bin/xcodebuild -list -project OrlixCloud.xcodeproj | rg -q 'OrlixCloud'

__bazel-migration-inventory:
	@$(ORLIX_RUBY) bazel/migration/inventory.rb --write

__bazel-migration-inventory-check:
	@$(ORLIX_RUBY) bazel/migration/inventory.rb --check

__bazel-kernel-boot: __bazel-feasibility-bootstrap
	@DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" build //OrlixKernel/Sources:OrlixKernelBoot $(ORLIX_BAZEL_KERNEL_FLAGS) --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)"
	@set -euo pipefail; \
	boot_rel="$$(DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" cquery //OrlixKernel/Sources:OrlixKernelBoot $(ORLIX_BAZEL_KERNEL_FLAGS) --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --output=files | awk '/libOrlixKernelBoot[.]lo$$/ {path=$$0; count++} END {if (count != 1) exit 1; print path}')"; \
	boot_lo="$(ORLIX_BAZEL_OUTPUT_BASE)/execroot/_main/$$boot_rel"; \
	test -n "$$boot_lo" && test -s "$$boot_lo" || { echo "missing OrlixKernelBoot .lo" >&2; exit 1; }; \
	/usr/bin/nm -gU "$$boot_lo" | /usr/bin/grep -E -q '[[:space:]]T[[:space:]]_OrlixBoot$$' || { echo "OrlixKernelBoot archive missing defined _OrlixBoot" >&2; exit 1; }
	@DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" build //bazel/feasibility/kernel:macho $(ORLIX_BAZEL_KERNEL_FLAGS) --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_TCTI_ISA_PREPARED="$(ORLIX_TCTI_ISA_PREPARED)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)"
	@set -euo pipefail; \
	macho_rel="$$(DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" cquery //bazel/feasibility/kernel:macho_archive $(ORLIX_BAZEL_KERNEL_FLAGS) --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_TCTI_ISA_PREPARED="$(ORLIX_TCTI_ISA_PREPARED)" --output=files | awk '/OrlixKernel[.]a$$/ {path=$$0; count++} END {if (count != 1) exit 1; print path}')"; \
	macho="$(ORLIX_BAZEL_OUTPUT_BASE)/execroot/_main/$$macho_rel"; \
	test -n "$$macho" && test -s "$$macho" || { echo "missing Mach-O OrlixKernel.a" >&2; exit 1; }; \
	/usr/bin/grep -E '[[:space:]]T[[:space:]]+_arch_boot_entry' "$${macho%/*}/symbols.txt" >/dev/null || /usr/bin/nm -gU "$$macho" | /usr/bin/grep -E '[[:space:]]T[[:space:]]+_arch_boot_entry' >/dev/null || { echo "OrlixKernel.a missing defined _arch_boot_entry" >&2; exit 1; }

__bazel-proof-graph: __bazel-kernel-uapi __bazel-kernel-boot
	@test -s bazel-bin/bazel/feasibility/kernel/uapi/uapi.sha256
	@mkdir -p "$(ORLIX_BUILD_ROOT)/Bazel/proof"
	@set -euo pipefail; \
	toolchain_digest="$$(/usr/bin/shasum -a 256 "$(ORLIX_BUILD_ROOT)/Bazel/toolchain.json" | /usr/bin/awk '{print $$1}')"; \
	extra=(); \
	mlibc_digest="$$(PYTHONPATH="$(CURDIR)/bazel/proof:$(CURDIR)/bazel/promotion" python3 -c 'import graph,sys; s,_=graph.subjects_from_lock(sys.argv[1]); p=graph.select_matching_live_digest(s,"mlibc",sys.argv[2],sys.argv[3]); print(p or "")' "$(CURDIR)/artifacts.lock.json" bazel-bin/bazel/feasibility/mlibc/sysroot/sysroot.sha256 "$(ORLIX_BUILD_ROOT)/Bazel/proof/mlibc-live-mismatch.json")"; \
	rootfs_digest="$$(PYTHONPATH="$(CURDIR)/bazel/proof:$(CURDIR)/bazel/promotion" python3 -c 'import graph,sys; s,_=graph.subjects_from_lock(sys.argv[1]); p=graph.select_matching_live_digest(s,"rootfs",sys.argv[2],sys.argv[3]); print(p or "")' "$(CURDIR)/artifacts.lock.json" bazel-bin/bazel/feasibility/rootfs/rootfs/source-input.sha256 "$(ORLIX_BUILD_ROOT)/Bazel/proof/rootfs-live-mismatch.json")"; \
	if [ -n "$$mlibc_digest" ]; then extra+=(--mlibc-digest "$$mlibc_digest"); fi; \
	if [ -n "$$rootfs_digest" ]; then extra+=(--rootfs-digest "$$rootfs_digest"); fi; \
	if [ -s bazel-bin/Orlix/Orlix.ipa ]; then extra+=(--app-digest "$$(/usr/bin/shasum -a 256 bazel-bin/Orlix/Orlix.ipa | /usr/bin/awk '{print $$1}')"); fi; \
	buildset="$$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1])).get("buildset") or "")' "$(CURDIR)/artifacts.lock.json")"; \
	if [ -n "$$buildset" ]; then extra+=(--buildset-digest "$$buildset"); fi; \
	for tier in kernel-dependency kunit kselftest orlixmlibc syscall-uapi posix-shell jq curl zsh product-integration; do \
		name="$$tier"; [ "$$tier" != orlixmlibc ] || name=mlibc; \
		evidence="$(ORLIX_BUILD_ROOT)/Bazel/proof/$$name.evidence"; \
		if [ -s "$$evidence" ]; then extra+=(--evidence "$$tier=$$evidence"); fi; \
	done; \
	PYTHONPATH="$(CURDIR)/bazel/proof:$(CURDIR)/bazel/promotion" python3 "$(CURDIR)/bazel/proof/graph.py" \
		--out "$(ORLIX_BUILD_ROOT)/Bazel/proof" \
		--lock "$(CURDIR)/artifacts.lock.json" \
		--uapi-digest bazel-bin/bazel/feasibility/kernel/uapi/uapi.sha256 \
		--kernel-digest "$$(/usr/bin/shasum -a 256 bazel-bin/bazel/feasibility/kernel/macho/OrlixKernel.a | /usr/bin/awk '{print $$1}')" \
		--toolchain-digest "$$toolchain_digest" \
		--profile "$(PROFILE)" \
		--destination iphonesimulator \
		"$${extra[@]}"
	@python3 -c 'import json,sys; p=json.load(open(sys.argv[1])); print("proof graph complete:", p["complete"]); sys.exit(0 if p["complete"] else 1)' "$(ORLIX_BUILD_ROOT)/Bazel/proof/index.json"

__bazel-prove-matrix: __bazel-orlix-app __bazel-apple-smoke __bazel-live-activity-smoke __bazel-native-dependency-smoke
	@mkdir -p "$(ORLIX_BUILD_ROOT)/Bazel/proof"
	@DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" build //bazel/feasibility/apple:SmokeApp --compilation_mode=opt --config=release --config=source --ios_multi_cpus=sim_arm64 --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)"
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
	@rg -q '^ORLIX_BAZEL_AUTHORITY \?= 0$$' Makefile
	@rg -F -q '__bazel-orlix-app' Makefile
	@rg -F -q 'ORLIX_BAZEL_COMPONENT_MODE ?= promoted' make/bazel-migration.mk
	@rg -F -q '//bazel/promotion:locked_buildset' Orlix/BUILD.bazel
	@rg -F -q 'name = "kernel_composition"' bazel/product/BUILD.bazel
	@rg -F -q 'name = "OrlixOSFramework"' Orlix/BUILD.bazel
	@rg -F -q '//Orlix:OrlixOSFramework' make/bazel-migration.mk
	@rg -F -q '__tcti-isa-restore' make/bazel-migration.mk
	@rg -F -q '__bazel-lock-proposal' make/bazel-migration.mk
	@rg -F -q '__bazel-substitute-promoted' make/bazel-migration.mk
	@rg -F -q 'promoted-components.json' make/bazel-migration.mk
	@rg -F -q 'oras is required to reconstruct' make/bazel-migration.mk
	@rg -F -q 'cosign is required to reconstruct' make/bazel-migration.mk
	@rg -F -q 'ORLIX_COSIGN_PUB is required to reconstruct' make/bazel-migration.mk
	@rg -F -q -- '--nouse_action_cache' make/bazel-migration.mk
	@rg -F -q 'unsigned promote mutated artifacts.lock.json' make/bazel-migration.mk
	@rg -F -q 'unsigned promote must not Cosign-sign' make/bazel-migration.mk
	@rg -F -q 'ORLIX_BAZEL_PROMOTE,uapi,//bazel/feasibility/kernel:uapi' make/bazel-migration.mk
	@rg -F -q 'ORLIX_BAZEL_PROMOTE,mlibc,//bazel/feasibility/mlibc:sysroot' make/bazel-migration.mk
	@rg -F -q 'ORLIX_BAZEL_PROMOTE,rootfs,//bazel/feasibility/rootfs:rootfs' make/bazel-migration.mk
	@rg -F -q '"name": "__bazel-substitute-promoted"' bazel/migration/legacy-target-map.json
	@rg -F -q 'ORLIX_DEVELOPMENT_TEAM ?= ZQ3L7M567L' Makefile
	@rg -F -q 'ios15_simulator_gate' make/bazel-migration.mk
	@rg -A3 '^ios15-simulator-gate:' Makefile | rg -F -q '__bazel-ios15-simulator-gate'
	@rg -A3 '^beta-archive:' Makefile | rg -F -q '__bazel-orlix-archive'
	@rg -F -q 'name = "OrlixUITests"' Orlix/BUILD.bazel
	@rg -F -q '__bazel-feasibility-xcodeproj' Makefile
	@rg -F -q '__bazel-kernel-uapi' Makefile
	@rg -A2 '^test:' Makefile | rg -F -q '__bazel-matrix-check'
	@rg -A3 '^rebuild:' Makefile | rg -F -q '__bazel-orlix-app'
	@rg -q '^runtime-tests: xcodeproj$$' Makefile
	@rg -F -q 'ORLIX_BAZEL_AUTHORITY),1' Makefile
	@rg -q '^common --repository_cache=~/Library/Caches/Orlix/Bazel/repository-cache$$' .bazelrc
	@PYTHONPATH="$(CURDIR)/bazel/migration" python3 -m unittest test_make_routing

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
	@PYTHONPATH="$(CURDIR)/bazel/promotion" python3 -m unittest test_substitute
	@PYTHONPATH="$(CURDIR)/bazel/proof" python3 -m unittest test_bind
	@PYTHONPATH="$(CURDIR)/bazel/proof" python3 -m unittest test_graph
	@PYTHONPATH="$(CURDIR)/bazel/config" ORLIX_XCODE_VERSION="$(ORLIX_XCODE_VERSION)" ORLIX_XCODE_BUILD="$(ORLIX_XCODE_BUILD)" ORLIX_BAZEL_DISK_CACHE="$(ORLIX_BAZEL_DISK_CACHE)" python3 -c 'import os, toolchain_pin as pin; pin.require_identity(os.environ["ORLIX_XCODE_VERSION"], os.environ["ORLIX_XCODE_BUILD"], os.environ["ORLIX_BAZEL_DISK_CACHE"]); print("pass: disk-cache namespace", pin.namespace_for(os.environ["ORLIX_XCODE_VERSION"], os.environ["ORLIX_XCODE_BUILD"]))'
	@DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" test //bazel/feasibility/analysis:all --config=release --config=source --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)" --test_output=errors

__bazel-orlixos: __bazel-feasibility-bootstrap
	@DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" build //OrlixOS/Sources/Session:OrlixOS //Orlix:OrlixOSFramework //OrlixOSTestApp:OrlixOSTestApp //OrlixOSTestApp:OrlixOSTestAppTests //OrlixOSTestApp:OrlixKernelConformanceTests //OrlixOSTestApp:OrlixMLibCConformanceTests //OrlixOSTestApp:OrlixPackagesConformanceTests //OrlixOSTestApp:OrlixOSRuntimeTests --compilation_mode=dbg --config=release --config=source --apple_platform_type=ios --ios_multi_cpus=sim_arm64 --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_TCTI_ISA_PREPARED="$(ORLIX_TCTI_ISA_PREPARED)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)"
	@test -s bazel-bin/OrlixOS/Sources/Session/libOrlixOS.a

.PHONY: __bazel-test-output-parser __bazel-test-native-smoke __bazel-test-terminal-surface __bazel-test-app __bazel-test-app-architecture
__bazel-test-output-parser __bazel-test-native-smoke __bazel-test-terminal-surface __bazel-test-app __bazel-test-app-architecture: __bazel-feasibility-xcodeproj
	@set -euo pipefail; \
	case "$@" in \
		__bazel-test-app) test_scheme="Orlix Tests"; test_filter="$(or $(ORLIX_APP_TEST_ONLY_TESTING),OrlixTests)" ;; \
		__bazel-test-app-architecture) test_scheme="OrlixOSTestApp Tests"; test_filter="OrlixOSTestAppTests/ArchitectureInvariantTests" ;; \
		__bazel-test-output-parser) test_scheme="OrlixOSTestApp Tests"; test_filter="OrlixOSTestAppTests/OrlixUpstreamTestOutputParserTests" ;; \
		__bazel-test-native-smoke) test_scheme="NativeSmokeTests"; test_filter="NativeSmokeTests/NativeSmokeTests/testMLXMetalLibraryContainsCompiledKernels" ;; \
		__bazel-test-terminal-surface) test_scheme="OrlixUITests"; test_filter="$(or $(ORLIX_APP_TEST_ONLY_TESTING),OrlixUITests/DefaultLocalInstanceUITests/testOpensDefaultLocalInstanceTerminal)" ;; \
	esac; \
	test_filters=("-only-testing:$$test_filter"); \
	if [ "$@" = "__bazel-test-terminal-surface" ] && [ -z "$(ORLIX_APP_TEST_ONLY_TESTING)" ]; then test_filters+=("-only-testing:OrlixUITests/TerminalProductionSSHUITests/testProductionSSHBackgroundPreservesSessionKeyboardAndTyping"); fi; \
	mkdir -p "$(ORLIX_BUILD_ROOT)/Bazel/proof"; \
	result_dir="$$(mktemp -d "$(ORLIX_BUILD_ROOT)/Bazel/proof/$(patsubst __bazel-test-%,%,$@).XXXXXX")"; \
	DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" /usr/bin/xcodebuild \
		-project "$(CURDIR)/Build/XcodeProjects/OrlixBazelFeasibility.xcodeproj" \
		-scheme "$$test_scheme" \
		-configuration Debug \
		-destination '$(ORLIX_TEST_DESTINATION)' \
		-derivedDataPath "$(ORLIX_BUILD_ROOT)/Bazel/DerivedData/OutputParser" \
		-resultBundlePath "$$result_dir/tests.xcresult" \
		-parallel-testing-enabled NO \
		"$${test_filters[@]}" \
		test; \
	DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" /usr/bin/xcrun xcresulttool get test-results summary --path "$$result_dir/tests.xcresult" > "$$result_dir/summary.json"; \
	python3 -c 'import json,sys; p=json.load(open(sys.argv[1])); assert p["result"] == "Passed" and p["passedTests"] > 0 and p["totalTestCount"] == p["passedTests"] and p["failedTests"] == p["skippedTests"] == p["expectedFailures"] == 0, p' "$$result_dir/summary.json"

__bazel-orlix-app: __bazel-feasibility-bootstrap $(if $(filter promoted,$(ORLIX_BAZEL_COMPONENT_MODE)),__bazel-substitute-promoted)
	@DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" build //Orlix:Orlix --compilation_mode=$(ORLIX_BAZEL_COMPILATION_MODE) --config=$(PROFILE) --config=$(ORLIX_BAZEL_COMPONENT_MODE) --apple_platform_type=ios --ios_multi_cpus=$(ORLIX_BAZEL_IOS_CPU) --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_TCTI_ISA_PREPARED="$(ORLIX_TCTI_ISA_PREPARED)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)"
	@set -euo pipefail; \
	ipa_rel="$$(DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" cquery //Orlix:Orlix --compilation_mode=$(ORLIX_BAZEL_COMPILATION_MODE) --config=$(PROFILE) --config=$(ORLIX_BAZEL_COMPONENT_MODE) --apple_platform_type=ios --ios_multi_cpus=$(ORLIX_BAZEL_IOS_CPU) --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_TCTI_ISA_PREPARED="$(ORLIX_TCTI_ISA_PREPARED)" --output=files | awk '/Orlix[.]ipa$$/ {path=$$0; count++} END {if (count != 1) exit 1; print path}')"; \
	ipa="$(ORLIX_BAZEL_OUTPUT_BASE)/execroot/_main/$$ipa_rel"; \
	test -n "$$ipa" && test -s "$$ipa" || { echo "missing //Orlix:Orlix ipa" >&2; exit 1; }; \
	/usr/bin/unzip -l "$$ipa" | /usr/bin/grep -F 'Payload/Orlix.app/Info.plist'; \
	/usr/bin/unzip -p "$$ipa" Payload/Orlix.app/Info.plist | python3 -c 'import plistlib,sys; info=plistlib.loads(sys.stdin.buffer.read()); assert info["MinimumOSVersion"] == "15.0"; assert info["CFBundleSupportedPlatforms"] == [sys.argv[1]]' "$(if $(filter iphoneos,$(ORLIX_BAZEL_DESTINATION)),iPhoneOS,iPhoneSimulator)"; \
	ipa_work="$$(mktemp -d "$${TMPDIR:-/tmp}/orlix-ipa.XXXXXX")"; \
	/usr/bin/unzip -q "$$ipa" -d "$$ipa_work"; \
	test -x "$$ipa_work/Payload/Orlix.app/Orlix"; \
	python3 -c 'import plistlib,sys; from pathlib import Path; app=Path(sys.argv[1]); assert all(info.get("CFBundleName") and info.get("CFBundlePackageType") == kind and info.get("CFBundleInfoDictionaryVersion") == "6.0" for bundle,kind in [(app,"APPL"),*((p,"XPC!") for p in app.glob("PlugIns/*.appex"))] for info in [plistlib.loads((bundle/"Info.plist").read_bytes())]), "app or extension is missing required bundle metadata"' "$$ipa_work/Payload/Orlix.app"; \
	if [ "$(ORLIX_BAZEL_DESTINATION)" = iphoneos ]; then \
	/usr/bin/codesign --verify --deep --strict "$$ipa_work/Payload/Orlix.app"; \
	python3 -c 'import plistlib,subprocess,sys; from pathlib import Path; app=Path(sys.argv[1]); assert all(ent["application-identifier"] == sys.argv[2]+"."+plistlib.loads((bundle/"Info.plist").read_bytes())["CFBundleIdentifier"] and ent["com.apple.developer.team-identifier"] == sys.argv[2] for bundle in [app,*app.glob("PlugIns/*.appex")] for ent in [plistlib.loads(subprocess.run(["codesign","-d","--entitlements",":-",str(bundle)],check=True,capture_output=True).stdout)]), "app or extension signing identity differs from its bundle or team"' "$$ipa_work/Payload/Orlix.app" "$(ORLIX_DEVELOPMENT_TEAM)"; \
	fi; \
	test -s "$$ipa_work/Payload/Orlix.app/mlx-swift_Cmlx.bundle/default.metallib" || { echo "missing compiled MLX shader library" >&2; exit 1; }; \
	os_binary="$$ipa_work/Payload/Orlix.app/Frameworks/OrlixOS.framework/OrlixOS"; \
	test -x "$$os_binary" || { echo "missing embedded OrlixOS.framework" >&2; exit 1; }; \
	python3 -c 'import plistlib,sys; from pathlib import Path; root=Path(sys.argv[1]); info=plistlib.loads((root/"Info.plist").read_bytes()); assert info["OrlixSelectedProfile"] == sys.argv[2]; assert len(info["OrlixOSRootImages"]) == 4; paths=[info[key] for key in ("OrlixRootInitramfs", "OrlixBaseRootImage", "OrlixStateRootImage")]+["arch/orlix/boot/dts/release.dtb", "arch/orlix/boot/dts/development.dtb", "composition.json"]; missing=[name for name in paths if not (root/name).is_file() or (root/name).stat().st_size == 0]; assert not missing, missing' "$${os_binary%/*}" "$(PROFILE)"; \
	/usr/bin/nm -gU "$$os_binary" | /usr/bin/grep -E '[[:space:]]T[[:space:]]+_OrlixBoot' >/dev/null || { echo "missing defined _OrlixBoot in OrlixOS.framework" >&2; /usr/bin/nm -gU "$$os_binary" | /usr/bin/grep OrlixBoot >&2 || true; rm -rf "$$ipa_work"; exit 1; }; \
	if /usr/bin/nm "$$os_binary" | /usr/bin/grep -E '[[:space:]]U[[:space:]]+_OrlixBoot' >/dev/null; then echo "_OrlixBoot must not remain undefined" >&2; rm -rf "$$ipa_work"; exit 1; fi; \
	/usr/bin/nm -gU "$$os_binary" | /usr/bin/grep -E '[[:space:]]T[[:space:]]+_arch_boot_entry' >/dev/null || { echo "missing defined _arch_boot_entry in OrlixOS.framework" >&2; /usr/bin/nm "$$os_binary" | /usr/bin/grep arch_boot_entry >&2 || true; rm -rf "$$ipa_work"; exit 1; }; \
	if /usr/bin/nm "$$os_binary" | /usr/bin/grep -E '[[:space:]]U[[:space:]]+_arch_boot_entry' >/dev/null; then echo "_arch_boot_entry must not remain undefined" >&2; rm -rf "$$ipa_work"; exit 1; fi; \
	if [ "$(ORLIX_BAZEL_COMPONENT_MODE)" = promoted ]; then \
	lock_buildset="$$(python3 -c 'import json; print(json.load(open("$(CURDIR)/artifacts.lock.json"))["buildset"])')"; \
	test "$${#lock_buildset}" -eq 64 || { echo "artifacts.lock.json missing buildset" >&2; rm -rf "$$ipa_work"; exit 1; }; \
	stamp="$$(/usr/bin/find "$$ipa_work/Payload/Orlix.app" -name 'locked-buildset.json' -print | /usr/bin/head -n 1)"; \
	test -s "$$stamp" || { echo "promoted //Orlix:Orlix must embed locked-buildset.json" >&2; rm -rf "$$ipa_work"; exit 1; }; \
	rg -F -q "$$lock_buildset" "$$stamp" || { echo "IPA lock stamp does not match artifacts.lock.json" >&2; rm -rf "$$ipa_work"; exit 1; }; \
	if rg -q ':latest' "$$stamp"; then echo "locked-buildset.json must not use mutable latest" >&2; rm -rf "$$ipa_work"; exit 1; fi; \
	rg -F -q "$$lock_buildset" "$${os_binary%/*}/composition.json" || { echo "promoted kernel composition must record the locked buildset" >&2; rm -rf "$$ipa_work"; exit 1; }; \
	initramfs="$$(/usr/bin/find "$$ipa_work/Payload/Orlix.app" -name 'initramfs.cpio.gz' -print | /usr/bin/head -n 1)"; \
	test -s "$$initramfs" || { echo "promoted IPA missing reconstructed rootfs initramfs" >&2; rm -rf "$$ipa_work"; exit 1; }; \
	imported_initramfs="$(CURDIR)/bazel/promotion/imported/rootfs/initramfs.cpio.gz"; \
	test -s "$$imported_initramfs" || { echo "missing staged reconstructed initramfs" >&2; rm -rf "$$ipa_work"; exit 1; }; \
	test "$$(/usr/bin/shasum -a 256 "$$initramfs" | /usr/bin/awk '{print $$1}')" = "$$(/usr/bin/shasum -a 256 "$$imported_initramfs" | /usr/bin/awk '{print $$1}')" || { echo "IPA initramfs does not match reconstructed OCI tree" >&2; rm -rf "$$ipa_work"; exit 1; }; \
	fi; \
	PYTHONPATH="$(CURDIR)/make" python3 -c "from pathlib import Path; import ios15_simulator_gate as gate; gate.validate_simulator_app(Path('$$ipa_work/Payload/Orlix.app'))"; \
	mkdir -p "$(ORLIX_BUILD_ROOT)/Bazel/proof"; \
	/usr/bin/nm -gU "$$os_binary" | /usr/bin/grep -E '[[:space:]]T[[:space:]]+_OrlixBoot|[[:space:]]T[[:space:]]+_arch_boot_entry' > "$(ORLIX_BUILD_ROOT)/Bazel/proof/kernel-link.log"; \
	test -s "$(ORLIX_BUILD_ROOT)/Bazel/proof/kernel-link.log" || { echo "missing kernel link evidence from IPA" >&2; rm -rf "$$ipa_work"; exit 1; }; \
	rm -rf "$$ipa_work"

__bazel-orlix-archive: __bazel-feasibility-bootstrap __bazel-substitute-promoted
	@test -n "$(ORLIX_DEVELOPMENT_TEAM)" || { echo "ORLIX_DEVELOPMENT_TEAM is required to archive for TestFlight" >&2; exit 1; }
	@test -d "$(ORLIX_PINNED_DEVELOPER_DIR)" || { echo "missing pinned Xcode developer directory: $(ORLIX_PINNED_DEVELOPER_DIR)" >&2; exit 1; }
	@DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" build //Orlix:Orlix.xcarchive --apple_generate_dsym --compilation_mode=opt --config=release --config=promoted --apple_platform_type=ios --ios_multi_cpus=arm64 --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_TCTI_ISA_PREPARED="$(ORLIX_TCTI_ISA_PREPARED)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)"
	@set -euo pipefail; \
	archive_rel="$$(DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" cquery //Orlix:Orlix.xcarchive --apple_generate_dsym --compilation_mode=opt --config=release --config=promoted --apple_platform_type=ios --ios_multi_cpus=arm64 --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_TCTI_ISA_PREPARED="$(ORLIX_TCTI_ISA_PREPARED)" --output=files | awk '/Orlix[.]xcarchive$$/ {path=$$0; count++} END {if (count != 1) exit 1; print path}')"; \
	archive="$(ORLIX_BAZEL_OUTPUT_BASE)/execroot/_main/$$archive_rel"; \
	test -s "$$archive/Info.plist" || { echo "missing Bazel xcarchive metadata" >&2; exit 1; }; \
	test -d "$$archive/dSYMs/Orlix.app.dSYM" || { echo "missing Orlix archive debug symbols" >&2; exit 1; }; \
	python3 -c 'import plistlib,sys; from pathlib import Path; p=plistlib.loads((Path(sys.argv[1])/"Info.plist").read_bytes())["ApplicationProperties"]; assert p["CFBundleIdentifier"] == "com.rudironsoni.Orlix"; assert p.get("Team") == sys.argv[2], "archive signing team mismatch"; assert p.get("SigningIdentity"), "archive has no signing identity"' "$$archive" "$(ORLIX_DEVELOPMENT_TEAM)"; \
	/usr/bin/codesign --verify --deep --strict "$$archive/Products/Applications/Orlix.app"; \
	mkdir -p "$(ORLIX_BETA_ARCHIVE_DIR)"; \
	if [ -e "$(ORLIX_BETA_ARCHIVE_PATH)" ]; then previous="$$(mktemp -d "$(ORLIX_BETA_ARCHIVE_DIR)/previous-archive.XXXXXX")"; mv "$(ORLIX_BETA_ARCHIVE_PATH)" "$$previous/Orlix.xcarchive"; fi; \
	/usr/bin/ditto "$$archive" "$(ORLIX_BETA_ARCHIVE_PATH)"

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
	type_id="$$(xcrun simctl list devices -j | jq -r --arg id "$(ORLIX_IOS15_SIMULATOR_ID)" '.devices | to_entries[] | select(.key | contains("iOS-15-5")) | .value[] | select(.udid == $$id) | .deviceTypeIdentifier')"; \
	test -n "$$type_id" || { echo "missing iOS 15.5 simulator deviceTypeIdentifier for $(ORLIX_IOS15_SIMULATOR_ID)" >&2; exit 1; }; \
	device_name="$$(xcrun simctl list devicetypes -j | jq -r --arg id "$$type_id" '.devicetypes[] | select(.identifier == $$id) | .name')"; \
	test -n "$$device_name" || { echo "missing iOS 15.5 simulator device type name for $$type_id" >&2; exit 1; }; \
	xcrun simctl bootstatus "$(ORLIX_IOS15_SIMULATOR_ID)" -b; \
	PYTHONPATH="$(CURDIR)/make" python3 -m unittest test_ios15_simulator_gate; \
	result_dir="$(ORLIX_BUILD_ROOT)/iOS15"; \
	result_log="$$result_dir/Orlix-iOS15.log"; \
	mkdir -p "$$result_dir"; \
	DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" test //Orlix:OrlixUITests --compilation_mode=dbg --config=release --config=promoted --apple_platform_type=ios --ios_multi_cpus=sim_arm64 --ios_simulator_version=15.5 --ios_simulator_device="$$device_name" --test_filter=AppLaunchSmokeUITests/testLaunchCapturesScreenshot --test_output=errors --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_TCTI_ISA_PREPARED="$(ORLIX_TCTI_ISA_PREPARED)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)" 2>&1 | tee "$$result_log"; \
	DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" build //Orlix:Orlix --compilation_mode=dbg --config=release --config=promoted --apple_platform_type=ios --ios_multi_cpus=sim_arm64 --ios_simulator_version=15.5 --ios_simulator_device="$$device_name" --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_TCTI_ISA_PREPARED="$(ORLIX_TCTI_ISA_PREPARED)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)" 2>&1 | tee -a "$$result_log"; \
	ipa="$$(/usr/bin/find "$(ORLIX_BAZEL_OUTPUT_BASE)/execroot/_main/bazel-out" -path '*/bin/Orlix/Orlix.ipa' ! -path '*/runfiles/*' -print | /usr/bin/head -n 1)"; \
	test -n "$$ipa" && test -s "$$ipa" || { echo "missing //Orlix:Orlix ipa after iOS 15 UI tests" >&2; exit 1; }; \
	ipa_work="$$(mktemp -d "$${TMPDIR:-/tmp}/orlix-ios15.XXXXXX")"; \
	/usr/bin/unzip -q "$$ipa" -d "$$ipa_work"; \
	PYTHONPATH="$(CURDIR)/make" python3 -c "from pathlib import Path; import ios15_simulator_gate as gate; gate.validate_simulator_app(Path('$$ipa_work/Payload/Orlix.app')); print('pass: iOS 15 app does not required-load AppIntents or ActivityKit')"; \
	rm -rf "$$ipa_work"

__bazel-hostadapter: __bazel-feasibility-bootstrap
	@DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" build //OrlixHostAdapter/Sources:OrlixHostAdapter --compilation_mode=dbg --config=release --config=source --apple_platform_type=ios --ios_multi_cpus=sim_arm64 --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)"

__bazel-product-composition: __bazel-kernel-uapi __bazel-hostadapter
	@DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" build //bazel/product:kernel_composition --config=release --config=source --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_TCTI_ISA_PREPARED="$(ORLIX_TCTI_ISA_PREPARED)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)"
	@test -s bazel-bin/bazel/product/kernel_composition/composition.json
	@rg -q '"linux_archive"' bazel-bin/bazel/product/kernel_composition/composition.json
	@rg -q '"hostadapter"' bazel-bin/bazel/product/kernel_composition/composition.json
	@rg -q '"boot"' bazel-bin/bazel/product/kernel_composition/composition.json
	@rg -q '"linked_symbol": "_arch_boot_entry"' bazel-bin/bazel/product/kernel_composition/composition.json
	@rg -F -q '"undefined_kernel_symbols": []' bazel-bin/bazel/product/kernel_composition/composition.json
	@rg -q '"xcframework": null' bazel-bin/bazel/product/kernel_composition/composition.json
	@if rg -q 'Makefile' bazel-bin/bazel/product/kernel_composition/composition.json; then echo "composition must not invoke wrapper Makefiles" >&2; exit 1; fi

__bazel-cache-equivalence: __bazel-feasibility-bootstrap
	@set -euo pipefail; \
	command -v jq >/dev/null || { echo "jq is required to inspect Bazel cache execution logs" >&2; exit 1; }; \
	proof="$$(/usr/bin/mktemp -d "$(ORLIX_BUILD_ROOT)/Bazel/cache-equivalence.XXXXXX")"; \
	flags=(--config=release --config=source --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_PINNED_DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=ORLIX_KBUILD_PERSIST= --action_env=ORLIX_COMPILER_LAUNCHER= --action_env=CCACHE_DISABLE=1 --remote_cache= --remote_executor= --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)" --symlink_prefix=/); \
	for side in seed cached uncached; do \
		mkdir -p "$$proof/$$side"; \
		cache_flags=(--disk_cache="$$proof/disk"); \
		if [ "$$side" = uncached ]; then cache_flags=(--nouse_action_cache --disk_cache=); fi; \
		echo "cache-equivalence: $$side build, evidence $$proof/$$side"; \
		DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --batch --output_base="$$proof/$$side/output-base" build //bazel/feasibility/kernel:uapi //bazel/feasibility/mlibc:sysroot //bazel/feasibility/rootfs:rootfs "$${flags[@]}" "$${cache_flags[@]}" --execution_log_json_file="$$proof/$$side/execution.json" --build_event_json_file="$$proof/$$side/build-events.json" --profile="$$proof/$$side/profile.json.gz"; \
		if [ "$$side" = seed ]; then continue; fi; \
		jq -e -s --arg side "$$side" 'map(select(.mnemonic == "OrlixLinuxHeadersInstall" or .mnemonic == "OrlixMLibCSysroot" or .mnemonic == "OrlixRootfs")) | if (map(.mnemonic) | sort) == ["OrlixLinuxHeadersInstall", "OrlixMLibCSysroot", "OrlixRootfs"] and all(.[]; (.exitCode // 0) == 0 and (.status // "") == "" and (if $$side == "cached" then .cacheHit == true and .runner == "disk cache hit" else (.cacheHit // false) == false and .runner != "remote" and (.runner | length) > 0 end)) then map({mnemonic, runner, cacheHit, exitCode}) else error("required component cache behavior was not observed") end' "$$proof/$$side/execution.json" > "$$proof/$$side/cache-observation.json"; \
		DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --batch --output_base="$$proof/$$side/output-base" cquery 'set(//bazel/feasibility/kernel:uapi //bazel/feasibility/mlibc:sysroot //bazel/feasibility/rootfs:rootfs)' "$${flags[@]}" "$${cache_flags[@]}" --output=files > "$$proof/$$side/outputs.txt"; \
	done; \
	PYTHONPATH="$(CURDIR)/bazel/promotion" python3 -c 'import compare,json,sys; from pathlib import Path; root=Path(sys.argv[1]); outputs={side:(root/side/"outputs.txt").read_text().splitlines() for side in ("cached","uncached")}; markers={"uapi":"/uapi.sha256","mlibc":"/sysroot.sha256","rootfs":"/source-input.sha256"}; trees={side:{name:[root/side/"output-base/execroot/_main"/p for p in paths if p.endswith(marker)] for name,marker in markers.items()} for side,paths in outputs.items()}; assert all(len(matches)==1 for components in trees.values() for matches in components.values()), trees; digests={name:compare.compare_trees(str(trees["cached"][name][0].parent),str(trees["uncached"][name][0].parent)) for name in markers}; (root/"comparison.json").write_text(json.dumps({"schema":1,"component_tree_digests":digests,"cache_observations":[str(root/side/"cache-observation.json") for side in outputs]},indent=2)+"\n"); print(json.dumps(digests,sort_keys=True))' "$$proof"
