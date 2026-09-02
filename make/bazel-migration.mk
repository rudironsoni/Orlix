ORLIX_BAZEL_CACHE_ROOT ?= $(HOME)/Library/Caches/Orlix/Bazel
ORLIX_BAZEL_VERSION ?= 9.2.0
ORLIX_XCODE_VERSION ?= 26.6
ORLIX_XCODE_BUILD ?= 17F113
ORLIX_BAZEL_DISK_CACHE ?= $(ORLIX_BAZEL_CACHE_ROOT)/disk-cache/bazel-$(ORLIX_BAZEL_VERSION)-xcode-$(ORLIX_XCODE_BUILD)
ORLIX_BAZEL_REPOSITORY_CACHE ?= $(ORLIX_BAZEL_CACHE_ROOT)/repository-cache
ORLIX_BAZEL_OUTPUT_BASE ?= $(ORLIX_BUILD_ROOT)/Bazel/output-base
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
export ORLIX_BAZEL_TOOL_ROOT
export CCACHE_BASEDIR
export CCACHE_DIR
export CCACHE_MAXSIZE
export CCACHE_COMPILERCHECK

.PHONY: __bazel-bootstrap __bazel-version-check __bazel-server-restart __bazel-module-lock-update __bazel-feasibility-bootstrap __bazel-apple-smoke
.PHONY: __bazel-apple-dependency-smoke __bazel-native-archives
.PHONY: __bazel-ghostty-archives __bazel-ssh-archives
.PHONY: __bazel-native-dependency-smoke __bazel-feasibility-xcodeproj
.PHONY: __bazel-kernel-uapi __bazel-mlibc-from-uapi __bazel-live-activity-smoke
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
	@DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" build //bazel/feasibility/kernel:uapi --config=release --config=source --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)"
	@sh bazel/feasibility/kernel/uapi_contract_test.sh

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
	@PYTHONPATH="$(CURDIR)/bazel/config" ORLIX_XCODE_VERSION="$(ORLIX_XCODE_VERSION)" ORLIX_XCODE_BUILD="$(ORLIX_XCODE_BUILD)" ORLIX_BAZEL_DISK_CACHE="$(ORLIX_BAZEL_DISK_CACHE)" python3 -c 'import os, toolchain_pin as pin; pin.require_identity(os.environ["ORLIX_XCODE_VERSION"], os.environ["ORLIX_XCODE_BUILD"], os.environ["ORLIX_BAZEL_DISK_CACHE"]); print("pass: disk-cache namespace", pin.namespace_for(os.environ["ORLIX_XCODE_VERSION"], os.environ["ORLIX_XCODE_BUILD"]))'
	@DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" test //bazel/feasibility/analysis:all --config=release --config=source --xcode_version=$(ORLIX_XCODE_VERSION) --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)" --test_output=errors
