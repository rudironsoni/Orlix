SHELL := /bin/bash
.DEFAULT_GOAL := all

PROJECT_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
REPO_ROOT := $(abspath $(PROJECT_DIR)/..)
KERNEL_MAKE := $(MAKE) -C $(REPO_ROOT) -f OrlixKernel/Makefile
ORLIXOS_MAKE := $(MAKE) -C $(REPO_ROOT) -f OrlixOS/Makefile

PROFILE ?= release
ORLIX_KERNEL_BASE_ROOT_TREE_INPUT ?= $(REPO_ROOT)/Build/OrlixOS/rootfs/$(PROFILE)/base-tree
ORLIX_KERNEL_STATE_ROOT_TREE_INPUT ?= $(REPO_ROOT)/Build/OrlixOS/rootfs/$(PROFILE)/state-tree
ORLIX_KERNEL_ROOT_INITRAMFS_INPUT ?= $(REPO_ROOT)/Build/OrlixOS/rootfs/$(PROFILE)/rootfs/initramfs.cpio.gz

type ?= app
vendor ?= all

include $(PROJECT_DIR)/make/vendor.mk
include $(PROJECT_DIR)/make/vvterm-sync.mk

.PHONY: all help build __build-app __vendor-reexec run clean mrproper

all: build

help:
	@printf '%s\n' 'Targets:'
	@printf '%s\n' '  build                         validate Orlix source ownership'
	@printf '%s\n' '  build type=vendor vendor=all  build all native vendor libraries'
	@printf '%s\n' '  build type=vendor vendor=ghostty|ssh|tssh'
	@printf '%s\n' '  run                           build and run the Orlix iOS host app'
	@printf '%s\n' '  clean                         remove project-local build intermediates only'
	@printf '%s\n' '  vvterm-sync                  merge one exact upstream snapshot into tracked Orlix source'
	@printf '%s\n' '  vvterm-sync-resolve          apply one reviewed conflict-resolution manifest'
	@printf '%s\n' '  vvterm-reconcile             apply the pending versioned branding policy'
	@printf '%s\n' '  vvterm-sync-complete         validate a resolved sync and advance source provenance'
	@printf '%s\n' '  vvterm-source-check          validate the tracked source snapshot and branding contract'
	@printf '%s\n' '  vvterm-upstream-tests        test the exact pristine upstream source revision'
	@printf '%s\n' 'Variables for vendor builds:'
	@printf '%s\n' '  GHOSTTY_REF=<40-char-sha>      override the pinned Ghostty commit'
	@printf '%s\n' '  KEEP_WORKDIR=1                 retain the temporary Ghostty checkout'

ifneq ($(filter build,$(MAKECMDGOALS)),)
ifeq ($(type),vendor)
ifneq ($(filter $(vendor),all ghostty ssh tssh),$(vendor))
$(error invalid vendor selector '$(vendor)'; expected all, ghostty, ssh, or tssh)
endif
ifeq ($(MAKE_VERSION),3.81)
build: __vendor-reexec
else
build: __vendor-$(vendor)
endif
else ifeq ($(type),app)
build: __build-app
else
$(error invalid build type '$(type)'; expected app or vendor)
endif
else
build: __build-app
endif

__build-app:
	@printf '%s\n' 'Orlix is built through the generated Orlix Xcode scheme.'

__vendor-reexec:
	@command -v gmake >/dev/null 2>&1 || { echo "GNU Make 4 is required for vendor builds; run: brew bundle --file Brewfile" >&2; exit 1; }
	@gmake --no-print-directory -f "$(PROJECT_DIR)/Makefile" build type=vendor vendor="$(vendor)" GHOSTTY_REF="$(GHOSTTY_REF)" KEEP_WORKDIR="$(KEEP_WORKDIR)"

run:
	@set -e
	@$(ORLIXOS_MAKE) rootfs PROFILE="$(PROFILE)"
	@$(KERNEL_MAKE) run PROFILE="$(PROFILE)" ORLIX_KERNEL_ROOT_INITRAMFS_INPUT="$(ORLIX_KERNEL_ROOT_INITRAMFS_INPUT)" ORLIX_KERNEL_BASE_ROOT_TREE_INPUT="$(ORLIX_KERNEL_BASE_ROOT_TREE_INPUT)" ORLIX_KERNEL_STATE_ROOT_TREE_INPUT="$(ORLIX_KERNEL_STATE_ROOT_TREE_INPUT)"

clean:
	@rm -rf "$(PROJECT_DIR)/.build"

mrproper: clean
