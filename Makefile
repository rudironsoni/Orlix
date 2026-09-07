override ORLIX_GNU_MAKE_CONTRACT_ENTRYPOINT := Makefile
include $(CURDIR)/make/gnu-make-contract.mk

ifeq ($(ORLIX_GNU_MAKE_CONTRACT_READY),1)

SHELL := /bin/bash
.DEFAULT_GOAL := all

KERNEL_MAKE := $(MAKE) -f OrlixKernel/Makefile
HOSTADAPTER_MAKE := $(MAKE) -f OrlixHostAdapter/Makefile
MLIBC_MAKE := $(MAKE) -f OrlixMLibC/Makefile
COREUTILS_MAKE := $(MAKE) -f OrlixCoreUtils/Makefile
ORLIXOS_MAKE := $(MAKE) -f OrlixOS/Makefile
APP_MAKE := $(MAKE) -f Orlix/Makefile
PROFILE ?= release
type ?= product
vendor ?= all
-include $(CURDIR)/.orlix.local.xcconfig
ORLIX_BUILD_ROOT ?= $(CURDIR)/Build
export ORLIX_BUILD_ROOT
include $(CURDIR)/make/bazel-migration.mk
ORLIXOS_XCFRAMEWORK_ROOT ?= $(ORLIX_BUILD_ROOT)/OrlixOS/xcframework
ORLIXOS_DEVICE_ARCHIVE ?= $(ORLIXOS_XCFRAMEWORK_ROOT)/iphoneos.xcarchive
ORLIXOS_SIMULATOR_ARCHIVE ?= $(ORLIXOS_XCFRAMEWORK_ROOT)/iphonesimulator.xcarchive
ORLIXOS_XCFRAMEWORK ?= $(ORLIXOS_XCFRAMEWORK_ROOT)/OrlixOS.xcframework
ORLIXOS_BASE_ROOT_TREE := $(ORLIX_BUILD_ROOT)/OrlixOS/rootfs/$(PROFILE)/base-tree
ORLIX_BETA_SCHEME ?= Orlix
ORLIX_BETA_ARCHIVE_DIR ?= $(ORLIX_BUILD_ROOT)/Release
ORLIX_BETA_ARCHIVE_PATH ?= $(ORLIX_BETA_ARCHIVE_DIR)/Orlix.xcarchive
ORLIX_BETA_EXPORT_DIR ?= $(ORLIX_BETA_ARCHIVE_DIR)/Export
ORLIX_BETA_EXPORT_OPTIONS_PLIST ?=
ORLIX_DEVELOPMENT_TEAM ?= ZQ3L7M567L
ORLIX_CODE_SIGN_STYLE ?= Automatic
ORLIX_CODE_SIGN_IDENTITY ?=
ORLIX_PROVISIONING_PROFILE_SPECIFIER ?=
ORLIX_ALLOW_PROVISIONING_UPDATES ?= YES
ORLIX_BETA_BUMP_BUILD_NUMBER ?= YES
ORLIX_BETA_BUILD_NUMBER ?=
ORLIX_BETA_BUILD_NUMBER_FILE ?= $(ORLIX_BETA_ARCHIVE_DIR)/build-number
ORLIX_ASC_API_KEY_PATH ?=
ORLIX_ASC_API_KEY_ID ?=
ORLIX_ASC_API_ISSUER_ID ?=
ORLIX_FASTLANE_API_KEY_PATH ?= $(HOME)/.config/fastlane/appstore_api_key.json
ORLIX_FASTLANE ?= bundle exec fastlane
ORLIX_BETA_IPA_PATH ?= $(ORLIX_BETA_EXPORT_DIR)/Orlix.ipa
ORLIX_OPENPANEL_CLIENT_ID ?=
ORLIX_SIGNOZ_INGESTION_KEY ?=
ORLIX_ANALYTICS_ENABLED ?= NO
ORLIX_OBSERVABILITY_ENABLED ?= NO
ORLIX_XCODEBUILD_ARCHIVE ?= xcodebuild
ORLIX_XCODEBUILD_EXPORT ?= xcodebuild
ORLIX_BETA_SIMULATOR_ID ?= ADE0D3EB-6E89-41DD-9AB9-CA20F10609F3
ORLIX_BETA_SIMULATOR_DESTINATION ?= platform=iOS Simulator,id=$(ORLIX_BETA_SIMULATOR_ID)
ORLIX_TCTI_TEST_DESTINATION ?= $(ORLIX_BETA_SIMULATOR_DESTINATION)
ORLIX_TEST_DESTINATION ?= $(ORLIX_BETA_SIMULATOR_DESTINATION)
ORLIX_KUNIT_PRODUCT_BUILD_ROOT ?= $(ORLIX_BUILD_ROOT)/KUnitTest
ORLIX_TCTI_DERIVED_DATA_PATH ?= $(ORLIX_KUNIT_PRODUCT_BUILD_ROOT)/DerivedData
ORLIX_TCTI_XCTEST_TIMEOUT_SECONDS ?= 330
ORLIX_TCTI_XCODEBUILD_WALL_TIMEOUT_SECONDS ?= 1800
ORLIX_TCTI_BUILD_FOR_TESTING_WALL_TIMEOUT_SECONDS ?= $(ORLIX_TCTI_XCODEBUILD_WALL_TIMEOUT_SECONDS)
ORLIX_TCTI_TEST_WITHOUT_BUILDING_WALL_TIMEOUT_SECONDS ?= $(ORLIX_TCTI_XCODEBUILD_WALL_TIMEOUT_SECONDS)
ORLIX_TCTI_TEST_ONLY_TESTING ?= OrlixKernelConformanceTests/OrlixKernelConformanceTests/testKselftestRootfsCompletesThroughOrlixOSTerminalSession
ORLIX_TCTI_XCODEBUILD ?= /usr/bin/xcodebuild
# ADR 0037 cutover. Make stays the public interface. Bazel owns product compile.
# Keep this default above every ifeq that reads it.
ORLIX_BAZEL_AUTHORITY ?= 1

