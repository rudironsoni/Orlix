SHELL := /bin/bash
.DEFAULT_GOAL := all
.NOTPARALLEL:

PROJECT_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
REPO_ROOT := $(abspath $(PROJECT_DIR)/..)
ORLIXOS_MAKE := $(MAKE) -C $(REPO_ROOT) -f OrlixOS/Makefile
MLIBC_MAKE := $(MAKE) -C $(REPO_ROOT) -f OrlixMLibC/Makefile

PROFILE ?= release
ORLIX_BUILD_ROOT ?= $(REPO_ROOT)/Build

include $(PROJECT_DIR)/Sources/make/config.mk
include $(REPO_ROOT)/OrlixOS/Sources/distribution/manifest.mk

ORLIX_COREUTILS_BUILD_ROOT := $(ORLIX_BUILD_ROOT)/OrlixCoreUtils
ORLIX_COREUTILS_UPSTREAM_DIR := $(ORLIX_COREUTILS_BUILD_ROOT)/upstream/coreutils-$(COREUTILS_VERSION).git
ORLIX_COREUTILS_SRC_DIR := $(ORLIX_COREUTILS_BUILD_ROOT)/src/coreutils-$(COREUTILS_VERSION)
ORLIX_COREUTILS_SOURCE_STAMP := $(ORLIX_COREUTILS_SRC_DIR)/.orlix-source-ready
ORLIX_COREUTILS_PACKAGE_BUILD_DIR := $(ORLIX_COREUTILS_BUILD_ROOT)/build/$(PROFILE)
ORLIX_COREUTILS_BUILD_DIR := $(ORLIX_COREUTILS_PACKAGE_BUILD_DIR)/coreutils-$(COREUTILS_VERSION)
ORLIX_COREUTILS_STAGE_DIR := $(ORLIX_COREUTILS_BUILD_ROOT)/stage/$(PROFILE)
ORLIX_COREUTILS_STAMP := $(ORLIX_COREUTILS_STAGE_DIR)/coreutils.stamp
ORLIX_COREUTILS_GETLIMITS_BINARY := $(ORLIX_COREUTILS_STAGE_DIR)/usr/bin/getlimits

