ORLIX_BAZEL_CACHE_ROOT ?= $(HOME)/Library/Caches/Orlix/Bazel
ORLIX_BAZEL_VERSION ?= 9.2.0
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
export ORLIX_XCODE_BUILD
export ORLIX_BAZEL_DISK_CACHE
export ORLIX_BAZEL_REPOSITORY_CACHE
export ORLIX_BAZEL_OUTPUT_BASE
export ORLIX_BAZEL_TOOL_ROOT
export CCACHE_BASEDIR
export CCACHE_DIR
export CCACHE_MAXSIZE
export CCACHE_COMPILERCHECK

.PHONY: __bazel-bootstrap __bazel-version-check __bazel-feasibility-bootstrap __bazel-apple-smoke
.PHONY: __bazel-apple-dependency-smoke
.PHONY: __bazel-migration-inventory __bazel-migration-inventory-check

__bazel-bootstrap:
	@ruby bazel/bootstrap.rb >/dev/null

__bazel-version-check: __bazel-bootstrap
	@test "$$($(ORLIX_BAZEL) --version)" = "bazel 9.2.0" || { echo "Bazel 9.2.0 is required" >&2; exit 1; }

__bazel-feasibility-bootstrap: __bazel-version-check __bazel-migration-inventory-check
	@test -d "$(ORLIX_PINNED_DEVELOPER_DIR)" || { echo "missing pinned Xcode developer directory: $(ORLIX_PINNED_DEVELOPER_DIR)" >&2; exit 1; }
	@test "$$(DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" /usr/bin/xcodebuild -version | head -n 1)" = "Xcode 26.6" || { echo "Xcode 26.6 is required" >&2; exit 1; }
	@mkdir -p "$(ORLIX_BAZEL_DISK_CACHE)" "$(ORLIX_BAZEL_REPOSITORY_CACHE)" "$(ORLIX_BAZEL_OUTPUT_BASE)"
	@DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" mod deps --lockfile_mode=error --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)"
	@DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" build //bazel/config:all --config=release --config=source --xcode_version=26.6 --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)"

__bazel-apple-smoke: __bazel-feasibility-bootstrap
	@DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" build //bazel/feasibility/apple:SmokeApp --compilation_mode=dbg --config=release --config=source --ios_multi_cpus=sim_arm64 --xcode_version=26.6 --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)"

__bazel-apple-dependency-smoke: __bazel-feasibility-bootstrap
	@DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" "$(ORLIX_BAZEL)" --output_base="$(ORLIX_BAZEL_OUTPUT_BASE)" build //bazel/feasibility/apple:DependencySmokeApp --compilation_mode=dbg --config=release --config=source --ios_multi_cpus=sim_arm64 --xcode_version=26.6 --repo_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --host_action_env=DEVELOPER_DIR="$(ORLIX_PINNED_DEVELOPER_DIR)" --disk_cache="$(ORLIX_BAZEL_DISK_CACHE)" --repository_cache="$(ORLIX_BAZEL_REPOSITORY_CACHE)"

__bazel-migration-inventory:
	@ruby bazel/migration/inventory.rb --write

__bazel-migration-inventory-check:
	@ruby bazel/migration/inventory.rb --check