define ORLIX_TCTI_XCODEBUILD_WATCHDOG_FUNCTIONS
run_xcodebuild() { \
	local timeout_marker="$$1" timeout_state marker_parent marker_ready child watchdog status wall_timeout process_group controller_group; \
	shift; \
	wall_timeout="$${ORLIX_TCTI_XCODEBUILD_WALL_TIMEOUT_SECONDS:-$(ORLIX_TCTI_XCODEBUILD_WALL_TIMEOUT_SECONDS)}"; \
	timeout_state="$$(mktemp "$${TMPDIR:-/tmp}/orlix-tcti-xcodebuild-timeout.XXXXXX")" || return 1; \
	marker_parent="$$(dirname "$$timeout_marker")"; marker_ready=1; \
	if ! mkdir -p "$$marker_parent" || ! rm -f "$$timeout_marker"; then marker_ready=0; echo "ORLIX_TCTI_XCODEBUILD_WATCHDOG_MARKER_UNAVAILABLE path=$$timeout_marker" >&2; fi; \
	set -m; \
	"$$@" & child=$$!; \
	process_group=$$(/bin/ps -o pgid= -p "$$child"); process_group=$${process_group//[[:space:]]/}; \
	controller_group=$$(/bin/ps -o pgid= -p "$$$$"); controller_group=$${controller_group//[[:space:]]/}; \
	if [[ ! "$$process_group" =~ ^[0-9]+$$ ]]; then echo "ORLIX_TCTI_XCODEBUILD_WATCHDOG_INVALID_GROUP pid=$$child pgid=$$process_group" >&2; kill -TERM "$$child" 2>/dev/null || true; wait "$$child" 2>/dev/null || true; rm -f "$$timeout_state"; return 1; fi; \
	if [ "$$process_group" = "$$controller_group" ]; then echo "ORLIX_TCTI_XCODEBUILD_WATCHDOG_CONTROLLER_GROUP pid=$$child pgid=$$process_group" >&2; kill -TERM "$$child" 2>/dev/null || true; wait "$$child" 2>/dev/null || true; rm -f "$$timeout_state"; return 1; fi; \
	( sleep "$$wall_timeout"; \
		if kill -0 -- "-$$process_group" 2>/dev/null; then \
			echo "ORLIX_TCTI_XCODEBUILD_TIMEOUT seconds=$$wall_timeout pid=$$child pgid=$$process_group" >&2; \
			printf '%s\n' 124 > "$$timeout_state"; \
			if [ "$$marker_ready" -eq 1 ]; then : > "$$timeout_marker" || echo "ORLIX_TCTI_XCODEBUILD_WATCHDOG_MARKER_WRITE_FAILED path=$$timeout_marker" >&2; fi; \
			kill -TERM -- "-$$process_group" 2>/dev/null || true; \
			sleep 1; \
			if kill -0 -- "-$$process_group" 2>/dev/null; then kill -KILL -- "-$$process_group" 2>/dev/null || true; fi; \
		fi \
	) & watchdog=$$!; \
	if wait "$$child"; then status=0; else status=$$?; fi; \
	if [ -s "$$timeout_state" ]; then \
		wait "$$watchdog" 2>/dev/null || true; rm -f "$$timeout_state"; \
		echo "ORLIX_TCTI_XCODEBUILD_TIMEOUT_RESULT status=124" >&2; \
		return 124; \
	fi; \
	kill "$$watchdog" 2>/dev/null || true; \
	wait "$$watchdog" 2>/dev/null || true; \
	rm -f "$$timeout_state"; \
	return "$$status"; \
};
endef
ORLIX_TCTI_INVENTORY_AUDITOR := $(ORLIX_BUILD_ROOT)/AgentHarness/orlix-tcti/audit_inventory
ORLIX_TCTI_INVENTORY_CONTRACT_TEST := $(ORLIX_BUILD_ROOT)/Tests/orlix-tcti-isa/inventory_contract_test
ORLIX_TCTI_HOST_LANE_BOUNDARY_TEST := $(ORLIX_BUILD_ROOT)/Tests/orlix-tcti-isa/host_lane_boundary_test
ORLIX_TCTI_TARGET_PROOF_REGISTRY_TEST := $(ORLIX_BUILD_ROOT)/Tests/orlix-tcti-isa/target_proof_registry_test
ORLIX_TCTI_TARGET_PROOF_INGESTION_TEST := $(ORLIX_BUILD_ROOT)/Tests/orlix-tcti-isa/target_proof_ingestion_test
include $(CURDIR)/OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/isa/build-time/instruction-artifact-contributors.mk
include $(CURDIR)/OrlixKernel/Sources/ports/orlix/kbuild/proof-provenance.mk
ORLIX_TCTI_RUNTIME_PROJECTION_TEST := $(ORLIX_BUILD_ROOT)/Tests/orlix-tcti-isa/target_runtime_projection_test
ORLIX_TCTI_SYSTEM_ACCESS_SELECTOR_TEST := $(ORLIX_BUILD_ROOT)/Tests/orlix-tcti-isa/target_system_access_selector_test
ORLIX_TCTI_SCALAR_OPERATION_CATALOG_TEST := $(ORLIX_BUILD_ROOT)/Tests/orlix-tcti-isa/target_scalar_operation_catalog_test
ORLIX_TCTI_LSE_OPERATION_CATALOG_TEST := $(ORLIX_BUILD_ROOT)/Tests/orlix-tcti-isa/target_lse_operation_catalog_test
ORLIX_TCTI_PROOF_CANDIDATE_TEST := $(ORLIX_BUILD_ROOT)/Tests/orlix-tcti-isa/target_proof_candidate_test
ORLIX_TCTI_INSTRUCTION_ARTIFACT_ROUNDTRIP_TEST := $(ORLIX_BUILD_ROOT)/Tests/orlix-tcti-isa/target_instruction_artifact_roundtrip_test
ORLIX_TCTI_INSTRUCTION_ARTIFACT_MUTATION_TEST := $(ORLIX_BUILD_ROOT)/Tests/orlix-tcti-isa/target_instruction_artifact_generated_mutation_test
ORLIX_TCTI_FEATURE_ARTIFACT_VALIDATOR_TEST := $(ORLIX_BUILD_ROOT)/Tests/orlix-tcti-isa/target_feature_artifact_generated_test
ORLIX_TCTI_FEATURE_APPLICABILITY_ARTIFACT_TEST := $(ORLIX_BUILD_ROOT)/Tests/orlix-tcti-isa/target_feature_applicability_artifact_test
ORLIX_TCTI_FEATURE_DOMAIN_TEST := $(ORLIX_BUILD_ROOT)/Tests/orlix-tcti-isa/target_feature_domain_test
ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_TEST := $(ORLIX_BUILD_ROOT)/Tests/orlix-tcti-isa/target_feature_field_domain_binding_artifact_test
ORLIX_TCTI_RUNTIME_CAPABILITY_COHORT_TEST := $(ORLIX_BUILD_ROOT)/Tests/orlix-tcti-isa/target_runtime_capability_cohort_artifact_test
ORLIX_TCTI_EXECUTION_SLICE_MAP_TEST := $(ORLIX_BUILD_ROOT)/Tests/orlix-tcti-isa/target_execution_slice_map_test
ORLIX_TCTI_TARGET_KBUILD_GENERATOR_TEST := $(ORLIX_BUILD_ROOT)/Tests/orlix-tcti-isa/target_isa_kbuild_generator_test
ORLIX_TCTI_ORDINAL_LEDGER_TEST := $(ORLIX_BUILD_ROOT)/Tests/orlix-tcti-isa/target_ordinal_ledger_test
ORLIX_TCTI_SEMANTIC_PROVENANCE_TEST := $(ORLIX_BUILD_ROOT)/Tests/orlix-tcti-isa/target_completion_semantic_provenance_test
ORLIX_APP_BUNDLE_ID ?= com.rudironsoni.orlix
include $(CURDIR)/make/release.mk
include $(CURDIR)/make/runtime.mk
include $(CURDIR)/make/tcti-proof-registry-provenance.mk
.PHONY: all help setup-env check-build-tools product-build-prepare product-build-version-check app-capability-gate app-capability-test app-release-inputs-check app-release-inputs-test app-exported-product-check console-policy-tests terminal-mux-tests orlix-tcti-semantic-provenance-tests orlix-tcti-isa-host-tests orlix-tcti-operational-note-pipeline-test orlix-tcti-isa-maintainer-source-check orlix-tcti-native-proof-symbol-check orlix-tcti-isa-audit orlix-tcti-kernel-tests mlibc-tests coreutils-tests hostadapter-tests orlixos-tests app-tests runtime-tests ios15-simulator-gate beta-prerequisites beta-signing-diagnostics beta-bump-build-number beta-resolve-build-number beta-install-simulator beta-simulator-gate docs-index docs-check agent-rules-generate agent-rules-check agent-hooks-generate agent-hooks-check agent-skills-check agent-subagents-check agent-mcp-check agent-status agent-next agent-task-envelope-check beta-archive beta-validate-archive beta-export-options beta-export-archive beta-validate-export beta-upload-prerequisites beta-upload beta-distribute beta-release-report app-store-release-report-check app-store-promote release-workflow-check build rebuild prepare scripts dtbs headers_install kunit kselftest kselftest-install test xcodeproj run clean mrproper __build-product __build-vendor __prepare-product __prepare-tcti-isa

.PHONY: orlixos-xcframework orlix-tcti-xcodebuild-watchdog-tests orlix-tcti-proof-source-linkage-tests
.PHONY: vvterm-sync vvterm-sync-resolve vvterm-reconcile vvterm-sync-complete vvterm-source-check vvterm-sync-tests vvterm-upstream-tests
.PHONY: orlix-tcti-native-proof-symbol-check-dependency-regression
.PHONY: orlix-tcti-proof-registry-provenance-regression
.PHONY: orlix-tcti-isa-host-provenance-dependency-regression
.PHONY: __orlix-tcti-proof-registry-provenance-write
.PHONY: __orlix-tcti-isa-host-provenance-ready
.PHONY: __orlix-root-gnu-make-contract-source-check

all: build

help:
	@$(KERNEL_MAKE) help
	@printf '%s\n' ''
	@printf '%s\n' 'Project Makefiles:'
	@printf '%s\n' '  OrlixKernel/Makefile'
	@printf '%s\n' '  OrlixHostAdapter/Makefile'
	@printf '%s\n' '  OrlixMLibC/Makefile'
	@printf '%s\n' '  OrlixCoreUtils/Makefile'
	@printf '%s\n' '  OrlixOS/Makefile'
	@printf '%s\n' '  Orlix/Makefile'
	@printf '%s\n' ''
	@printf '%s\n' 'Owning test suites:'
	@printf '%s\n' '  orlix-tcti-isa-host-tests    run deterministic OrlixTCTI inventory contract tests'
	@printf '%s\n' '  orlix-tcti-isa-audit         audit the canonical C target inventory and proof ledger'
	@printf '%s\n' '  prepare type=tcti-isa         atomically refresh the pinned C artifact bundle'
	@printf '%s\n' '  orlix-tcti-kernel-tests run OrlixTCTI KUnit and app-hosted Linux kselftests'
	@printf '%s\n' '  mlibc-tests         run the upstream mlibc suite through OrlixOS'
	@printf '%s\n' '  coreutils-tests     run the OrlixCoreUtils upstream suite through OrlixOS'
	@printf '%s\n' '  hostadapter-tests   run private Darwin transport and memory tests'
	@printf '%s\n' '  orlixos-tests       run OrlixOS unit tests'
	@printf '%s\n' '  orlixos-xcframework build the sole public OrlixOS.xcframework SDK'
	@printf '%s\n' '  app-tests           run native Orlix app and UI tests'
	@printf '%s\n' '  runtime-tests       run app-hosted OrlixOS runtime integration tests'
	@printf '%s\n' ''
	@printf '%s\n' 'Beta targets:'
	@printf '%s\n' '  beta-prerequisites  verify local TestFlight build prerequisites'
	@printf '%s\n' '  beta-signing-diagnostics check local Apple signing/keychain state'
	@printf '%s\n' '  beta-install-simulator build, fresh-install, and launch Release Orlix'
	@printf '%s\n' '  beta-simulator-gate run focused simulator gate for first beta'
	@printf '%s\n' '  beta-archive        generate Xcode project and archive Orlix Release'
	@printf '%s\n' '  beta-validate-archive inspect required app/framework/payload archive contents'
	@printf '%s\n' '  beta-export-archive export archived Orlix for upload'
	@printf '%s\n' '  beta-upload         upload the exact exported IPA to TestFlight'
	@printf '%s\n' '  beta-distribute     assign the processed beta to one internal group'
	@printf '%s\n' '  app-store-promote   submit the exact approved beta build for App Review'
	@printf '%s\n' '  release-workflow-check validate release policy, tests, and workflow YAML'

__orlix-root-gnu-make-contract-source-check:
	@set -euo pipefail; \
	tmp="$$(mktemp -d "$${TMPDIR:-/tmp}/orlix-root-gmake-contract.XXXXXX")"; \
	trap 'rm -rf "$$tmp"' EXIT; \
	fail() { printf 'root GNU Make contract test failed: %s\n' "$$*" >&2; exit 1; }; \
	mac_make=/usr/bin/make; root_make="$(CURDIR)/Makefile"; \
	[ -x "$$mac_make" ] || fail 'missing macOS /usr/bin/make'; \
	"$$mac_make" --no-print-directory -f "$$root_make" help > "$$tmp/help" || fail '/usr/bin/make help did not re-exec GNU Make'; \
	grep -Fq 'Project Makefiles:' "$$tmp/help" || fail '/usr/bin/make help did not run the root help target'; \
	"$$mac_make" --no-print-directory -f "$$root_make" __orlix-tcti-instruction-artifact-inputs-source-check > "$$tmp/source-check" || fail '/usr/bin/make source target did not re-exec GNU Make'; \
	grep -Fq 'instruction artifact sha256:' "$$tmp/source-check" || fail '/usr/bin/make source target did not complete'; \
	"$$mac_make" --no-print-directory -f "$$root_make" Makefile > "$$tmp/arbitrary-goal" || fail '/usr/bin/make arbitrary existing goal did not preserve child success'; \
	grep -Fq "Nothing to be done for 'Makefile'." "$$tmp/arbitrary-goal" || fail '/usr/bin/make arbitrary existing goal did not run the re-exec child'; \
	if "$$mac_make" --no-print-directory -f "$$root_make" __orlix_root_gmake_contract_nonexistent_goal > "$$tmp/exit-status" 2>&1; then fail '/usr/bin/make arbitrary missing goal accepted'; fi; \
	grep -Fq "No rule to make target '__orlix_root_gmake_contract_nonexistent_goal'" "$$tmp/exit-status" || fail '/usr/bin/make arbitrary missing goal did not preserve child failure'; \
	if ORLIX_GMAKE="$$tmp/missing-gmake" "$$mac_make" --no-print-directory -f "$$root_make" help > "$$tmp/missing" 2>&1; then fail 'missing GNU Make was accepted'; fi; \
	grep -Fq 'missing required GNU Make >= 4.0 tool' "$$tmp/missing" || fail 'missing GNU Make diagnostic changed'; \
	if ORLIX_GMAKE="$$mac_make" "$$mac_make" --no-print-directory -f "$$root_make" help > "$$tmp/incompatible" 2>&1; then fail 'incompatible GNU Make was accepted'; fi; \
	grep -Fq 'requires GNU Make >= 4.0' "$$tmp/incompatible" || fail 'incompatible GNU Make diagnostic changed'; \
	if ORLIX_ROOT_GMAKE_REEXEC=1 "$$mac_make" --no-print-directory -f "$$root_make" help > "$$tmp/guard" 2>&1; then fail 'GNU Make re-exec guard was accepted'; fi; \
	grep -Fq 're-exec guard tripped' "$$tmp/guard" || fail 'GNU Make re-exec guard diagnostic changed'; \
	if "$$mac_make" --no-print-directory -f "$$root_make" MAKE_VERSION=4.4.1 help > "$$tmp/make-version-command" 2>&1; then fail 'command-line MAKE_VERSION override was accepted'; fi; \
	grep -Fq 'refuses caller override of MAKE_VERSION' "$$tmp/make-version-command" || fail 'command-line MAKE_VERSION override diagnostic changed'; \
	if MAKE_VERSION=4.4.1 "$$mac_make" --no-print-directory -f "$$root_make" help > "$$tmp/make-version-environment" 2>&1; then fail 'environment MAKE_VERSION override was accepted'; fi; \
	grep -Fq 'refuses caller override of MAKE_VERSION' "$$tmp/make-version-environment" || fail 'environment MAKE_VERSION override diagnostic changed'; \
	"$$mac_make" --no-print-directory -f OrlixKernel/Makefile __orlix-tcti-instruction-artifact-inputs-source-check > "$$tmp/kernel-source-check" || fail 'direct /usr/bin/make OrlixKernel source target did not re-exec GNU Make'; \
	grep -Fq 'instruction artifact sha256:' "$$tmp/kernel-source-check" || fail 'direct /usr/bin/make OrlixKernel source target did not complete'; \
	if "$$mac_make" --no-print-directory -f OrlixKernel/Makefile ORLIX_GNU_MAKE_CONTRACT_MAJOR=4 __orlix-tcti-instruction-artifact-inputs-source-check > "$$tmp/kernel-make-major-command" 2>&1; then fail 'direct OrlixKernel command-line Make major override was accepted'; fi; \
	grep -Fq 'refuses caller override of ORLIX_GNU_MAKE_CONTRACT_MAJOR' "$$tmp/kernel-make-major-command" || fail 'direct OrlixKernel command-line Make major override diagnostic changed'; \
	if ORLIX_GNU_MAKE_CONTRACT_MAJOR=4 "$$mac_make" --no-print-directory -f OrlixKernel/Makefile __orlix-tcti-instruction-artifact-inputs-source-check > "$$tmp/kernel-make-major-environment" 2>&1; then fail 'direct OrlixKernel environment Make major override was accepted'; fi; \
	grep -Fq 'refuses caller override of ORLIX_GNU_MAKE_CONTRACT_MAJOR' "$$tmp/kernel-make-major-environment" || fail 'direct OrlixKernel environment Make major override diagnostic changed'; \
	printf '%s\n' 'root GNU Make contract source check: passed'

setup-env: check-build-tools
	@$(KERNEL_MAKE) setup-env

check-build-tools:
	@set -euo pipefail; \
	if ! command -v brew >/dev/null 2>&1; then \
		echo "Homebrew is required to check Orlix build tool dependencies; install Homebrew, then run: brew bundle --file Brewfile" >&2; \
		exit 1; \
	fi; \
	if ! brew bundle check --file Brewfile; then \
		echo "missing Orlix build tool dependencies; install them with: brew bundle --file Brewfile" >&2; \
		exit 1; \
	fi

product-build-prepare:
	@set -euo pipefail; \
	current="$$(awk -F': *' '/^[[:space:]]*CURRENT_PROJECT_VERSION:/ { gsub(/"/, "", $$2); print $$2; exit }' project.yml)"; \
	[[ "$$current" =~ ^[0-9]+$$ ]] || { echo "CURRENT_PROJECT_VERSION must be an integer in project.yml, got: $$current" >&2; exit 1; }; \
	head_build="$$(git show HEAD:project.yml 2>/dev/null | awk -F': *' '/^[[:space:]]*CURRENT_PROJECT_VERSION:/ { gsub(/"/, "", $$2); print $$2; exit }')"; \
	product_paths=( \
		OrlixKernel/Makefile OrlixKernel/Sources/boot OrlixKernel/Sources/include OrlixKernel/Sources/Support OrlixKernel/Sources/ports/orlix \
		OrlixHostAdapter/Makefile OrlixHostAdapter/Sources \
		OrlixMLibC/Makefile OrlixMLibC/Sources \
		OrlixOS/Makefile OrlixOS/Sources \
		Orlix/Makefile Orlix OrlixOSTestApp/Sources \
	); \
	working_changes="$$(git status --porcelain=v1 --untracked-files=all -- "$${product_paths[@]}" | grep -v ' project.yml$$' || true)"; \
	project_nonsemantic_keys='CURRENT_PROJECT_VERSION|MARKETING_VERSION|CODE_SIGN_STYLE|CODE_SIGN_IDENTITY|DEVELOPMENT_TEAM|PROVISIONING_PROFILE_SPECIFIER'; \
	project_semantic_changes="$$(git diff HEAD -- project.yml | grep -E '^[+-]' | grep -vE '^(---|\+\+\+|[+-][[:space:]]*('"$$project_nonsemantic_keys"'):[[:space:]])' || true)"; \
	if [ -n "$$project_semantic_changes" ]; then working_changes="$$working_changes project.yml"; fi; \
	baseline="$$(git log -G 'CURRENT_PROJECT_VERSION:' -1 --format=%H -- project.yml)"; \
	[ -n "$$baseline" ] || { echo "cannot find CURRENT_PROJECT_VERSION baseline in project.yml history" >&2; exit 1; }; \
	baseline_build="$$(git show "$$baseline:project.yml" | awk -F': *' '/^[[:space:]]*CURRENT_PROJECT_VERSION:/ { gsub(/"/, "", $$2); print $$2; exit }')"; \
	committed_project_semantic_changes="$$(git diff "$$baseline" HEAD -- project.yml | grep -E '^[+-]' | grep -vE '^(---|\+\+\+|[+-][[:space:]]*('"$$project_nonsemantic_keys"'):[[:space:]])' || true)"; \
	committed_product_change=false; \
	if ! git diff --quiet "$$baseline" HEAD -- "$${product_paths[@]}" || [ -n "$$committed_project_semantic_changes" ]; then committed_product_change=true; fi; \
	needs_bump=false; \
	if [ -n "$$working_changes" ] && [ "$$current" = "$$head_build" ]; then needs_bump=true; fi; \
	if [ "$$committed_product_change" = true ] && [ "$$current" = "$$baseline_build" ]; then needs_bump=true; fi; \
	if [ "$$needs_bump" = true ]; then \
		next="$$((current + 1))"; \
		perl -0pi -e 's/^([[:space:]]*CURRENT_PROJECT_VERSION:[[:space:]]*)[0-9]+([[:space:]]*)$$/$${1}'"$$next"'$${2}/m or die "CURRENT_PROJECT_VERSION not found\n"' project.yml; \
		printf '%s\n' "bumped CURRENT_PROJECT_VERSION $$current -> $$next for product input changes"; \
	else \
		printf '%s\n' "product version unchanged: CURRENT_PROJECT_VERSION=$$current"; \
	fi

