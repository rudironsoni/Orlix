SHELL := /bin/bash
.DEFAULT_GOAL := all

KERNEL_MAKE := $(MAKE) -f OrlixKernel/Makefile
HOSTADAPTER_MAKE := $(MAKE) -f OrlixHostAdapter/Makefile
MLIBC_MAKE := $(MAKE) -f OrlixMLibC/Makefile
ORLIXOS_MAKE := $(MAKE) -f OrlixOS/Makefile
APP_MAKE := $(MAKE) -f Orlix/Makefile
PROFILE ?= release
ORLIX_EXTERNAL_SSD_ROOT ?= $(shell external-ssd-root 2>/dev/null)
ORLIX_BUILD_ROOT ?= $(if $(ORLIX_EXTERNAL_SSD_ROOT),$(ORLIX_EXTERNAL_SSD_ROOT)/Xcode/OrlixSystem/Build,$(CURDIR)/Build)
ORLIXOS_BASE_ROOT_TREE := $(ORLIX_BUILD_ROOT)/OrlixOS/rootfs/$(PROFILE)/base-tree
ORLIX_BETA_SCHEME ?= Orlix
ORLIX_BETA_ARCHIVE_DIR ?= $(ORLIX_BUILD_ROOT)/Release
ORLIX_BETA_ARCHIVE_PATH ?= $(ORLIX_BETA_ARCHIVE_DIR)/Orlix.xcarchive
ORLIX_BETA_EXPORT_DIR ?= $(ORLIX_BETA_ARCHIVE_DIR)/Export
ORLIX_BETA_EXPORT_OPTIONS_PLIST ?=
ORLIX_DEVELOPMENT_TEAM ?=
ORLIX_CODE_SIGN_STYLE ?= Automatic
ORLIX_CODE_SIGN_IDENTITY ?=
ORLIX_PROVISIONING_PROFILE_SPECIFIER ?=
ORLIX_ALLOW_PROVISIONING_UPDATES ?= YES
ORLIX_BETA_BUMP_BUILD_NUMBER ?= YES
ORLIX_ASC_API_KEY_PATH ?=
ORLIX_ASC_API_KEY_ID ?=
ORLIX_ASC_API_ISSUER_ID ?=
ORLIX_FASTLANE_API_KEY_PATH ?= $(HOME)/.config/fastlane/appstore_api_key.json
ORLIX_BETA_IPA_PATH ?= $(ORLIX_BETA_EXPORT_DIR)/Orlix.ipa
ORLIX_XCODE_ROOT ?= $(ORLIX_EXTERNAL_SSD_ROOT)/Xcode
ORLIX_XCODEBUILD_ARCHIVE ?= /usr/bin/xcodebuild
ORLIX_XCODEBUILD_EXPORT ?= /usr/bin/xcodebuild
ORLIX_BETA_SIMULATOR_ID ?= 4C88CA42-EA50-463F-B989-7B0560075A9B
ORLIX_BETA_SIMULATOR_DESTINATION ?= platform=iOS Simulator,id=$(ORLIX_BETA_SIMULATOR_ID)
ORLIX_APP_BUNDLE_ID ?= com.rudironsoni.Orlix
ORLIX_APP_LEGACY_BUNDLE_IDS ?= com.rudironsoni.OrlixTerminal org.orlix.OrlixTerminal
.PHONY: all help setup-env check-build-tools beta-prerequisites beta-signing-diagnostics beta-bump-build-number beta-install-simulator beta-simulator-gate runtime-validation beta-archive beta-validate-archive beta-export-archive beta-upload build prepare scripts dtbs headers_install kunit kselftest kselftest-install test xcodeproj run clean mrproper

all: build

