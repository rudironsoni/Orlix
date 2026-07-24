# SPDX-License-Identifier: GPL-2.0-only
#
# This directory is an explicit maintainer refresh tool.  It reads the pinned
# Arm source release and emits or verifies checked-in C artifacts.  Normal
# kernel builds and normal ISA audits must not invoke it.

CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -pedantic

BUILD_ROOT ?= ../../Build
OUT := $(abspath $(BUILD_ROOT)/Tools/tcti-isa-maintainer)

INSTRUCTIONS ?= $(ORLIX_AARCHMRS_INSTRUCTIONS)
FEATURES ?= $(ORLIX_AARCHMRS_FEATURES)
REGISTERS ?= $(ORLIX_AARCHMRS_REGISTERS)

.PHONY: check source-check refresh

check: source-check

source-check:
	@set -eu; \
	for pair in "INSTRUCTIONS:$(INSTRUCTIONS):Instructions" "FEATURES:$(FEATURES):Features" "REGISTERS:$(REGISTERS):Registers"; do \
		name=$${pair%%:*}; rest=$${pair#*:}; path=$${rest%%:*}; kind=$${rest#*:}; \
		[ -n "$$path" ] || { echo "$$name must name the pinned Arm $$kind source" >&2; exit 2; }; \
		[ -f "$$path" ] || { echo "Arm $$kind source not found: $$path" >&2; exit 2; }; \
	done
	@mkdir -p '$(OUT)'
	@$(CC) $(CFLAGS) target_inventory_import.c target_inventory_import_test.c \
		-o '$(OUT)/target_inventory_import_test'
	@'$(OUT)/target_inventory_import_test' '$(INSTRUCTIONS)'
	@$(CC) $(CFLAGS) target_feature_model.c target_feature_model_test.c \
		-o '$(OUT)/target_feature_model_test'
	@'$(OUT)/target_feature_model_test' '$(FEATURES)'
	@$(CC) $(CFLAGS) target_register_model.c target_register_model_test.c \
		-o '$(OUT)/target_register_model_test'
	@'$(OUT)/target_register_model_test' '$(REGISTERS)'
	@$(CC) $(CFLAGS) target_register_model.c \
		target_register_artifact_generator_test.c \
		-o '$(OUT)/target_register_artifact_generator_test'
	@'$(OUT)/target_register_artifact_generator_test' '$(REGISTERS)'

# Refresh is intentionally unavailable until all three source importers produce
# one transactionally validated C artifact set, including registers.
refresh:
	@echo "tcti ISA refresh is unavailable until the register C artifact generator is complete" >&2
	@exit 2