product-build-version-check: product-build-prepare
	@set -euo pipefail; \
	current="$$(awk -F': *' '/^[[:space:]]*CURRENT_PROJECT_VERSION:/ { gsub(/"/, "", $$2); print $$2; exit }' project.yml)"; \
	baseline="$$(git log -G 'CURRENT_PROJECT_VERSION:' -1 --format=%H -- project.yml)"; \
	[ -n "$$baseline" ] || { echo "cannot find CURRENT_PROJECT_VERSION baseline in project.yml history" >&2; exit 1; }; \
	baseline_build="$$(git show "$$baseline:project.yml" | awk -F': *' '/^[[:space:]]*CURRENT_PROJECT_VERSION:/ { gsub(/"/, "", $$2); print $$2; exit }')"; \
	if [ "$$current" = "$$baseline_build" ]; then \
		product_paths=( \
			OrlixKernel/Makefile OrlixKernel/Sources/boot OrlixKernel/Sources/include OrlixKernel/Sources/Support OrlixKernel/Sources/ports/orlix \
			OrlixHostAdapter/Makefile OrlixHostAdapter/Sources \
			OrlixMLibC/Makefile OrlixMLibC/Sources \
			OrlixOS/Makefile OrlixOS/Sources \
			Orlix/Makefile Orlix OrlixOSTestApp/Sources \
		); \
		git diff --quiet "$$baseline" HEAD -- "$${product_paths[@]}" || { \
			echo "product inputs changed after CURRENT_PROJECT_VERSION=$$current was established; run make product-build-prepare and commit project.yml" >&2; \
			exit 1; \
		}; \
	fi

app-capability-gate: __release-manifest-check

app-capability-test: __release-tests

app-release-inputs-check: __release-inputs-check

app-release-inputs-test: __release-tests

app-exported-product-check: __exported-app-check

vvterm-sync vvterm-sync-resolve vvterm-reconcile vvterm-sync-complete vvterm-source-check vvterm-sync-tests vvterm-upstream-tests:
	@$(APP_MAKE) $@ VVTERM_COMMIT="$(VVTERM_COMMIT)" VVTERM_RESOLUTION="$(VVTERM_RESOLUTION)" VVTERM_TEST_DESTINATION="$(VVTERM_TEST_DESTINATION)"

beta-prerequisites: check-build-tools __release-inputs-check
	@set -euo pipefail; \
	command -v xcodegen >/dev/null 2>&1 || { echo "xcodegen is required; run: brew bundle --file Brewfile" >&2; exit 1; }; \
	command -v xcodebuild >/dev/null 2>&1 || { echo "xcodebuild is required" >&2; exit 1; }; \
	test -f project.yml || { echo "missing XcodeGen source: project.yml" >&2; exit 1; }

beta-bump-build-number: beta-resolve-build-number
	@printf '%s\n' "beta-bump-build-number is a compatibility alias; project.yml was not changed"

beta-resolve-build-number: beta-prerequisites
	@set -euo pipefail; \
	mkdir -p "$(ORLIX_BETA_ARCHIVE_DIR)"; \
	PYTHONPATH="$(ORLIX_RELEASE_SUPPORT_DIR)" python3 -m orlix_release_ci project-identity --project project.yml --output "$(ORLIX_RELEASE_IDENTITY_PATH)"; \
	current="$$(jq -r '.project_build_number' "$(ORLIX_RELEASE_IDENTITY_PATH)")"; \
	marketing="$$(jq -r '.marketing_version' "$(ORLIX_RELEASE_IDENTITY_PATH)")"; \
	if [ -n "$(ORLIX_BETA_BUILD_NUMBER)" ]; then \
		next="$(ORLIX_BETA_BUILD_NUMBER)"; \
	else \
		test -s "$(ORLIX_FASTLANE_API_KEY_PATH)" || { echo "ORLIX_BETA_BUILD_NUMBER or ORLIX_FASTLANE_API_KEY_PATH is required" >&2; exit 1; }; \
		latest="$$( $(ORLIX_FASTLANE) run latest_testflight_build_number api_key_path:"$(ORLIX_FASTLANE_API_KEY_PATH)" app_identifier:"$(ORLIX_APP_BUNDLE_ID)" version:"$$marketing" initial_build_number:0 | awk '/Result:/ { print $$NF }' | tail -n 1)"; \
		[[ "$$latest" =~ ^[0-9]+$$ ]] || { echo "could not resolve latest TestFlight build number" >&2; exit 1; }; \
		if [ "$$latest" -ge "$$current" ]; then next="$$((latest + 1))"; else next="$$((current + 1))"; fi; \
	fi; \
	[[ "$$next" =~ ^[1-9][0-9]*$$ ]] || { echo "beta build number must be a positive integer, got: $$next" >&2; exit 1; }; \
	tmp="$(ORLIX_BETA_BUILD_NUMBER_FILE).tmp"; \
	printf '%s\n' "$$next" > "$$tmp"; \
	mv "$$tmp" "$(ORLIX_BETA_BUILD_NUMBER_FILE)"; \
	printf '%s\n' "resolved TestFlight build $$marketing ($$next); project.yml was not changed"

beta-signing-diagnostics:
	@set -euo pipefail; \
	identity="$(ORLIX_CODE_SIGN_IDENTITY)"; \
	if [ -z "$$identity" ]; then identity="Apple Distribution"; fi; \
	[ -n "$$identity" ] || { echo "ORLIX_CODE_SIGN_IDENTITY is required, for example: Apple Distribution" >&2; exit 1; }; \
	security find-identity -v -p codesigning; \
	profile_count=0; \
	for profile_dir in \
		"$$HOME/Library/Developer/Xcode/UserData/Provisioning Profiles" \
		"$$HOME/Library/MobileDevice/Provisioning Profiles"; do \
		if [ -d "$$profile_dir" ]; then \
			count="$$(find "$$profile_dir" -maxdepth 1 -type f -name '*.mobileprovision' -print | wc -l | awk '{ print $$1 }')"; \
			printf '%s\n' "provisioning_profiles[$$profile_dir]=$$count"; \
			profile_count="$$((profile_count + count))"; \
		fi; \
	done; \
	printf '%s\n' "provisioning_profiles_total=$$profile_count"; \
	tmp="$$(mktemp -d "$${TMPDIR:-/tmp}/orlix-signing-diagnostics.XXXXXX")"; \
	trap 'rm -rf "$$tmp"' EXIT; \
	bundle="$$tmp/SigningProbe.framework"; \
	mkdir -p "$$bundle"; \
	printf '%s\n' probe > "$$bundle/SigningProbe"; \
	/usr/bin/codesign --force --sign "$$identity" "$$bundle"; \
	/usr/bin/codesign --verify --verbose=2 "$$bundle"; \
	printf '%s\n' "validated signing identity: $$identity"

beta-install-simulator: beta-prerequisites
	@set -euo pipefail; \
	xcodegen generate --spec project.yml; \
	xcodebuild \
		-project Orlix.xcodeproj \
		-scheme "$(ORLIX_BETA_SCHEME)" \
		-configuration Release \
		-destination '$(ORLIX_BETA_SIMULATOR_DESTINATION)' \
		build; \
	app="$$(xcodebuild \
		-project Orlix.xcodeproj \
		-scheme "$(ORLIX_BETA_SCHEME)" \
		-configuration Release \
		-destination '$(ORLIX_BETA_SIMULATOR_DESTINATION)' \
		-showBuildSettings \
		| awk -F' = ' '/TARGET_BUILD_DIR = / { build_dir=$$2 } /WRAPPER_NAME = / { wrapper=$$2 } END { if (build_dir != "" && wrapper != "") print build_dir "/" wrapper }')"; \
	test -d "$$app" || { echo "missing built simulator app: $$app" >&2; exit 1; }; \
	xcrun simctl terminate "$(ORLIX_BETA_SIMULATOR_ID)" "$(ORLIX_APP_BUNDLE_ID)" >/dev/null 2>&1 || true; \
	xcrun simctl uninstall "$(ORLIX_BETA_SIMULATOR_ID)" "$(ORLIX_APP_BUNDLE_ID)" >/dev/null 2>&1 || true; \
	xcrun simctl install "$(ORLIX_BETA_SIMULATOR_ID)" "$$app"; \
	installed_app="$$(xcrun simctl get_app_container "$(ORLIX_BETA_SIMULATOR_ID)" "$(ORLIX_APP_BUNDLE_ID)" app)"; \
	payload_info="$$installed_app/Frameworks/OrlixOS.framework/OrlixOSManifest.plist"; \
	test -f "$$payload_info" || { echo "missing installed OrlixOS payload metadata: $$payload_info" >&2; exit 1; }; \
	plutil -extract OrlixSelectedRootMode raw -o - "$$payload_info" | grep -qx 'direct'; \
	plutil -extract OrlixKernelCommandLine raw -o - "$$payload_info" | grep -q 'root=/dev/vda'; \
	xcrun simctl launch "$(ORLIX_BETA_SIMULATOR_ID)" "$(ORLIX_APP_BUNDLE_ID)"

ios15-simulator-gate:
ifeq ($(ORLIX_BAZEL_AUTHORITY),1)
	@$(MAKE) __bazel-ios15-simulator-gate
else
	@set -euo pipefail; \
	test -n "$(ORLIX_IOS15_SIMULATOR_ID)" || { echo "ORLIX_IOS15_SIMULATOR_ID is required" >&2; exit 1; }; \
	runtime="$$(xcrun simctl list devices -j | jq -r --arg id "$(ORLIX_IOS15_SIMULATOR_ID)" '.devices | to_entries[] | select(.key | contains("iOS-15-5")) | .value[] | select(.udid == $$id and .isAvailable == true) | .udid')"; \
	test "$$runtime" = "$(ORLIX_IOS15_SIMULATOR_ID)" || { echo "the selected simulator is not an available iOS 15.5 device" >&2; exit 1; }; \
	xcrun simctl bootstatus "$(ORLIX_IOS15_SIMULATOR_ID)" -b; \
	xcodegen generate --spec project.yml; \
	PYTHONPATH="$(CURDIR)/make" python3 -m unittest test_ios15_simulator_gate; \
	PYTHONPATH="$(CURDIR)/make" python3 -c 'from pathlib import Path; import ios15_simulator_gate as gate; gate.validate_generated_project(Path("Orlix.xcodeproj/project.pbxproj")); print("pass: AppIntents.framework is weakly linked")'; \
	result_dir="$(ORLIX_BUILD_ROOT)/iOS15"; \
	result_bundle="$$result_dir/Orlix-iOS15.xcresult"; \
	result_log="$$result_dir/Orlix-iOS15.log"; \
	derived_data="$$result_dir/DerivedData"; \
	mkdir -p "$$result_dir"; \
	rm -rf "$$result_bundle"; \
	destination="platform=iOS Simulator,id=$(ORLIX_IOS15_SIMULATOR_ID)"; \
	xcodebuild \
		-project Orlix.xcodeproj \
		-scheme "Orlix UI Tests" \
		-configuration Debug \
		-destination "$$destination" \
		-derivedDataPath "$$derived_data" \
		-resultBundlePath "$$result_bundle" \
		ENABLE_DEBUG_DYLIB=NO \
		-only-testing:OrlixUITests/AppLaunchSmokeUITests/testLaunchCapturesScreenshot \
		build-for-testing 2>&1 | tee "$$result_log"; \
	app="$$derived_data/Build/Products/Debug-iphonesimulator/Orlix.app"; \
	test -d "$$app" || { echo "missing iOS 15 simulator app: $$app" >&2; exit 1; }; \
	PYTHONPATH="$(CURDIR)/make" ORLIX_IOS15_APP="$$app" python3 -c 'import os; from pathlib import Path; import ios15_simulator_gate as gate; gate.validate_simulator_app(Path(os.environ["ORLIX_IOS15_APP"])); print("pass: iOS 15 app does not required-load AppIntents or ActivityKit")'; \
	rm -rf "$$result_bundle"; \
	xcodebuild \
		-project Orlix.xcodeproj \
		-scheme "Orlix UI Tests" \
		-configuration Debug \
		-destination "$$destination" \
		-derivedDataPath "$$derived_data" \
		-resultBundlePath "$$result_bundle" \
		ENABLE_DEBUG_DYLIB=NO \
		-only-testing:OrlixUITests/AppLaunchSmokeUITests/testLaunchCapturesScreenshot \
		-test-timeouts-enabled YES \
		-default-test-execution-time-allowance 120 \
		-maximum-test-execution-time-allowance 180 \
		test-without-building 2>&1 | tee -a "$$result_log"
endif

beta-simulator-gate: beta-prerequisites
	@set -euo pipefail; \
	xcodegen generate --spec project.yml; \
	xcodebuild \
		-project Orlix.xcodeproj \
		-scheme "OrlixOS Tests" \
		-configuration Debug \
		-destination '$(ORLIX_BETA_SIMULATOR_DESTINATION)' \
		-only-testing:OrlixOSTests/OrlixOSSessionTests/testResourcesAreResolvedDirectlyFromOrlixOSFramework \
		-only-testing:OrlixOSTests/OrlixOSSessionTests/testRootImageDescriptorsComeFromOrlixOSTargetMetadata \
		test; \
	xcodebuild \
		-project Orlix.xcodeproj \
		-scheme "OrlixOS Runtime Tests" \
		-configuration Debug \
		-destination '$(ORLIX_BETA_SIMULATOR_DESTINATION)' \
		-only-testing:OrlixOSRuntimeTests/OrlixOSRuntimeTests/testLinuxPTYCarriesInteractiveShellInputAndOutput \
		test; \
	xcodebuild \
		-project Orlix.xcodeproj \
		-scheme "OrlixOS Runtime Tests" \
		-configuration Debug \
		-destination '$(ORLIX_BETA_SIMULATOR_DESTINATION)' \
		-only-testing:OrlixOSRuntimeTests/OrlixEnvironmentRootRuntimeTests/testOCIDerivedMaterializedRootBindsDescriptorExecutionDefaults \
		test

orlix-tcti-semantic-provenance-tests:
	@mkdir -p '$(dir $(ORLIX_TCTI_SEMANTIC_PROVENANCE_TEST))'
	@$(call orlix_tcti_write_proof_registry_provenance,$(ORLIX_TCTI_PROOF_REGISTRY_PROVENANCE_HEADER),$(word 1,$(ORLIX_TCTI_PROOF_REGISTRY_PROVENANCE_INPUTS)),$(word 2,$(ORLIX_TCTI_PROOF_REGISTRY_PROVENANCE_INPUTS)),$(word 3,$(ORLIX_TCTI_PROOF_REGISTRY_PROVENANCE_INPUTS)))
	@$(CC) -std=c11 -Wall -Wextra -Werror -pedantic \
		-IOrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests \
		-I'$(dir $(ORLIX_TCTI_PROOF_REGISTRY_PROVENANCE_HEADER))' \
		OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/target_feature_artifact.c \
		OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/target_feature_applicability_artifact.c \
		OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/target_feature_domain.c \
		OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/target_feature_field_domain_binding_artifact.c \
		OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/target_runtime_capability_cohort_artifact.c \
		OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/target_instruction_artifact.c \
		OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/target_proof_registry.c \
		OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/target_completion_audit.c \
		OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/target_completion_semantic_provenance_test.c \
		-o '$(ORLIX_TCTI_SEMANTIC_PROVENANCE_TEST)'
	@'$(ORLIX_TCTI_SEMANTIC_PROVENANCE_TEST)'

orlix-tcti-operational-note-pipeline-test: orlix-tcti-isa-maintainer-source-check
	@$(KERNEL_MAKE) __tcti-operational-note-pipeline-test

orlix-tcti-isa-maintainer-source-check:
	@test -n '$(ORLIX_AARCHMRS_INSTRUCTIONS)' || { \
		echo 'ORLIX_AARCHMRS_INSTRUCTIONS must point to the pinned AARCHMRS Instructions.json' >&2; \
		exit 2; \
	}
	@test -f '$(ORLIX_AARCHMRS_INSTRUCTIONS)' || { \
		echo 'missing pinned AARCHMRS input: $(ORLIX_AARCHMRS_INSTRUCTIONS)' >&2; \
		exit 2; \
	}
	@$(KERNEL_MAKE) __tcti-instruction-source-check

orlix-tcti-isa-host-tests: __orlix-tcti-isa-host-provenance-ready __orlix-tcti-instruction-artifact-inputs-source-check

ORLIX_TCTI_PROOF_REGISTRY_PROVENANCE_HEADER := $(ORLIX_BUILD_ROOT)/Tests/orlix-tcti-isa/target_proof_registry_provenance.h
ORLIX_TCTI_PROOF_REGISTRY_PROVENANCE_INPUTS := \
	OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/orlix_tcti_decode_test.c \
	OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/orlix_tcti_system_accessor_partition_test.h \
	OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/Makefile

__orlix-tcti-proof-registry-provenance-write:
	@$(call orlix_tcti_write_proof_registry_provenance,$(ORLIX_TCTI_PROOF_REGISTRY_PROVENANCE_OUTPUT),$(ORLIX_TCTI_PROOF_REGISTRY_PROVENANCE_DECODE_INPUT),$(ORLIX_TCTI_PROOF_REGISTRY_PROVENANCE_PARTITION_INPUT),$(ORLIX_TCTI_PROOF_REGISTRY_PROVENANCE_BUILD_INPUT))

orlix-tcti-proof-registry-provenance-regression:
	@set -eu; regression_parent="$$(mktemp -d "$${TMPDIR:-$(ORLIX_BUILD_ROOT)/tmp}/orlix-tcti-proof-registry-provenance.XXXXXX")"; \
	trap 'rm -rf "$$regression_parent"' EXIT; \
	cleanup_marker="$$regression_parent/cleanup-marker"; \
	( set -eu; regression_root="$$regression_parent/fixture"; mkdir "$$regression_root"; \
	trap 'rm -rf "$$regression_root"; : > "$$cleanup_marker"' EXIT; \
	decode="$$regression_root/decode.c"; partition="$$regression_root/partition.h"; build="$$regression_root/Makefile"; header="$$regression_root/target_proof_registry_provenance.h"; missing="$$regression_root/missing.c"; missing_log="$$regression_root/missing.log"; malformed_log="$$regression_root/malformed.log"; \
	printf '%s\n' 'initial decode source' > "$$decode"; printf '%s\n' 'initial partition source' > "$$partition"; printf '%s\n' 'initial KUnit build source' > "$$build"; \
	$(MAKE) --no-print-directory __orlix-tcti-proof-registry-provenance-write ORLIX_TCTI_PROOF_REGISTRY_PROVENANCE_OUTPUT="$$header" ORLIX_TCTI_PROOF_REGISTRY_PROVENANCE_DECODE_INPUT="$$decode" ORLIX_TCTI_PROOF_REGISTRY_PROVENANCE_PARTITION_INPUT="$$partition" ORLIX_TCTI_PROOF_REGISTRY_PROVENANCE_BUILD_INPUT="$$build"; \
	initial_mtime="$$(stat -f %m "$$header")"; sleep 1; \
	$(MAKE) --no-print-directory __orlix-tcti-proof-registry-provenance-write ORLIX_TCTI_PROOF_REGISTRY_PROVENANCE_OUTPUT="$$header" ORLIX_TCTI_PROOF_REGISTRY_PROVENANCE_DECODE_INPUT="$$decode" ORLIX_TCTI_PROOF_REGISTRY_PROVENANCE_PARTITION_INPUT="$$partition" ORLIX_TCTI_PROOF_REGISTRY_PROVENANCE_BUILD_INPUT="$$build"; \
	[ "$$initial_mtime" = "$$(stat -f %m "$$header")" ] || { echo 'unchanged provenance header refreshed and invalidated archive caches' >&2; exit 1; }; \
	cp "$$header" "$$regression_root/initial.h"; \
	printf '%s\n' 'mutated decode source' >> "$$decode"; printf '%s\n' 'stale output' > "$$header"; \
	$(MAKE) --no-print-directory __orlix-tcti-proof-registry-provenance-write ORLIX_TCTI_PROOF_REGISTRY_PROVENANCE_OUTPUT="$$header" ORLIX_TCTI_PROOF_REGISTRY_PROVENANCE_DECODE_INPUT="$$decode" ORLIX_TCTI_PROOF_REGISTRY_PROVENANCE_PARTITION_INPUT="$$partition" ORLIX_TCTI_PROOF_REGISTRY_PROVENANCE_BUILD_INPUT="$$build"; \
	cmp -s "$$regression_root/initial.h" "$$header" && { echo 'provenance header did not refresh after input mutation' >&2; exit 1; }; \
	cp "$$header" "$$regression_root/valid.h"; \
	test -d "$$regression_root" && test ! -e "$$missing" || { echo 'missing provenance fixture was not absent in a live temporary directory' >&2; exit 1; }; \
	if $(MAKE) --no-print-directory __orlix-tcti-proof-registry-provenance-write ORLIX_TCTI_PROOF_REGISTRY_PROVENANCE_OUTPUT="$$header" ORLIX_TCTI_PROOF_REGISTRY_PROVENANCE_DECODE_INPUT="$$missing" ORLIX_TCTI_PROOF_REGISTRY_PROVENANCE_PARTITION_INPUT="$$partition" ORLIX_TCTI_PROOF_REGISTRY_PROVENANCE_BUILD_INPUT="$$build" > "$$missing_log" 2>&1; then echo 'missing provenance input unexpectedly succeeded' >&2; exit 1; fi; \
	test -d "$$regression_root" && test ! -e "$$missing" || { echo 'missing provenance fixture directory was removed before failure handling' >&2; exit 1; }; \
	grep -F -- "$$missing" "$$missing_log" >/dev/null && grep -F 'No such file or directory' "$$missing_log" >/dev/null || { echo 'missing provenance input did not fail for the absent fixture' >&2; exit 1; }; \
	cmp -s "$$regression_root/valid.h" "$$header" || { echo 'missing provenance input corrupted the valid header' >&2; exit 1; }; \
	printf '%s\n' '#!/bin/sh' 'printf "%s\\n" "not-a-sha256  -"' > "$$regression_root/malformed-shasum"; chmod +x "$$regression_root/malformed-shasum"; \
	if $(MAKE) --no-print-directory __orlix-tcti-proof-registry-provenance-write ORLIX_TCTI_SHASUM="$$regression_root/malformed-shasum" ORLIX_TCTI_PROOF_REGISTRY_PROVENANCE_OUTPUT="$$header" ORLIX_TCTI_PROOF_REGISTRY_PROVENANCE_DECODE_INPUT="$$decode" ORLIX_TCTI_PROOF_REGISTRY_PROVENANCE_PARTITION_INPUT="$$partition" ORLIX_TCTI_PROOF_REGISTRY_PROVENANCE_BUILD_INPUT="$$build" > "$$malformed_log" 2>&1; then echo 'malformed provenance hash unexpectedly succeeded' >&2; exit 1; fi; \
	grep -F '__orlix-tcti-proof-registry-provenance-write' "$$malformed_log" >/dev/null && grep -F 'Error 1' "$$malformed_log" >/dev/null || { echo 'malformed provenance hash did not fail in the provenance writer' >&2; exit 1; }; \
	cmp -s "$$regression_root/valid.h" "$$header" || { echo 'malformed provenance hash corrupted the valid header' >&2; exit 1; }; \
	if find "$$regression_root" -name 'target_proof_registry_provenance.h.tmp.*' -exec false \;; then :; else echo 'provenance temporary files were retained' >&2; exit 1; fi ); \
	test -f "$$cleanup_marker" && test ! -e "$$regression_parent/fixture" || { echo 'provenance fixture cleanup did not run after assertions' >&2; exit 1; }; \
	printf '%s\n' 'OrlixTCTI proof registry provenance regression: passed'

__orlix-tcti-isa-host-provenance-ready: orlix-tcti-proof-registry-provenance-regression
	@$(MAKE) --no-print-directory orlix-tcti-semantic-provenance-tests

orlix-tcti-isa-host-provenance-dependency-regression:
	@set -euo pipefail; \
	makefile="$(CURDIR)/Makefile"; \
	fail() { printf 'OrlixTCTI provenance dependency regression failed: %s\n' "$$*" >&2; exit 1; }; \
	host_prerequisites="$$(awk '/^orlix-tcti-isa-host-tests:/ { sub(/^[^:]*:[[:space:]]*/, ""); print; exit }' "$$makefile")"; \
	[ "$$host_prerequisites" = '__orlix-tcti-isa-host-provenance-ready __orlix-tcti-instruction-artifact-inputs-source-check' ] || fail 'host target bypasses the provenance readiness chain'; \
	audit_prerequisites="$$(awk '/^orlix-tcti-isa-audit:/ { sub(/^[^:]*:[[:space:]]*/, ""); print; exit }' "$$makefile")"; \
	[ "$$audit_prerequisites" = 'orlix-tcti-isa-host-tests orlix-tcti-native-proof-symbol-check' ] || fail 'aggregate target retains a direct semantic provenance prerequisite'; \
	readiness_prerequisites="$$(awk '/^__orlix-tcti-isa-host-provenance-ready:/ { sub(/^[^:]*:[[:space:]]*/, ""); print; exit }' "$$makefile")"; \
	[ "$$readiness_prerequisites" = 'orlix-tcti-proof-registry-provenance-regression' ] || fail 'readiness target does not begin with the negative fixture'; \
	semantic_invocations="$$(awk '/^__orlix-tcti-isa-host-provenance-ready:/ { in_readiness=1; next } in_readiness && /^[^[:space:]#][^:]*:/ { exit } in_readiness && /orlix-tcti-semantic-provenance-tests/ { count++ } END { print count + 0 }' "$$makefile")"; \
	[ "$$semantic_invocations" = 1 ] || fail "expected one semantic provenance generation invocation, got $$semantic_invocations"; \
	printf '%s\n' 'OrlixTCTI provenance dependency regression: passed'

orlix-tcti-isa-host-tests:
	@mkdir -p '$(dir $(ORLIX_TCTI_INVENTORY_CONTRACT_TEST))'
	@$(CC) -std=c17 -Wall -Wextra -Werror \
		OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/inventory_contract.c \
		OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/inventory_contract_test.c \
		-o '$(ORLIX_TCTI_INVENTORY_CONTRACT_TEST)'
	@'$(ORLIX_TCTI_INVENTORY_CONTRACT_TEST)'
	@$(CC) -std=c11 -Wall -Wextra -Werror -pedantic \
		OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/host_lane_boundary_test.c \
		-o '$(ORLIX_TCTI_HOST_LANE_BOUNDARY_TEST)'
	@env -i PATH="$(PATH)" '$(ORLIX_TCTI_HOST_LANE_BOUNDARY_TEST)' .
	@$(CC) -std=c11 -Wall -Wextra -Werror -pthread \
		-I'$(dir $(ORLIX_TCTI_PROOF_REGISTRY_PROVENANCE_HEADER))' \
		OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/target_proof_registry.c \
		OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/target_proof_registry_test.c \
		-o '$(ORLIX_TCTI_TARGET_PROOF_REGISTRY_TEST)'
	@'$(ORLIX_TCTI_TARGET_PROOF_REGISTRY_TEST)'
	@set -euo pipefail; \
	$(orlix_tcti_file_sha256) \
	$(orlix_tcti_candidate_source_revision) \
	$(orlix_tcti_manifest_sha256) \
	$(orlix_tcti_proof_profile_sha256) \
	$(orlix_tcti_proof_inputs_sha256) \
	$(call orlix_tcti_assign_instruction_artifact_sha256,proof_artifact_sha256,exit 1) \
	proof_source_paths="$$(mktemp "$${TMPDIR:-$(ORLIX_BUILD_ROOT)/tmp}/orlix-tcti-host-candidate-paths.XXXXXX")"; \
	trap 'rm -f "$$proof_source_paths"' EXIT; \
	git -C "$(CURDIR)" ls-files -co --exclude-standard -z -- Makefile OrlixKernel/Makefile OrlixKernel/Sources/ports/orlix > "$$proof_source_paths"; \
	proof_source_revision="$$(orlix_tcti_candidate_source_revision "$(CURDIR)" "$$proof_source_paths")"; \
	proof_config_sha256="$$(orlix_tcti_file_sha256 "OrlixKernel/Sources/ports/orlix/configs/$(PROFILE)_defconfig")"; \
	proof_profile_sha256="$$(orlix_tcti_proof_profile_sha256 "$(PROFILE)" "OrlixKernel/Sources/ports/orlix/configs/$(PROFILE)_defconfig")"; \
	printf -v proof_archive_prefix 'lane=host-proof-ingestion\nsource_revision=%s\n' "$$proof_source_revision"; \
	proof_archive_sha256="$$(orlix_tcti_proof_inputs_sha256 "$$proof_archive_prefix" Makefile OrlixKernel/Makefile OrlixKernel/Sources/ports/orlix/kbuild/proof-provenance.mk OrlixKernel/Sources/ports/orlix/configs/$(PROFILE)_defconfig OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/target_proof_ingestion.c OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/target_proof_ingestion_test.c)"; \
	$(CC) -DORLIX_TCTI_PROOF_INGESTION_HOST_TEST -DORLIX_TCTI_KERNEL_ARCHIVE_INPUT_SHA256=\"$$proof_archive_sha256\" -DORLIX_TCTI_KERNEL_CONFIG_SHA256=\"$$proof_config_sha256\" -DORLIX_TCTI_BUILD_PROFILE_SHA256=\"$$proof_profile_sha256\" -DORLIX_TCTI_DURABLE_SOURCE_REVISION=\"$$proof_source_revision\" -DORLIX_TCTI_INSTRUCTION_ARTIFACT_SHA256=\"$$proof_artifact_sha256\" -std=c11 -Wall -Wextra -Werror -pedantic \
		-IOrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests \
		OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/target_proof_ingestion.c \
		OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/target_proof_ingestion_test.c \
		-o '$(ORLIX_TCTI_TARGET_PROOF_INGESTION_TEST)'
	@'$(ORLIX_TCTI_TARGET_PROOF_INGESTION_TEST)'
	@$(CC) -DORLIX_TCTI_RUNTIME_PROJECTION_HOST_TEST -std=c11 \
		-Wall -Wextra -Werror -pedantic \
		OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/runtime_projection.c \
		OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/target_runtime_projection_test.c \
		-o '$(ORLIX_TCTI_RUNTIME_PROJECTION_TEST)'
	@'$(ORLIX_TCTI_RUNTIME_PROJECTION_TEST)'
	@$(CC) -std=c11 -Wall -Wextra -Werror -pedantic \
		OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/target_system_access_selector.c \
		OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/target_system_access_selector_test.c \
		-o '$(ORLIX_TCTI_SYSTEM_ACCESS_SELECTOR_TEST)'
	@'$(ORLIX_TCTI_SYSTEM_ACCESS_SELECTOR_TEST)'
	@$(CC) -std=c11 -Wall -Wextra -Werror -pedantic \
		OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/target_scalar_operation_catalog.c \
		OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/target_scalar_operation_catalog_test.c \
		-o '$(ORLIX_TCTI_SCALAR_OPERATION_CATALOG_TEST)'
	@'$(ORLIX_TCTI_SCALAR_OPERATION_CATALOG_TEST)'
	@$(CC) -std=c11 -Wall -Wextra -Werror -pedantic \
		OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/target_lse_operation_catalog.c \
		OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/target_lse_operation_catalog_test.c \
		-o '$(ORLIX_TCTI_LSE_OPERATION_CATALOG_TEST)'
	@'$(ORLIX_TCTI_LSE_OPERATION_CATALOG_TEST)'
	@$(CC) -std=c11 -Wall -Wextra -Werror -pedantic \
		OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/target_scalar_operation_catalog.c \
		OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/target_lse_operation_catalog.c \
		OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/target_proof_candidate.c \
		OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/target_proof_candidate_test.c \
		-o '$(ORLIX_TCTI_PROOF_CANDIDATE_TEST)'
	@'$(ORLIX_TCTI_PROOF_CANDIDATE_TEST)'
	@$(CC) -O2 -std=c11 -Wall -Wextra -Werror -pedantic \
		OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/target_isa_kbuild_generator_test.c \
		-o '$(ORLIX_TCTI_TARGET_KBUILD_GENERATOR_TEST)'
	@'$(ORLIX_TCTI_TARGET_KBUILD_GENERATOR_TEST)'
	@$(CC) -std=c11 -Wall -Wextra -Werror -pedantic \
		-IOrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests \
		-IOrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/isa \
		OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/target_instruction_artifact.c \
		OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/target_instruction_artifact_roundtrip_test.c \
		-o '$(ORLIX_TCTI_INSTRUCTION_ARTIFACT_ROUNDTRIP_TEST)'
	@'$(ORLIX_TCTI_INSTRUCTION_ARTIFACT_ROUNDTRIP_TEST)'
	@$(CC) -std=c11 -Wall -Wextra -Werror -pedantic \
		-IOrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests \
		-IOrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/isa \
		OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/target_instruction_artifact.c \
		OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/target_instruction_artifact_generated_mutation_test.c \
		-o '$(ORLIX_TCTI_INSTRUCTION_ARTIFACT_MUTATION_TEST)'
	@'$(ORLIX_TCTI_INSTRUCTION_ARTIFACT_MUTATION_TEST)'
	@$(CC) -std=c11 -Wall -Wextra -Werror -pedantic \
		-IOrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests \
		OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/target_feature_artifact.c \
		OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/target_feature_artifact_test.c \
		-o '$(ORLIX_TCTI_FEATURE_ARTIFACT_VALIDATOR_TEST)'
	@'$(ORLIX_TCTI_FEATURE_ARTIFACT_VALIDATOR_TEST)'
	@$(CC) -std=c11 -Wall -Wextra -Werror -pedantic \
		-IOrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests \
		-IOrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/isa \
		OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/target_feature_artifact.c \
		OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/target_instruction_artifact.c \
		OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/target_feature_domain.c \
		OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/target_feature_field_domain_binding_artifact.c \
		OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/target_feature_applicability_artifact.c \
		OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/target_feature_applicability_artifact_test.c \
		-o '$(ORLIX_TCTI_FEATURE_APPLICABILITY_ARTIFACT_TEST)'
	@'$(ORLIX_TCTI_FEATURE_APPLICABILITY_ARTIFACT_TEST)'
	@$(CC) -std=c11 -Wall -Wextra -Werror -pedantic \
		-IOrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests \
		-IOrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/isa \
		OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/target_feature_artifact.c \
		OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/target_instruction_artifact.c \
		OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/target_feature_domain.c \
		OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/target_feature_domain_test.c \
		-o '$(ORLIX_TCTI_FEATURE_DOMAIN_TEST)'
	@'$(ORLIX_TCTI_FEATURE_DOMAIN_TEST)'
	@$(CC) -std=c11 -Wall -Wextra -Werror -pedantic \
		-IOrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests \
		OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/target_feature_artifact.c \
		OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/target_feature_field_domain_binding_artifact.c \
		OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/target_feature_field_domain_binding_artifact_test.c \
		-o '$(ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_TEST)'
	@'$(ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_TEST)'
	@$(CC) -std=c11 -Wall -Wextra -Werror -pedantic \
		-IOrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests \
		OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/target_feature_artifact.c \
		OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/target_runtime_capability_cohort_artifact.c \
		OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/target_runtime_capability_cohort_artifact_test.c \
		-o '$(ORLIX_TCTI_RUNTIME_CAPABILITY_COHORT_TEST)'
	@'$(ORLIX_TCTI_RUNTIME_CAPABILITY_COHORT_TEST)'
	@$(CC) -std=c11 -Wall -Wextra -Werror -pedantic \
		-IOrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests \
		OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/target_instruction_artifact.c \
		OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/target_execution_slice_map.c \
		OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/target_execution_slice_map_test.c \
		-o '$(ORLIX_TCTI_EXECUTION_SLICE_MAP_TEST)'
	@'$(ORLIX_TCTI_EXECUTION_SLICE_MAP_TEST)'
	@$(CC) -std=c11 -Wall -Wextra -Werror -pedantic \
		-IOrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/isa \
		OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/isa/target_ordinal_ledger.c \
		OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/target_ordinal_ledger_test.c \
		-o '$(ORLIX_TCTI_ORDINAL_LEDGER_TEST)'
	@'$(ORLIX_TCTI_ORDINAL_LEDGER_TEST)'

orlix-tcti-native-proof-symbol-check:
	@$(KERNEL_MAKE) orlix-tcti-native-proof-symbol-check

orlix-tcti-native-proof-symbol-check-dependency-regression:
	@$(KERNEL_MAKE) orlix-tcti-native-proof-symbol-check-dependency-regression

orlix-tcti-proof-source-linkage-tests:
	@$(KERNEL_MAKE) orlix-tcti-proof-source-linkage-tests

orlix-tcti-isa-audit: orlix-tcti-isa-host-tests orlix-tcti-native-proof-symbol-check
	@mkdir -p '$(dir $(ORLIX_TCTI_INVENTORY_AUDITOR))'
	@$(CC) -std=c11 -Wall -Wextra -Werror -pedantic \
		-IOrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests \
		-I'$(dir $(ORLIX_TCTI_PROOF_REGISTRY_PROVENANCE_HEADER))' \
		OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/target_feature_artifact.c \
		OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/target_feature_applicability_artifact.c \
		OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/target_feature_domain.c \
		OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/target_feature_field_domain_binding_artifact.c \
		OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/target_runtime_capability_cohort_artifact.c \
		OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/target_instruction_artifact.c \
		OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/target_proof_registry.c \
		OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/target_completion_audit.c \
		OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/target_completion_audit_main.c \
		-o '$(ORLIX_TCTI_INVENTORY_AUDITOR)'
	@'$(ORLIX_TCTI_INVENTORY_AUDITOR)'

# `prepare type=tcti-isa` validates pinned Arm inputs and atomically selects
# one immutable authoritative C generation below arch/orlix.
orlix-tcti-kernel-tests:
orlix-tcti-kernel-tests: xcodeproj
	@/bin/bash -c 'set -e; \
		PATH="$$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin"; \
		timeout_marker="$(ORLIX_KUNIT_PRODUCT_BUILD_ROOT)/.orlix-tcti-xcodebuild-timeout"; \
		$(ORLIX_TCTI_XCODEBUILD_WATCHDOG_FUNCTIONS) \
		provisioning=(); \
		if [ "$(ORLIX_ALLOW_PROVISIONING_UPDATES)" = YES ]; then provisioning=(-allowProvisioningUpdates); fi; \
		ORLIX_TCTI_XCODEBUILD_WALL_TIMEOUT_SECONDS="$(ORLIX_TCTI_BUILD_FOR_TESTING_WALL_TIMEOUT_SECONDS)" run_xcodebuild "$$timeout_marker" "$(ORLIX_TCTI_XCODEBUILD)" \
			-project Orlix.xcodeproj \
			-scheme "OrlixKernel Conformance" \
			-configuration Debug \
			-destination "$(ORLIX_TCTI_TEST_DESTINATION)" \
			-derivedDataPath "$(ORLIX_TCTI_DERIVED_DATA_PATH)" \
			"$${provisioning[@]}" \
			ORLIX_PROFILE=development \
			ORLIX_KERNEL_KUNIT=1 \
			ORLIX_BUILD_ROOT="$(ORLIX_KUNIT_PRODUCT_BUILD_ROOT)" \
			ORLIX_OS_SKIP_ENVIRONMENT_RUNTIME_FIXTURES=YES \
			DEVELOPMENT_TEAM="$(ORLIX_DEVELOPMENT_TEAM)" \
			build-for-testing; \
		ORLIX_TCTI_XCODEBUILD_WALL_TIMEOUT_SECONDS="$(ORLIX_TCTI_TEST_WITHOUT_BUILDING_WALL_TIMEOUT_SECONDS)" run_xcodebuild "$$timeout_marker" "$(ORLIX_TCTI_XCODEBUILD)" \
			-project Orlix.xcodeproj \
			-scheme "OrlixKernel Conformance" \
			-configuration Debug \
			-destination "$(ORLIX_TCTI_TEST_DESTINATION)" \
			-derivedDataPath "$(ORLIX_TCTI_DERIVED_DATA_PATH)" \
			"$${provisioning[@]}" \
			-test-timeouts-enabled YES \
			-default-test-execution-time-allowance $(ORLIX_TCTI_XCTEST_TIMEOUT_SECONDS) \
			-maximum-test-execution-time-allowance $(ORLIX_TCTI_XCTEST_TIMEOUT_SECONDS) \
			-only-testing:$(ORLIX_TCTI_TEST_ONLY_TESTING) \
			DEVELOPMENT_TEAM="$(ORLIX_DEVELOPMENT_TEAM)" \
			test-without-building'

orlix-tcti-xcodebuild-watchdog-tests:
	@/bin/bash -c 'set -e; \
		PATH="$$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin"; \
		scratch=$$(mktemp -d /private/tmp/orlix-tcti-xcodebuild-watchdog.XXXXXX); export scratch; \
		trap "rm -rf \"$$scratch\"" EXIT; \
		$(ORLIX_TCTI_XCODEBUILD_WATCHDOG_FUNCTIONS) \
		if run_xcodebuild "$$scratch/normal" /bin/sh -c "exit 17"; then normal_status=0; else normal_status=$$?; fi; \
		if [ "$$normal_status" -ne 17 ]; then echo "normal status propagation failed: $$normal_status" >&2; exit 1; fi; \
		if ORLIX_TCTI_XCODEBUILD_WALL_TIMEOUT_SECONDS=1 run_xcodebuild "$$scratch/timeout" /bin/sh -c "trap '\''sleep 30 & descendant=\$$!; echo \$$descendant > \"\$$scratch/descendant\"; exit 0'\'' TERM; sleep 30 & child=\$$!; echo \$$child > \"\$$scratch/child\"; wait \$$child"; then timeout_status=0; else timeout_status=$$?; fi; \
		if [ "$$timeout_status" -ne 124 ]; then echo "timeout status propagation failed: $$timeout_status" >&2; exit 1; fi; \
	child=$$(<"$$scratch/child"); descendant=$$(<"$$scratch/descendant"); \
	if kill -0 "$$child" 2>/dev/null; then echo "owned child survived timeout: $$child" >&2; exit 1; fi; \
	if kill -0 "$$descendant" 2>/dev/null; then echo "reparented descendant survived timeout: $$descendant" >&2; exit 1; fi; \
	: > "$$scratch/marker-parent-is-file"; \
	if ORLIX_TCTI_XCODEBUILD_WALL_TIMEOUT_SECONDS=1 run_xcodebuild "$$scratch/marker-parent-is-file/timeout" /bin/sh -c "trap '\''sleep 30 & descendant=\$$!; echo \$$descendant > \"\$$scratch/blocked-descendant\"; exit 0'\'' TERM; sleep 30 & child=\$$!; echo \$$child > \"\$$scratch/blocked-child\"; wait \$$child"; then blocked_timeout_status=0; else blocked_timeout_status=$$?; fi; \
	if [ "$$blocked_timeout_status" -ne 124 ]; then echo "blocked marker timeout status propagation failed: $$blocked_timeout_status" >&2; exit 1; fi; \
	blocked_child=$$(<"$$scratch/blocked-child"); blocked_descendant=$$(<"$$scratch/blocked-descendant"); \
	if kill -0 "$$blocked_child" 2>/dev/null; then echo "owned child survived blocked marker timeout: $$blocked_child" >&2; exit 1; fi; \
	if kill -0 "$$blocked_descendant" 2>/dev/null; then echo "reparented descendant survived blocked marker timeout: $$blocked_descendant" >&2; exit 1; fi; \
		arguments="$$( ORLIX_GMAKE_CONTRACT_REEXEC=1 ORLIX_ROOT_GMAKE_REEXEC=1 "$${ORLIX_GMAKE:-/opt/homebrew/bin/gmake}" -f Makefile -n -o xcodeproj ORLIX_TCTI_BUILD_FOR_TESTING_WALL_TIMEOUT_SECONDS=101 ORLIX_TCTI_TEST_WITHOUT_BUILDING_WALL_TIMEOUT_SECONDS=202 ORLIX_TCTI_XCTEST_TIMEOUT_SECONDS=303 ORLIX_TCTI_DERIVED_DATA_PATH="$$scratch/DerivedData" ORLIX_TCTI_TEST_DESTINATION="platform=iOS Simulator,name=iPhone 17 Pro" orlix-tcti-kernel-tests )"; \
		count_occurrences() { local remaining="$$1" needle="$$2" count=0; while [[ "$$remaining" == *"$$needle"* ]]; do remaining=$${remaining#*"$$needle"}; count=$$((count + 1)); done; printf "%s" "$$count"; }; \
		assert_count() { local needle="$$1" expected="$$2" actual; actual=$$(count_occurrences "$$arguments" "$$needle"); if [ "$$actual" -ne "$$expected" ]; then echo "argument count failed needle=$$needle expected=$$expected actual=$$actual" >&2; exit 1; fi; }; \
		assert_count "run_xcodebuild \"\$$timeout_marker\" \"/usr/bin/xcodebuild\"" 2; \
		assert_count "ORLIX_TCTI_XCODEBUILD_WALL_TIMEOUT_SECONDS=\"101\"" 1; \
		assert_count "ORLIX_TCTI_XCODEBUILD_WALL_TIMEOUT_SECONDS=\"202\"" 1; \
		assert_count "-destination \"platform=iOS Simulator,name=iPhone 17 Pro\"" 2; \
		assert_count "-derivedDataPath \"$$scratch/DerivedData\"" 2; \
		assert_count "-test-timeouts-enabled YES" 1; \
		assert_count "-default-test-execution-time-allowance 303" 1; \
		assert_count "-maximum-test-execution-time-allowance 303" 1; \
		assert_count "-only-testing:OrlixKernelConformanceTests/OrlixKernelConformanceTests/testKselftestRootfsCompletesThroughOrlixOSTerminalSession" 1; \
		if [[ "$$arguments" == *"ADE0D3EB-6E89-41DD-9AB9-CA20F10609F3"* ]]; then echo "destination override regressed to the stale default simulator" >&2; exit 1; fi; \
		build_arguments=$${arguments%%ORLIX_TCTI_XCODEBUILD_WALL_TIMEOUT_SECONDS=\"202\"*}; \
		if [[ "$$build_arguments" == *"-test-timeouts-enabled"* || "$$build_arguments" == *"-only-testing:"* ]]; then echo "build-for-testing received XCTest-only arguments" >&2; exit 1; fi; \
	echo "ORLIX_TCTI_XCODEBUILD_WATCHDOG_TEST normal_status=$$normal_status timeout_status=$$timeout_status blocked_timeout_status=$$blocked_timeout_status child_gone=$$child descendant_gone=$$descendant blocked_child_gone=$$blocked_child blocked_descendant_gone=$$blocked_descendant invocations=2"'

ORLIX_MLIBC_TEST_ONLY_TESTING ?= OrlixMLibCConformanceTests/OrlixMLibCConformanceTests/testMLibCRootfsCompletesThroughOrlixOSTerminalSession

mlibc-tests: xcodeproj
	@PATH="$$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin" xcodebuild \
		-project Orlix.xcodeproj \
		-scheme "OrlixMLibC Conformance" \
		-configuration Debug \
		-destination '$(ORLIX_TEST_DESTINATION)' \
		ORLIX_PROFILE='$(PROFILE)' \
		ORLIX_OS_SKIP_ENVIRONMENT_RUNTIME_FIXTURES=YES \
		-only-testing:$(ORLIX_MLIBC_TEST_ONLY_TESTING) \
		test

coreutils-tests: xcodeproj
	@$(COREUTILS_MAKE) build PROFILE='$(PROFILE)' ORLIX_BUILD_ROOT='$(ORLIX_BUILD_ROOT)'
	@$(ORLIXOS_MAKE) coreutils-test-initramfs PROFILE='$(PROFILE)' ORLIX_BUILD_ROOT='$(ORLIX_BUILD_ROOT)'
	@PATH="$$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin" xcodebuild \
		-project Orlix.xcodeproj \
		-scheme "OrlixPackages Conformance" \
		-configuration Debug \
		-destination '$(ORLIX_TEST_DESTINATION)' \
		ORLIX_PROFILE='$(PROFILE)' \
		ORLIX_OS_SKIP_ENVIRONMENT_RUNTIME_FIXTURES=YES \
		test

hostadapter-tests: xcodeproj
	@PATH="$$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin" xcodebuild \
		-project Orlix.xcodeproj \
		-scheme "OrlixHostAdapter Tests" \
		-configuration Debug \
		-destination '$(ORLIX_TEST_DESTINATION)' \
		test

orlixos-xcframework: xcodeproj
	@set -euo pipefail; \
	mkdir -p "$(ORLIXOS_XCFRAMEWORK_ROOT)"; \
	rm -rf "$(ORLIXOS_DEVICE_ARCHIVE)" "$(ORLIXOS_SIMULATOR_ARCHIVE)" "$(ORLIXOS_XCFRAMEWORK)"; \
	xcodebuild archive \
		-project Orlix.xcodeproj \
		-scheme OrlixOS \
		-configuration Release \
		-destination 'generic/platform=iOS' \
		-derivedDataPath "$(ORLIXOS_XCFRAMEWORK_ROOT)/DerivedData/iphoneos" \
		-archivePath "$(ORLIXOS_DEVICE_ARCHIVE)" \
		SKIP_INSTALL=NO BUILD_LIBRARY_FOR_DISTRIBUTION=YES CODE_SIGNING_ALLOWED=NO; \
	xcodebuild archive \
		-project Orlix.xcodeproj \
		-scheme OrlixOS \
		-configuration Release \
		-destination 'generic/platform=iOS Simulator' \
		-derivedDataPath "$(ORLIXOS_XCFRAMEWORK_ROOT)/DerivedData/iphonesimulator" \
		-archivePath "$(ORLIXOS_SIMULATOR_ARCHIVE)" \
		SKIP_INSTALL=NO BUILD_LIBRARY_FOR_DISTRIBUTION=YES CODE_SIGNING_ALLOWED=NO; \
	xcodebuild -create-xcframework \
		-framework "$(ORLIXOS_DEVICE_ARCHIVE)/Products/Library/Frameworks/OrlixOS.framework" \
		-framework "$(ORLIXOS_SIMULATOR_ARCHIVE)/Products/Library/Frameworks/OrlixOS.framework" \
		-output "$(ORLIXOS_XCFRAMEWORK)"; \
	test -f "$(ORLIXOS_XCFRAMEWORK)/Info.plist"; \
	echo "built public OrlixOS SDK: $(ORLIXOS_XCFRAMEWORK)"

orlixos-tests: xcodeproj console-policy-tests terminal-mux-tests
	@PATH="$$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin" xcodebuild \
		-project Orlix.xcodeproj \
		-scheme "OrlixOS Tests" \
		-configuration Debug \
		-destination '$(ORLIX_TEST_DESTINATION)' \
		test

app-tests: xcodeproj
	@PATH="$$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin" xcodebuild \
		-project Orlix.xcodeproj \
		-scheme "Orlix Tests" \
		-configuration Debug \
		-destination '$(ORLIX_TEST_DESTINATION)' \
		test
	@PATH="$$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin" xcodebuild \
		-project Orlix.xcodeproj \
		-scheme "OrlixOSTestApp Tests" \
		-configuration Debug \
		-destination '$(ORLIX_TEST_DESTINATION)' \
		-only-testing:OrlixOSTestAppTests/ArchitectureInvariantTests \
		test

runtime-tests: xcodeproj
	@PATH="$$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin" xcodebuild \
		-project Orlix.xcodeproj \
		-scheme "OrlixOS Runtime Tests" \
		-configuration Debug \
		-destination '$(ORLIX_TEST_DESTINATION)' \
		ORLIX_PROFILE='$(PROFILE)' \
		test

docs-index:
	@python3 .agents/skills/orlix-docs-lint/scripts/build_index.py docs

docs-check:
	@python3 .agents/skills/orlix-docs-lint/scripts/build_index.py --check docs
	@python3 .agents/skills/orlix-docs-lint/scripts/wiki_link_check.py docs
	@python3 .agents/skills/orlix-docs-lint/scripts/legacy_path_check.py

agent-rules-generate:
	@rulesync generate --targets copilot,cursor,claudecode,codexcli --features rules

agent-rules-check:
	@rulesync generate --targets copilot,cursor,claudecode,codexcli --features rules --check

agent-harness-check: docs-check
	@.agents/tests/harness-check all

agent-hooks-generate:
	@rulesync generate --targets codexcli --features hooks

agent-hooks-check:
	@.agents/tests/harness-check hooks

agent-skills-check:
	@.agents/tests/harness-check skills

agent-subagents-check:
	@.agents/tests/harness-check subagents

agent-mcp-check:
	@.agents/tests/harness-check mcp

agent-status:
	@test "$(AREA)" = "orlix-tcti" || { echo "AREA=orlix-tcti required" >&2; exit 2; }
	@.agents/skills/orlix-tcti-next-step/scripts/status

agent-next:
	@test "$(AREA)" = "orlix-tcti" || { echo "AREA=orlix-tcti required" >&2; exit 2; }
	@.agents/skills/orlix-tcti-next-step/scripts/next

agent-task-envelope-check:
	@test "$(AREA)" = "orlix-tcti" || { echo "AREA=orlix-tcti required" >&2; exit 2; }
	@.agents/skills/orlix-tcti-next-step/scripts/task-envelope-check

beta-archive: beta-resolve-build-number
ifeq ($(ORLIX_BAZEL_AUTHORITY),1)
	@$(MAKE) __bazel-orlix-archive
else
	@set -euo pipefail; \
	xcodegen generate --spec project.yml; \
	build_number="$$(tr -d '[:space:]' < "$(ORLIX_BETA_BUILD_NUMBER_FILE)")"; \
	[ -n "$(ORLIX_DEVELOPMENT_TEAM)" ] || { echo "ORLIX_DEVELOPMENT_TEAM is required to archive for TestFlight" >&2; exit 1; }; \
	archive_settings=(DEVELOPMENT_TEAM="$(ORLIX_DEVELOPMENT_TEAM)" CODE_SIGN_STYLE="$(ORLIX_CODE_SIGN_STYLE)" CURRENT_PROJECT_VERSION="$$build_number"); \
	case "$(ORLIX_ANALYTICS_ENABLED)" in YES|NO) ;; *) echo "ORLIX_ANALYTICS_ENABLED must be YES or NO" >&2; exit 2;; esac; \
	case "$(ORLIX_OBSERVABILITY_ENABLED)" in YES|NO) ;; *) echo "ORLIX_OBSERVABILITY_ENABLED must be YES or NO" >&2; exit 2;; esac; \
	archive_settings+=(ORLIX_ANALYTICS_ENABLED="$(ORLIX_ANALYTICS_ENABLED)" ORLIX_OBSERVABILITY_ENABLED="$(ORLIX_OBSERVABILITY_ENABLED)"); \
	if [ "$(ORLIX_ANALYTICS_ENABLED)" = YES ]; then \
		[ -n "$(ORLIX_OPENPANEL_CLIENT_ID)" ] || { echo "ORLIX_OPENPANEL_CLIENT_ID is required when analytics is enabled" >&2; exit 1; }; \
		archive_settings+=(ORLIX_OPENPANEL_CLIENT_ID="$(ORLIX_OPENPANEL_CLIENT_ID)"); \
	fi; \
	if [ "$(ORLIX_OBSERVABILITY_ENABLED)" = YES ]; then \
		[ -n "$(ORLIX_SIGNOZ_INGESTION_KEY)" ] || { echo "ORLIX_SIGNOZ_INGESTION_KEY is required when observability is enabled" >&2; exit 1; }; \
		archive_settings+=(ORLIX_SIGNOZ_INGESTION_KEY="$(ORLIX_SIGNOZ_INGESTION_KEY)"); \
	fi; \
	xcodebuild_signing_flags=(); \
	if [ "$(ORLIX_ALLOW_PROVISIONING_UPDATES)" = YES ]; then xcodebuild_signing_flags+=(-allowProvisioningUpdates); fi; \
	if [ -n "$(ORLIX_ASC_API_KEY_PATH)$(ORLIX_ASC_API_KEY_ID)$(ORLIX_ASC_API_ISSUER_ID)" ]; then \
		[ -n "$(ORLIX_ASC_API_KEY_PATH)" ] || { echo "ORLIX_ASC_API_KEY_PATH is required when App Store Connect API key signing is used" >&2; exit 1; }; \
		[ -n "$(ORLIX_ASC_API_KEY_ID)" ] || { echo "ORLIX_ASC_API_KEY_ID is required when App Store Connect API key signing is used" >&2; exit 1; }; \
		[ -n "$(ORLIX_ASC_API_ISSUER_ID)" ] || { echo "ORLIX_ASC_API_ISSUER_ID is required when App Store Connect API key signing is used" >&2; exit 1; }; \
		xcodebuild_signing_flags+=(-authenticationKeyPath "$(ORLIX_ASC_API_KEY_PATH)" -authenticationKeyID "$(ORLIX_ASC_API_KEY_ID)" -authenticationKeyIssuerID "$(ORLIX_ASC_API_ISSUER_ID)"); \
	fi; \
	if [ -n "$(ORLIX_CODE_SIGN_IDENTITY)" ]; then archive_settings+=(CODE_SIGN_IDENTITY="$(ORLIX_CODE_SIGN_IDENTITY)"); fi; \
	if [ -n "$(ORLIX_PROVISIONING_PROFILE_SPECIFIER)" ]; then archive_settings+=(PROVISIONING_PROFILE_SPECIFIER="$(ORLIX_PROVISIONING_PROFILE_SPECIFIER)"); fi; \
	$(MAKE) -f OrlixMLibC/Makefile build PROFILE=release; \
	$(MAKE) -f OrlixCoreUtils/Makefile build PROFILE=release; \
	$(MAKE) -f OrlixOS/Makefile rootfs PROFILE=release; \
	$(MAKE) -f OrlixKernel/Makefile __kernel-archive PROFILE=release ORLIX_KERNEL_ARCHIVE_PLATFORMS=iphoneos ORLIX_KERNEL_BASE_ROOT_TREE_INPUT="$(ORLIXOS_BASE_ROOT_TREE)"; \
	$(MAKE) -f OrlixOS/Makefile kernel-payload PROFILE=release; \
	mkdir -p "$(ORLIX_BETA_ARCHIVE_DIR)"; \
	"$(ORLIX_XCODEBUILD_ARCHIVE)" \
		-project Orlix.xcodeproj \
		-scheme "$(ORLIX_BETA_SCHEME)" \
		-configuration Release \
		-destination 'generic/platform=iOS' \
		-archivePath "$(ORLIX_BETA_ARCHIVE_PATH)" \
		"$${xcodebuild_signing_flags[@]}" \
		"$${archive_settings[@]}" \
		archive
endif

beta-validate-archive:
	@set -euo pipefail; \
	app="$(ORLIX_BETA_ARCHIVE_PATH)/Products/Applications/Orlix.app"; \
	test -d "$$app" || { echo "missing archived app: $$app" >&2; exit 1; }; \
	test -d "$$app/Frameworks/OrlixOS.framework" || { echo "missing OrlixOS.framework in archive" >&2; exit 1; }; \
	test -f "$$app/Frameworks/OrlixOS.framework/OrlixOSManifest.plist" || { echo "missing direct OrlixOS manifest in archive" >&2; exit 1; }; \
	test -d "$$app/Frameworks/OrlixOS.framework/rootfs" || { echo "missing direct OrlixOS rootfs resources in archive" >&2; exit 1; }; \
	for private_framework in OrlixKernel OrlixMLibC OrlixCoreUtils; do \
		test ! -e "$$app/Frameworks/$$private_framework.framework" || { echo "private framework must be statically incorporated into OrlixOS: $$private_framework" >&2; exit 1; }; \
	done; \
	! find "$$app" -maxdepth 5 -name 'OrlixOSPayload.*' -print -quit | grep -q . || { echo "retired OrlixOS payload bundle remains in archive" >&2; exit 1; }; \
	$(MAKE) --no-print-directory __exported-app-check ORLIX_EXPORTED_APP="$$app"; \
	printf '%s\n' "validated beta archive contents: $(ORLIX_BETA_ARCHIVE_PATH)"

beta-export-options:
	@set -euo pipefail; \
	[ -n "$(ORLIX_BETA_EXPORT_OPTIONS_PLIST)" ] || { echo "ORLIX_BETA_EXPORT_OPTIONS_PLIST is required" >&2; exit 1; }; \
	[ -n "$(ORLIX_DEVELOPMENT_TEAM)" ] || { echo "ORLIX_DEVELOPMENT_TEAM is required" >&2; exit 1; }; \
	mkdir -p "$$(dirname "$(ORLIX_BETA_EXPORT_OPTIONS_PLIST)")"; \
	plutil -create xml1 "$(ORLIX_BETA_EXPORT_OPTIONS_PLIST)"; \
	/usr/libexec/PlistBuddy -c 'Add :method string app-store-connect' -c 'Add :signingStyle string automatic' -c 'Add :teamID string $(ORLIX_DEVELOPMENT_TEAM)' -c 'Add :uploadSymbols bool true' "$(ORLIX_BETA_EXPORT_OPTIONS_PLIST)"; \
	echo "wrote runtime App Store export options: $(ORLIX_BETA_EXPORT_OPTIONS_PLIST)"

beta-export-archive: beta-validate-archive
	@set -euo pipefail; \
	[ -n "$(ORLIX_BETA_EXPORT_OPTIONS_PLIST)" ] || { echo "ORLIX_BETA_EXPORT_OPTIONS_PLIST is required to export the archive" >&2; exit 1; }; \
	test -f "$(ORLIX_BETA_EXPORT_OPTIONS_PLIST)" || { echo "missing export options plist: $(ORLIX_BETA_EXPORT_OPTIONS_PLIST)" >&2; exit 1; }; \
	xcodebuild_signing_flags=(); \
	if [ "$(ORLIX_ALLOW_PROVISIONING_UPDATES)" = YES ]; then xcodebuild_signing_flags+=(-allowProvisioningUpdates); fi; \
	if [ -n "$(ORLIX_ASC_API_KEY_PATH)$(ORLIX_ASC_API_KEY_ID)$(ORLIX_ASC_API_ISSUER_ID)" ]; then \
		[ -n "$(ORLIX_ASC_API_KEY_PATH)" ] || { echo "ORLIX_ASC_API_KEY_PATH is required when App Store Connect API key signing is used" >&2; exit 1; }; \
		[ -n "$(ORLIX_ASC_API_KEY_ID)" ] || { echo "ORLIX_ASC_API_KEY_ID is required when App Store Connect API key signing is used" >&2; exit 1; }; \
		[ -n "$(ORLIX_ASC_API_ISSUER_ID)" ] || { echo "ORLIX_ASC_API_ISSUER_ID is required when App Store Connect API key signing is used" >&2; exit 1; }; \
		xcodebuild_signing_flags+=(-authenticationKeyPath "$(ORLIX_ASC_API_KEY_PATH)" -authenticationKeyID "$(ORLIX_ASC_API_KEY_ID)" -authenticationKeyIssuerID "$(ORLIX_ASC_API_ISSUER_ID)"); \
	fi; \
	mkdir -p "$(ORLIX_BETA_EXPORT_DIR)"; \
	"$(ORLIX_XCODEBUILD_EXPORT)" \
	-exportArchive \
	-archivePath "$(ORLIX_BETA_ARCHIVE_PATH)" \
	-exportOptionsPlist "$(ORLIX_BETA_EXPORT_OPTIONS_PLIST)" \
	$${xcodebuild_signing_flags[@]+"$${xcodebuild_signing_flags[@]}"} \
		-exportPath "$(ORLIX_BETA_EXPORT_DIR)"

beta-validate-export:
	@set -euo pipefail; \
	test -f "$(ORLIX_BETA_IPA_PATH)" || { echo "missing exported IPA: $(ORLIX_BETA_IPA_PATH)" >&2; exit 1; }; \
	tmp="$$(mktemp -d "$${TMPDIR:-/tmp}/orlix-beta-export.XXXXXX")"; \
	trap 'rm -rf "$$tmp"' EXIT; \
	/usr/bin/ditto -x -k "$(ORLIX_BETA_IPA_PATH)" "$$tmp"; \
	app="$$(find "$$tmp/Payload" -maxdepth 1 -type d -name '*.app' -print -quit)"; \
	[ -n "$$app" ] || { echo "exported IPA does not contain an app" >&2; exit 1; }; \
	$(MAKE) --no-print-directory __exported-app-check ORLIX_EXPORTED_APP="$$app" ORLIX_REQUIRE_PUBLIC_DISTRIBUTION="$(ORLIX_REQUIRE_PUBLIC_DISTRIBUTION)"

beta-upload-prerequisites: __release-tag-check
	@$(MAKE) --no-print-directory beta-validate-export

beta-upload:
	@set -euo pipefail; \
	test -s "$(ORLIX_FASTLANE_API_KEY_PATH)" || { echo "missing fastlane App Store Connect API key JSON: $(ORLIX_FASTLANE_API_KEY_PATH)" >&2; exit 1; }; \
	test -f "$(ORLIX_BETA_IPA_PATH)" || { echo "missing exported IPA: $(ORLIX_BETA_IPA_PATH)" >&2; exit 1; }; \
	$(ORLIX_FASTLANE) pilot upload \
		--api_key_path "$(ORLIX_FASTLANE_API_KEY_PATH)" \
		--app_identifier "$(ORLIX_APP_BUNDLE_ID)" \
		--ipa "$(ORLIX_BETA_IPA_PATH)" \
		--uses_non_exempt_encryption false

beta-distribute:
	@set -euo pipefail; \
	test -s "$(ORLIX_FASTLANE_API_KEY_PATH)" || { echo "missing fastlane App Store Connect API key JSON: $(ORLIX_FASTLANE_API_KEY_PATH)" >&2; exit 1; }; \
	test -f "$(ORLIX_RELEASE_IDENTITY_PATH)" || { echo "missing release identity: $(ORLIX_RELEASE_IDENTITY_PATH)" >&2; exit 1; }; \
	test -f "$(ORLIX_BETA_BUILD_NUMBER_FILE)" || { echo "missing beta build number: $(ORLIX_BETA_BUILD_NUMBER_FILE)" >&2; exit 1; }; \
	[ -n "$(ORLIX_TESTFLIGHT_INTERNAL_GROUP)" ] || { echo "ORLIX_TESTFLIGHT_INTERNAL_GROUP is required" >&2; exit 1; }; \
	ORLIX_MARKETING_VERSION="$$(jq -r '.marketing_version' "$(ORLIX_RELEASE_IDENTITY_PATH)")" \
	ORLIX_BUILD_NUMBER="$$(tr -d '[:space:]' < "$(ORLIX_BETA_BUILD_NUMBER_FILE)")" \
	ORLIX_TESTFLIGHT_INTERNAL_GROUP="$(ORLIX_TESTFLIGHT_INTERNAL_GROUP)" \
	ORLIX_FASTLANE_API_KEY_PATH="$(ORLIX_FASTLANE_API_KEY_PATH)" \
	$(ORLIX_FASTLANE) ios distribute_internal

beta-release-report: __release-report-write

app-store-release-report-check: __release-tag-check __release-report-check __release-public-approval-check __release-metadata-check
	@echo "pass: exact TestFlight beta is eligible for App Review submission"

app-store-promote: app-store-release-report-check
	@set -euo pipefail; \
	test -s "$(ORLIX_FASTLANE_API_KEY_PATH)" || { echo "missing fastlane App Store Connect API key JSON: $(ORLIX_FASTLANE_API_KEY_PATH)" >&2; exit 1; }; \
	ORLIX_MARKETING_VERSION="$$(jq -r '.marketing_version' "$(ORLIX_RELEASE_REPORT_SELECTION)")" \
	ORLIX_BUILD_NUMBER="$$(jq -r '.build_number' "$(ORLIX_RELEASE_REPORT_SELECTION)")" \
	ORLIX_FASTLANE_API_KEY_PATH="$(ORLIX_FASTLANE_API_KEY_PATH)" \
	ORLIX_STORE_METADATA_PATH="$(ORLIX_STORE_METADATA_PATH)" \
	ORLIX_STORE_SCREENSHOTS_PATH="$(ORLIX_STORE_SCREENSHOTS_PATH)" \
	$(ORLIX_FASTLANE) ios promote_to_review

release-workflow-check: __release-workflow-tests

xcodeproj:
ifeq ($(ORLIX_BAZEL_AUTHORITY),1)
	@$(MAKE) __bazel-feasibility-xcodeproj
else
	@$(KERNEL_MAKE) xcodeproj
endif

build: __build-$(type)

__build-product: product-build-version-check
ifeq ($(ORLIX_BAZEL_AUTHORITY),1)
	@$(MAKE) __bazel-orlix-app
else
	@$(MLIBC_MAKE) build
	@$(COREUTILS_MAKE) build PROFILE="$(PROFILE)"
	@$(ORLIXOS_MAKE) rootfs PROFILE="$(PROFILE)"
	@$(KERNEL_MAKE) build PROFILE="$(PROFILE)" ORLIX_KERNEL_BASE_ROOT_TREE_INPUT="$(ORLIXOS_BASE_ROOT_TREE)"
	@$(HOSTADAPTER_MAKE) build
	@$(APP_MAKE) build
endif

__build-vendor:
	@$(APP_MAKE) build type=vendor vendor="$(vendor)"

rebuild: clean
ifeq ($(ORLIX_BAZEL_AUTHORITY),1)
	@$(MAKE) __bazel-orlix-app
else
	@$(MAKE) build
endif

prepare: __prepare-$(type)

__prepare-product:
	@$(KERNEL_MAKE) prepare

__prepare-tcti-isa:
	@$(KERNEL_MAKE) prepare type=tcti-isa

scripts dtbs kunit kselftest kselftest-install:
	@$(KERNEL_MAKE) $@

test:
ifeq ($(ORLIX_BAZEL_AUTHORITY),1)
	@$(MAKE) __bazel-matrix-check
else
	@$(KERNEL_MAKE) test
endif

headers_install:
ifeq ($(ORLIX_BAZEL_AUTHORITY),1)
	@$(MAKE) __bazel-kernel-uapi
else
	@$(MLIBC_MAKE) headers_install
endif

run:
	@$(APP_MAKE) run PROFILE="$(PROFILE)" ORLIX_KERNEL_BASE_ROOT_TREE_INPUT="$(ORLIXOS_BASE_ROOT_TREE)"

clean:
	@set -euo pipefail; \
	if [ -L '$(ORLIX_BUILD_ROOT)' ]; then echo "refusing to clean symlinked Build directory" >&2; exit 1; fi; \
	rm -rf '$(ORLIX_BUILD_ROOT)'
	@$(HOSTADAPTER_MAKE) clean
	@$(APP_MAKE) clean

mrproper:
	@$(KERNEL_MAKE) mrproper
	@$(HOSTADAPTER_MAKE) mrproper
	@$(MLIBC_MAKE) mrproper
	@$(COREUTILS_MAKE) mrproper
	@$(ORLIXOS_MAKE) mrproper
	@$(APP_MAKE) mrproper
terminal-mux-tests:
	@mkdir -p '$(ORLIX_BUILD_ROOT)/Tests/terminal-mux'
	@$(CC) -std=c17 -Wall -Wextra -Werror \
		OrlixOS/Sources/init/terminal_mux.c \
		OrlixOS/Tests/TerminalMux/terminal_mux_tests.c \
		-o '$(ORLIX_BUILD_ROOT)/Tests/terminal-mux/terminal_mux_tests'
	@'$(ORLIX_BUILD_ROOT)/Tests/terminal-mux/terminal_mux_tests'

console-policy-tests:
	@mkdir -p '$(ORLIX_BUILD_ROOT)/Tests/console-policy'
	@$(CC) -std=c17 -Wall -Wextra -Werror \
		OrlixOS/Sources/init/console_policy.c \
		OrlixOS/Tests/ConsolePolicy/console_policy_tests.c \
		-o '$(ORLIX_BUILD_ROOT)/Tests/console-policy/console_policy_tests'
	@'$(ORLIX_BUILD_ROOT)/Tests/console-policy/console_policy_tests'

endif