help:
	@$(KERNEL_MAKE) help
	@printf '%s\n' ''
	@printf '%s\n' 'Project Makefiles:'
	@printf '%s\n' '  OrlixKernel/Makefile'
	@printf '%s\n' '  OrlixHostAdapter/Makefile'
	@printf '%s\n' '  OrlixMLibC/Makefile'
	@printf '%s\n' '  OrlixOS/Makefile'
	@printf '%s\n' '  Orlix/Makefile'
	@printf '%s\n' ''
	@printf '%s\n' 'Beta targets:'
	@printf '%s\n' '  beta-prerequisites  verify local TestFlight build prerequisites'
	@printf '%s\n' '  beta-signing-diagnostics check local Apple signing/keychain state'
	@printf '%s\n' '  beta-install-simulator build, fresh-install, and launch Release Orlix'
	@printf '%s\n' '  beta-simulator-gate run focused simulator gate for first beta'
	@printf '%s\n' '  beta-archive        generate Xcode project and archive Orlix Release'
	@printf '%s\n' '  beta-validate-archive inspect required app/framework/payload archive contents'
	@printf '%s\n' '  beta-export-archive export archived Orlix for upload'

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

beta-prerequisites: check-build-tools
	@set -euo pipefail; \
	command -v xcodegen >/dev/null 2>&1 || { echo "xcodegen is required; run: brew bundle --file Brewfile" >&2; exit 1; }; \
	command -v xcodebuild >/dev/null 2>&1 || { echo "xcodebuild is required" >&2; exit 1; }; \
	test -f project.yml || { echo "missing XcodeGen source: project.yml" >&2; exit 1; }