ORLIX_COREUTILS_HOMEBREW_PREFIX ?= $(shell command -v brew >/dev/null 2>&1 && brew --prefix)
ORLIX_COREUTILS_BOOTSTRAP_PATH ?= $(if $(ORLIX_COREUTILS_HOMEBREW_PREFIX),$(ORLIX_COREUTILS_HOMEBREW_PREFIX)/opt/bison/bin:$(ORLIX_COREUTILS_HOMEBREW_PREFIX)/opt/coreutils/libexec/gnubin:$(PATH),$(PATH))
ORLIX_COREUTILS_OS_PACKAGE_DIR := $(ORLIX_BUILD_ROOT)/OrlixOS/packages/$(PROFILE)
ORLIX_COREUTILS_MLIBC_SYSROOT := $(ORLIX_BUILD_ROOT)/OrlixMLibC/sysroot/$(PROFILE)
ORLIX_COREUTILS_MLIBC_HEADERS := $(ORLIX_BUILD_ROOT)/OrlixMLibC/kernel-headers/$(PROFILE)/include
ORLIX_COREUTILS_MLIBC_RTLIB := $(ORLIX_BUILD_ROOT)/OrlixMLibC/compiler-rt/$(PROFILE)/liborlix_compiler_rt.a
ORLIX_COREUTILS_ACL_STAMP := $(ORLIX_COREUTILS_OS_PACKAGE_DIR)/acl.stamp
ORLIX_COREUTILS_LIBCAP_STAMP := $(ORLIX_COREUTILS_OS_PACKAGE_DIR)/libcap.stamp
ORLIX_COREUTILS_LIBSELINUX_STAMP := $(ORLIX_COREUTILS_OS_PACKAGE_DIR)/libselinux.stamp
ORLIX_COREUTILS_LIBATTR_A := $(ORLIX_COREUTILS_OS_PACKAGE_DIR)/usr/lib/libattr.a
ORLIX_COREUTILS_LIBACL_A := $(ORLIX_COREUTILS_OS_PACKAGE_DIR)/usr/lib/libacl.a
ORLIX_COREUTILS_LIBCAP_A := $(ORLIX_COREUTILS_OS_PACKAGE_DIR)/usr/lib/libcap.a
ORLIX_COREUTILS_LIBPCRE2_8_A := $(ORLIX_COREUTILS_OS_PACKAGE_DIR)/usr/lib/libpcre2-8.a
ORLIX_COREUTILS_LIBFTS_A := $(ORLIX_COREUTILS_OS_PACKAGE_DIR)/usr/lib/libfts.a
ORLIX_COREUTILS_LIBSEPOL_A := $(ORLIX_COREUTILS_OS_PACKAGE_DIR)/usr/lib/libsepol.a
ORLIX_COREUTILS_LIBSELINUX_A := $(ORLIX_COREUTILS_OS_PACKAGE_DIR)/usr/lib/libselinux.a
ORLIX_COMPILER_LAUNCHER ?= $(shell if command -v ccache >/dev/null 2>&1; then command -v ccache; elif command -v sccache >/dev/null 2>&1; then command -v sccache; fi)
ORLIX_COREUTILS_CC ?= clang
ORLIX_COREUTILS_CC_COMMAND := $(if $(strip $(ORLIX_COMPILER_LAUNCHER)),$(strip $(ORLIX_COMPILER_LAUNCHER)) $(ORLIX_COREUTILS_CC),$(ORLIX_COREUTILS_CC))
ORLIX_COREUTILS_LLVM_BIN ?= $(shell if command -v llvm-ar >/dev/null 2>&1; then dirname "$$(command -v llvm-ar)"; elif [ -x /opt/homebrew/opt/llvm/bin/llvm-ar ]; then printf '%s\n' /opt/homebrew/opt/llvm/bin; fi)
ORLIX_COREUTILS_AR ?= $(if $(ORLIX_COREUTILS_LLVM_BIN),$(ORLIX_COREUTILS_LLVM_BIN)/llvm-ar,llvm-ar)
ORLIX_COREUTILS_RANLIB ?= $(if $(ORLIX_COREUTILS_LLVM_BIN),$(ORLIX_COREUTILS_LLVM_BIN)/llvm-ranlib,llvm-ranlib)
ORLIX_COREUTILS_STRIP ?= $(if $(ORLIX_COREUTILS_LLVM_BIN),$(ORLIX_COREUTILS_LLVM_BIN)/llvm-strip,llvm-strip)
ORLIX_COREUTILS_BUILD_CC ?= xcrun --sdk macosx cc -target arm64-apple-macosx
ORLIX_COREUTILS_PACKAGE_CFLAGS := -O2 -D_FILE_OFFSET_BITS=64 -Wno-unknown-warning-option -Wno-incompatible-function-pointer-types -include limits.h
ORLIX_COREUTILS_FORCE_RECONFIGURE ?= 0

.PHONY: all help source dependencies build clean mrproper

all: build

help:
	@printf '%s\n' 'Targets:'
	@printf '%s\n' '  source       prepare the pinned upstream Coreutils checkout'
	@printf '%s\n' '  dependencies build the current OrlixOS-owned package prerequisites'
	@printf '%s\n' '  build        stage the Orlix Linux Coreutils payload'
	@printf '%s\n' '  clean        remove generated Coreutils build and staged outputs'

source: $(ORLIX_COREUTILS_SOURCE_STAMP)

dependencies:
	@$(ORLIXOS_MAKE) "$(ORLIX_COREUTILS_ACL_STAMP)" "$(ORLIX_COREUTILS_LIBCAP_STAMP)" "$(ORLIX_COREUTILS_LIBSELINUX_STAMP)" PROFILE="$(PROFILE)" ORLIX_BUILD_ROOT="$(ORLIX_BUILD_ROOT)"

build: dependencies $(ORLIX_COREUTILS_STAMP)

$(ORLIX_COREUTILS_ACL_STAMP) $(ORLIX_COREUTILS_LIBCAP_STAMP) $(ORLIX_COREUTILS_LIBSELINUX_STAMP):
	@$(ORLIXOS_MAKE) "$@" PROFILE="$(PROFILE)" ORLIX_BUILD_ROOT="$(ORLIX_BUILD_ROOT)"

$(ORLIX_COREUTILS_MLIBC_SYSROOT)/.orlixmlibc-sysroot-ready $(ORLIX_COREUTILS_MLIBC_RTLIB):
	@$(MLIBC_MAKE) build PROFILE="$(PROFILE)" ORLIX_BUILD_ROOT="$(ORLIX_BUILD_ROOT)"

$(ORLIX_COREUTILS_SOURCE_STAMP): $(PROJECT_DIR)/Sources/make/config.mk $(PROJECT_DIR)/Makefile
	@set -euo pipefail; \
	for path in "$(ORLIX_BUILD_ROOT)" "$(ORLIX_COREUTILS_BUILD_ROOT)" "$(ORLIX_COREUTILS_UPSTREAM_DIR)" "$(ORLIX_COREUTILS_SRC_DIR)"; do \
		if [ -e "$$path" ] && [ -L "$$path" ]; then echo "refusing to use symlinked OrlixCoreUtils path: $$path" >&2; exit 1; fi; \
	done; \
	export PATH="$(ORLIX_COREUTILS_BOOTSTRAP_PATH)"; \
	command -v git >/dev/null 2>&1 || { echo "git is required to clone Coreutils source" >&2; exit 1; }; \
	if [ -e "$(ORLIX_COREUTILS_SOURCE_STAMP)" ] && [ -d "$(ORLIX_COREUTILS_SRC_DIR)/.git" ] && [ -x "$(ORLIX_COREUTILS_SRC_DIR)/configure" ] && [ -e "$(ORLIX_COREUTILS_SRC_DIR)/build-aux/config.rpath" ] && [ -d "$(ORLIX_COREUTILS_SRC_DIR)/gnulib" ] && [ -d "$(ORLIX_COREUTILS_UPSTREAM_DIR)/objects" ]; then \
		actual="$$(git -C "$(ORLIX_COREUTILS_SRC_DIR)" rev-parse HEAD)"; \
		if [ "$$actual" = "$(COREUTILS_GIT_COMMIT)" ]; then \
			touch "$(ORLIX_COREUTILS_SOURCE_STAMP)"; \
			echo "upstream Coreutils source already ready: $(ORLIX_COREUTILS_SRC_DIR) ($(COREUTILS_GIT_REF) $(COREUTILS_GIT_COMMIT))"; \
			exit 0; \
		fi; \
	fi; \
	command -v autoconf >/dev/null 2>&1 || { echo "autoconf is required to bootstrap Coreutils from git" >&2; exit 1; }; \
	command -v automake >/dev/null 2>&1 || { echo "automake is required to bootstrap Coreutils from git" >&2; exit 1; }; \
	command -v autopoint >/dev/null 2>&1 || { echo "autopoint is required to bootstrap Coreutils from git" >&2; exit 1; }; \
	command -v bison >/dev/null 2>&1 || { echo "GNU bison is required to bootstrap Coreutils from git" >&2; exit 1; }; \
	command -v makeinfo >/dev/null 2>&1 || { echo "makeinfo is required to bootstrap Coreutils from git" >&2; exit 1; }; \
	command -v texi2pdf >/dev/null 2>&1 || { echo "texi2pdf is required to bootstrap Coreutils from git" >&2; exit 1; }; \
	mkdir -p "$(dir $(ORLIX_COREUTILS_UPSTREAM_DIR))" "$(dir $(ORLIX_COREUTILS_SRC_DIR))"; \
	if [ ! -d "$(ORLIX_COREUTILS_UPSTREAM_DIR)/objects" ]; then \
		rm -rf "$(ORLIX_COREUTILS_UPSTREAM_DIR)"; \
		git init --bare "$(ORLIX_COREUTILS_UPSTREAM_DIR)" >/dev/null; \
		git -C "$(ORLIX_COREUTILS_UPSTREAM_DIR)" remote add origin "$(COREUTILS_GIT_URL)"; \
	else \
		git -C "$(ORLIX_COREUTILS_UPSTREAM_DIR)" remote set-url origin "$(COREUTILS_GIT_URL)"; \
	fi; \
	git -C "$(ORLIX_COREUTILS_UPSTREAM_DIR)" fetch --force --depth 1 --no-tags origin "$(COREUTILS_GIT_COMMIT):refs/orlix/coreutils-$(COREUTILS_VERSION)"; \
	git -C "$(ORLIX_COREUTILS_UPSTREAM_DIR)" fetch --force --depth 1 origin "refs/tags/$(COREUTILS_GIT_REF):refs/tags/$(COREUTILS_GIT_REF)"; \
	git -C "$(ORLIX_COREUTILS_UPSTREAM_DIR)" symbolic-ref HEAD refs/orlix/coreutils-$(COREUTILS_VERSION); \
	rm -rf "$(ORLIX_COREUTILS_SRC_DIR)"; \
	git clone --shared --no-checkout "$(ORLIX_COREUTILS_UPSTREAM_DIR)" "$(ORLIX_COREUTILS_SRC_DIR)"; \
	git -C "$(ORLIX_COREUTILS_SRC_DIR)" checkout --detach "$(COREUTILS_GIT_COMMIT)"; \
	git -C "$(ORLIX_COREUTILS_SRC_DIR)" config submodule.gnulib.url "$(COREUTILS_GNULIB_GIT_URL)"; \
	git -C "$(ORLIX_COREUTILS_SRC_DIR)" submodule update --init --depth 1 --recommend-shallow --recursive --jobs 4; \
	actual="$$(git -C "$(ORLIX_COREUTILS_SRC_DIR)" rev-parse HEAD)"; \
	[ "$$actual" = "$(COREUTILS_GIT_COMMIT)" ] || { echo "Coreutils clone resolved $$actual, expected $(COREUTILS_GIT_COMMIT)" >&2; exit 1; }; \
	cd "$(ORLIX_COREUTILS_SRC_DIR)"; \
	./bootstrap --skip-po --no-git --gnulib-srcdir=gnulib; \
	touch "$(ORLIX_COREUTILS_SOURCE_STAMP)"; \
	echo "freshly cloned upstream Coreutils source: $(ORLIX_COREUTILS_SRC_DIR) ($(COREUTILS_GIT_REF) $(COREUTILS_GIT_COMMIT))"