beta-bump-build-number: beta-prerequisites
	@set -euo pipefail; \
	if [ "$(ORLIX_BETA_BUMP_BUILD_NUMBER)" != YES ]; then \
		printf '%s\n' "skipping TestFlight build number bump (ORLIX_BETA_BUMP_BUILD_NUMBER=$(ORLIX_BETA_BUMP_BUILD_NUMBER))"; \
		exit 0; \
	fi; \
	current="$$(awk -F': *' '/^[[:space:]]*CURRENT_PROJECT_VERSION:/ { gsub(/"/, "", $$2); print $$2; exit }' project.yml)"; \
	[[ "$$current" =~ ^[0-9]+$$ ]] || { echo "CURRENT_PROJECT_VERSION must be an integer in project.yml, got: $$current" >&2; exit 1; }; \
	marketing="$$(awk -F': *' '/^[[:space:]]*MARKETING_VERSION:/ { gsub(/"/, "", $$2); print $$2; exit }' project.yml)"; \
	latest=""; \
	if [ -s "$(ORLIX_FASTLANE_API_KEY_PATH)" ] && command -v fastlane >/dev/null 2>&1; then \
		latest="$$(fastlane run latest_testflight_build_number api_key_path:"$(ORLIX_FASTLANE_API_KEY_PATH)" app_identifier:"$(ORLIX_APP_BUNDLE_ID)" version:"$$marketing" initial_build_number:0 | awk '/Result:/ { print $$NF }' | tail -n 1)"; \
	fi; \
	if [[ "$$latest" =~ ^[0-9]+$$ ]] && [ "$$latest" -ge "$$current" ]; then \
		next="$$((latest + 1))"; \
	else \
		next="$$((current + 1))"; \
	fi; \
	perl -0pi -e 's/^([[:space:]]*CURRENT_PROJECT_VERSION:[[:space:]]*)[0-9]+([[:space:]]*)$$/$${1}'"$$next"'$${2}/m or die "CURRENT_PROJECT_VERSION not found\n"' project.yml; \
	printf '%s\n' "bumped CURRENT_PROJECT_VERSION $$current -> $$next"

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
	for bundle_id in $(ORLIX_APP_LEGACY_BUNDLE_IDS); do \
		xcrun simctl terminate "$(ORLIX_BETA_SIMULATOR_ID)" "$$bundle_id" >/dev/null 2>&1 || true; \
		xcrun simctl uninstall "$(ORLIX_BETA_SIMULATOR_ID)" "$$bundle_id" >/dev/null 2>&1 || true; \
	done; \
	xcrun simctl terminate "$(ORLIX_BETA_SIMULATOR_ID)" "$(ORLIX_APP_BUNDLE_ID)" >/dev/null 2>&1 || true; \
	xcrun simctl uninstall "$(ORLIX_BETA_SIMULATOR_ID)" "$(ORLIX_APP_BUNDLE_ID)" >/dev/null 2>&1 || true; \
	xcrun simctl install "$(ORLIX_BETA_SIMULATOR_ID)" "$$app"; \
	installed_app="$$(xcrun simctl get_app_container "$(ORLIX_BETA_SIMULATOR_ID)" "$(ORLIX_APP_BUNDLE_ID)" app)"; \
	payload_info="$$installed_app/Frameworks/OrlixOS.framework/OrlixOSPayload.bundle/Info.plist"; \
	test -f "$$payload_info" || { echo "missing installed OrlixOS payload metadata: $$payload_info" >&2; exit 1; }; \
	plutil -extract OrlixSelectedRootMode raw -o - "$$payload_info" | grep -qx 'direct'; \
	plutil -extract OrlixKernelCommandLine raw -o - "$$payload_info" | grep -q 'root=/dev/vda'; \
	xcrun simctl launch "$(ORLIX_BETA_SIMULATOR_ID)" "$(ORLIX_APP_BUNDLE_ID)"

beta-simulator-gate: beta-prerequisites
	@set -euo pipefail; \
	xcodegen generate --spec project.yml; \
	xcodebuild \
		-project Orlix.xcodeproj \
		-scheme "OrlixOS Tests" \
		-configuration Debug \
		-destination '$(ORLIX_BETA_SIMULATOR_DESTINATION)' \
		-only-testing:OrlixOSTests/OrlixOSSessionTests/testPayloadBundleIsResolvedFromOrlixOSTargetMetadata \
		-only-testing:OrlixOSTests/OrlixOSSessionTests/testRootImageDescriptorsComeFromOrlixOSTargetMetadata \
		test; \
	xcodebuild \
		-project Orlix.xcodeproj \
		-scheme "OrlixRuntime Tests" \
		-configuration Debug \
		-destination '$(ORLIX_BETA_SIMULATOR_DESTINATION)' \
		-only-testing:OrlixRuntimeTests/testLinuxPTYCarriesInteractiveShellInputAndOutput \
		test; \
	xcodebuild \
		-project Orlix.xcodeproj \
		-scheme "OrlixRuntime Tests" \
		-configuration Debug \
		-destination '$(ORLIX_BETA_SIMULATOR_DESTINATION)' \
		-only-testing:OrlixRuntimeTests/OrlixEnvironmentRootRuntimeTests/testOCIDerivedMaterializedRootBindsDescriptorExecutionDefaults \
		test

runtime-validation: beta-prerequisites
	@tools/runtime/orlix-runtime-validation.sh

beta-archive: beta-bump-build-number
	@set -euo pipefail; \
	xcodegen generate --spec project.yml; \
	[ -n "$(ORLIX_DEVELOPMENT_TEAM)" ] || { echo "ORLIX_DEVELOPMENT_TEAM is required to archive for TestFlight" >&2; exit 1; }; \
	archive_settings=(DEVELOPMENT_TEAM="$(ORLIX_DEVELOPMENT_TEAM)" CODE_SIGN_STYLE="$(ORLIX_CODE_SIGN_STYLE)"); \
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
	$(MAKE) -f OrlixOS/Makefile rootfs PROFILE=release; \
	$(MAKE) -f OrlixKernel/Makefile __kernel-archive PROFILE=release ORLIX_KERNEL_ARCHIVE_PLATFORMS=iphoneos ORLIX_KERNEL_BASE_ROOT_TREE_INPUT="$(ORLIXOS_BASE_ROOT_TREE)"; \
	$(MAKE) -f OrlixOS/Makefile kernel-payload PROFILE=release; \
	mkdir -p "$(ORLIX_BETA_ARCHIVE_DIR)"; \
	TMPDIR="$(ORLIX_XCODE_ROOT)/tmp/" "$(ORLIX_XCODEBUILD_ARCHIVE)" \
		-derivedDataPath "$(ORLIX_XCODE_ROOT)/DerivedData" \
		-clonedSourcePackagesDirPath "$(ORLIX_XCODE_ROOT)/PackageCache" \
		-project Orlix.xcodeproj \
		-scheme "$(ORLIX_BETA_SCHEME)" \
		-configuration Release \
		-destination 'generic/platform=iOS' \
		-archivePath "$(ORLIX_BETA_ARCHIVE_PATH)" \
		"$${xcodebuild_signing_flags[@]}" \
		"$${archive_settings[@]}" \
		archive

beta-validate-archive:
	@set -euo pipefail; \
	app="$(ORLIX_BETA_ARCHIVE_PATH)/Products/Applications/Orlix.app"; \
	test -d "$$app" || { echo "missing archived app: $$app" >&2; exit 1; }; \
	test -d "$$app/Frameworks/OrlixOS.framework" || { echo "missing OrlixOS.framework in archive" >&2; exit 1; }; \
	test -d "$$app/Frameworks/OrlixKernel.framework" || { echo "missing OrlixKernel.framework in archive" >&2; exit 1; }; \
	find "$$app" -maxdepth 4 -name 'OrlixOSPayload.*' -print -quit | grep -q . || { echo "missing OrlixOS payload bundle in archive" >&2; exit 1; }; \
	printf '%s\n' "validated beta archive contents: $(ORLIX_BETA_ARCHIVE_PATH)"

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

beta-upload:
	@set -euo pipefail; \
	command -v fastlane >/dev/null 2>&1 || { echo "fastlane required to upload TestFlight build" >&2; exit 1; }; \
	test -s "$(ORLIX_FASTLANE_API_KEY_PATH)" || { echo "missing fastlane App Store Connect API key JSON: $(ORLIX_FASTLANE_API_KEY_PATH)" >&2; exit 1; }; \
	test -f "$(ORLIX_BETA_IPA_PATH)" || { echo "missing exported IPA: $(ORLIX_BETA_IPA_PATH)" >&2; exit 1; }; \
	fastlane pilot upload \
		--api_key_path "$(ORLIX_FASTLANE_API_KEY_PATH)" \
		--app_identifier "$(ORLIX_APP_BUNDLE_ID)" \
		--ipa "$(ORLIX_BETA_IPA_PATH)" \
		--uses_non_exempt_encryption false \
		--skip_waiting_for_build_processing true

xcodeproj:
	@$(KERNEL_MAKE) xcodeproj

build:
	@$(MAKE) clean
	@$(MLIBC_MAKE) build
	@$(ORLIXOS_MAKE) rootfs PROFILE="$(PROFILE)"
	@$(KERNEL_MAKE) build PROFILE="$(PROFILE)" ORLIX_KERNEL_BASE_ROOT_TREE_INPUT="$(ORLIXOS_BASE_ROOT_TREE)"
	@$(HOSTADAPTER_MAKE) build
	@$(APP_MAKE) build

prepare scripts dtbs kunit kselftest kselftest-install test:
	@$(KERNEL_MAKE) $@

headers_install:
	@$(MLIBC_MAKE) headers_install

run:
	@$(APP_MAKE) run PROFILE="$(PROFILE)" ORLIX_KERNEL_BASE_ROOT_TREE_INPUT="$(ORLIXOS_BASE_ROOT_TREE)"

clean:
	@set -euo pipefail; \
	if [ -L Build ]; then echo "refusing to clean symlinked Build directory" >&2; exit 1; fi; \
	rm -rf Build
	@$(HOSTADAPTER_MAKE) clean
	@$(APP_MAKE) clean

mrproper:
	@$(KERNEL_MAKE) mrproper
	@$(HOSTADAPTER_MAKE) mrproper
	@$(MLIBC_MAKE) mrproper
	@$(ORLIXOS_MAKE) mrproper
	@$(APP_MAKE) mrproper