$(ORLIX_COREUTILS_STAMP): $(ORLIX_COREUTILS_SOURCE_STAMP) $(ORLIX_COREUTILS_ACL_STAMP) $(ORLIX_COREUTILS_LIBCAP_STAMP) $(ORLIX_COREUTILS_LIBSELINUX_STAMP) $(ORLIX_COREUTILS_MLIBC_SYSROOT)/.orlixmlibc-sysroot-ready $(ORLIX_COREUTILS_MLIBC_RTLIB) $(PROJECT_DIR)/Makefile $(PROJECT_DIR)/Sources/make/config.mk
	@set -euo pipefail; \
	sysroot="$(ORLIX_COREUTILS_MLIBC_SYSROOT)"; \
	headers="$(ORLIX_COREUTILS_MLIBC_HEADERS)"; \
	rtlib="$(ORLIX_COREUTILS_MLIBC_RTLIB)"; \
	[ -s "$$sysroot/usr/lib/libc.a" ] || { echo "missing OrlixMLibC libc archive: $$sysroot/usr/lib/libc.a" >&2; exit 1; }; \
	[ -d "$$headers" ] || { echo "missing Orlix Linux UAPI headers: $$headers" >&2; exit 1; }; \
	[ -s "$$rtlib" ] || { echo "missing Orlix compiler runtime archive: $$rtlib" >&2; exit 1; }; \
	command -v "$(ORLIX_COREUTILS_CC)" >/dev/null 2>&1 || { echo "clang is required to build Coreutils; set ORLIX_COREUTILS_CC=/path/to/clang" >&2; exit 1; }; \
	if [ -n "$(ORLIX_COMPILER_LAUNCHER)" ]; then command -v "$(ORLIX_COMPILER_LAUNCHER)" >/dev/null 2>&1 || { echo "compiler launcher is not executable: $(ORLIX_COMPILER_LAUNCHER)" >&2; exit 1; }; fi; \
	command -v "$(ORLIX_COREUTILS_AR)" >/dev/null 2>&1 || { echo "llvm-ar is required to build Coreutils; set ORLIX_COREUTILS_AR=/path/to/llvm-ar" >&2; exit 1; }; \
	command -v "$(ORLIX_COREUTILS_RANLIB)" >/dev/null 2>&1 || { echo "llvm-ranlib is required to build Coreutils; set ORLIX_COREUTILS_RANLIB=/path/to/llvm-ranlib" >&2; exit 1; }; \
	command -v "$(ORLIX_COREUTILS_STRIP)" >/dev/null 2>&1 || { echo "llvm-strip is required to package Coreutils; set ORLIX_COREUTILS_STRIP=/path/to/llvm-strip" >&2; exit 1; }; \
	mkdir -p "$(ORLIX_COREUTILS_BUILD_DIR)" "$(ORLIX_COREUTILS_STAGE_DIR)/usr/bin"; \
	cd "$(ORLIX_COREUTILS_BUILD_DIR)"; \
	export CC="$(ORLIX_COREUTILS_CC_COMMAND) --target=aarch64-linux-gnu --sysroot=$$sysroot -isystem $$headers -D_GNU_SOURCE -fhosted -fno-builtin -ffixed-x18 -fPIE"; \
	export CPPFLAGS="-I$(ORLIX_COREUTILS_OS_PACKAGE_DIR)/usr/include"; \
	export CFLAGS="$(ORLIX_COREUTILS_PACKAGE_CFLAGS)"; \
	export LDFLAGS="--target=aarch64-linux-gnu --sysroot=$$sysroot -L$(ORLIX_COREUTILS_OS_PACKAGE_DIR)/usr/lib -static-pie -fuse-ld=lld -nostdlib -Wl,--gc-sections -Wl,-z,max-page-size=0x4000 $$sysroot/usr/lib/crt1.o $$sysroot/usr/lib/crti.o -Wl,--start-group"; \
	export LIBS="$(ORLIX_COREUTILS_LIBSELINUX_A) $(ORLIX_COREUTILS_LIBCAP_A) $(ORLIX_COREUTILS_LIBACL_A) $(ORLIX_COREUTILS_LIBATTR_A) $(ORLIX_COREUTILS_LIBSEPOL_A) $(ORLIX_COREUTILS_LIBPCRE2_8_A) $(ORLIX_COREUTILS_LIBFTS_A) $$sysroot/usr/lib/libc.a $$sysroot/usr/lib/libm.a $$sysroot/usr/lib/libpthread.a $$sysroot/usr/lib/libssp_nonshared.a $$sysroot/usr/lib/libssp.a $$rtlib -Wl,--end-group $$sysroot/usr/lib/crtn.o"; \
	export AR="$(ORLIX_COREUTILS_AR)"; \
	export RANLIB="$(ORLIX_COREUTILS_RANLIB)"; \
	export BUILD_CC="$(ORLIX_COREUTILS_BUILD_CC)"; \
	export PATH="$(ORLIX_COREUTILS_BOOTSTRAP_PATH)"; \
	export gl_cv_header_working_fcntl_h=yes; \
	export gl_cv_func_getopt_gnu=yes; \
	export gl_cv_func_getopt_long_gnu=yes; \
	export gl_cv_func_strtod_works=yes; \
	configure_signature="$(ORLIX_COREUTILS_BUILD_DIR)/.orlix-configure.signature"; \
	current_signature="profile=$(PROFILE)|version=$(COREUTILS_VERSION)|commit=$(COREUTILS_GIT_COMMIT)|source=$(ORLIX_COREUTILS_SRC_DIR)|cc=$$CC|cppflags=$$CPPFLAGS|cflags=$$CFLAGS|ldflags=$$LDFLAGS|libs=$$LIBS"; \
	outputs_ready=1; \
	for program in $(ORLIX_COREUTILS_PROGRAMS); do \
		if [ ! -x "$(ORLIX_COREUTILS_STAGE_DIR)/usr/bin/$$program" ]; then outputs_ready=0; fi; \
	done; \
	if [ "$(ORLIX_COREUTILS_FORCE_RECONFIGURE)" != 1 ] && [ "$$outputs_ready" = 1 ] && [ -x "$(ORLIX_COREUTILS_GETLIMITS_BINARY)" ] && [ -s "$(ORLIX_COREUTILS_STAMP)" ] && [ -s Makefile ] && [ -s "$$configure_signature" ] && [ "$$(cat "$$configure_signature")" = "$$current_signature" ] && [ "$(ORLIX_COREUTILS_STAMP)" -nt "$(ORLIX_COREUTILS_SOURCE_STAMP)" ] && [ "$(ORLIX_COREUTILS_STAMP)" -nt "$(ORLIX_COREUTILS_ACL_STAMP)" ] && [ "$(ORLIX_COREUTILS_STAMP)" -nt "$(ORLIX_COREUTILS_LIBCAP_STAMP)" ] && [ "$(ORLIX_COREUTILS_STAMP)" -nt "$(ORLIX_COREUTILS_LIBSELINUX_STAMP)" ] && [ "$(ORLIX_COREUTILS_STAMP)" -nt "$(ORLIX_COREUTILS_MLIBC_SYSROOT)/.orlixmlibc-sysroot-ready" ] && [ "$(ORLIX_COREUTILS_STAMP)" -nt "$(ORLIX_COREUTILS_MLIBC_RTLIB)" ]; then \
		touch "$(ORLIX_COREUTILS_STAMP)"; \
		echo "reusing Orlix Linux Coreutils provider payload: $(ORLIX_COREUTILS_STAGE_DIR)"; \
		exit 0; \
	fi; \
	rm -f "$(ORLIX_COREUTILS_STAMP)"; \
	if [ "$(ORLIX_COREUTILS_FORCE_RECONFIGURE)" = 1 ] || [ ! -s Makefile ] || [ ! -s "$$configure_signature" ] || [ "$$(cat "$$configure_signature")" != "$$current_signature" ]; then \
		cd "$(ORLIX_COREUTILS_PACKAGE_BUILD_DIR)"; \
		rm -rf "$(ORLIX_COREUTILS_BUILD_DIR)"; \
		mkdir -p "$(ORLIX_COREUTILS_BUILD_DIR)"; \
		cd "$(ORLIX_COREUTILS_BUILD_DIR)"; \
		"$(ORLIX_COREUTILS_SRC_DIR)/configure" --host=aarch64-linux-gnu --build=aarch64-apple-darwin --prefix=/usr --disable-nls --with-selinux --enable-libcap --disable-gcc-warnings; \
		printf '%s\n' "$$current_signature" > "$$configure_signature"; \
	fi; \
	"$${MAKE:-make}" -j1 all PROGRAMS= LIBRARIES= MANS= INFO_DEPS=; \
	for program in $(ORLIX_COREUTILS_PROGRAMS); do \
		source_program="$$program"; \
		if [ "$$program" = install ]; then source_program=ginstall; fi; \
		"$${MAKE:-make}" -j1 "src/$$source_program" MANS= INFO_DEPS=; \
	done; \
	"$${MAKE:-make}" -j1 src/getlimits; \
	for program in $(ORLIX_COREUTILS_PROGRAMS); do \
		source_program="$$program"; \
		if [ "$$program" = install ]; then source_program=ginstall; fi; \
		"$(ORLIX_COREUTILS_STRIP)" "$(ORLIX_COREUTILS_BUILD_DIR)/src/$$source_program"; \
		install -m 0755 "$(ORLIX_COREUTILS_BUILD_DIR)/src/$$source_program" "$(ORLIX_COREUTILS_STAGE_DIR)/usr/bin/$$program"; \
	done; \
	"$(ORLIX_COREUTILS_STRIP)" "$(ORLIX_COREUTILS_BUILD_DIR)/src/getlimits"; \
	cp "$(ORLIX_COREUTILS_BUILD_DIR)/src/getlimits" "$(ORLIX_COREUTILS_GETLIMITS_BINARY)"; \
	file "$(ORLIX_COREUTILS_GETLIMITS_BINARY)" | grep -F -q 'ELF 64-bit LSB pie executable, ARM aarch64' || { file "$(ORLIX_COREUTILS_GETLIMITS_BINARY)" >&2; exit 1; }; \
	for program in $(ORLIX_COREUTILS_PROGRAMS); do \
		file "$(ORLIX_COREUTILS_STAGE_DIR)/usr/bin/$$program" | grep -F -q 'ELF 64-bit LSB pie executable, ARM aarch64' || { file "$(ORLIX_COREUTILS_STAGE_DIR)/usr/bin/$$program" >&2; exit 1; }; \
	done; \
	printf 'profile=%s\ndistribution=%s\nchannel=%s\npackage=coreutils\nprograms=%s\nversion=%s\ngit_url=%s\ngit_ref=%s\ngit_commit=%s\ngnulib_git_url=%s\n' "$(PROFILE)" "$(ORLIXOS_DISTRIBUTION_ID)" "$(ORLIXOS_DISTRIBUTION_CHANNEL)" "$(ORLIX_COREUTILS_PROGRAMS)" "$(COREUTILS_VERSION)" "$(COREUTILS_GIT_URL)" "$(COREUTILS_GIT_REF)" "$(COREUTILS_GIT_COMMIT)" "$(COREUTILS_GNULIB_GIT_URL)" > "$(ORLIX_COREUTILS_STAMP)"; \
	echo "built Orlix Linux Coreutils provider payload: $(ORLIX_COREUTILS_STAGE_DIR)"

clean:
	@set -euo pipefail; \
	if [ -L "$(ORLIX_COREUTILS_BUILD_ROOT)" ]; then echo "refusing to clean symlinked OrlixCoreUtils build root: $(ORLIX_COREUTILS_BUILD_ROOT)" >&2; exit 1; fi; \
	rm -rf "$(ORLIX_COREUTILS_BUILD_ROOT)/build" "$(ORLIX_COREUTILS_BUILD_ROOT)/stage"

mrproper:
	@set -euo pipefail; \
	if [ -L "$(ORLIX_COREUTILS_BUILD_ROOT)" ]; then echo "refusing to clean symlinked OrlixCoreUtils build root: $(ORLIX_COREUTILS_BUILD_ROOT)" >&2; exit 1; fi; \
	rm -rf "$(ORLIX_COREUTILS_BUILD_ROOT)"
