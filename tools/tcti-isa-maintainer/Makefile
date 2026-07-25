# SPDX-License-Identifier: GPL-2.0-only
#
# This directory is an explicit maintainer refresh tool.  It reads the pinned
# Arm source release and emits or verifies checked-in C artifacts.  Normal
# kernel builds and normal ISA audits must not invoke it.

CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -pedantic

BUILD_ROOT ?= ../../Build
OUT := $(abspath $(BUILD_ROOT)/Tools/tcti-isa-maintainer)
CONDITION_INCLUDE := -I../../OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/tcti/tests
REFRESH_DEFINES := -DTARGET_MANIFEST_GENERATOR_NO_MAIN \
	-DTARGET_INSTRUCTION_ARTIFACT_GENERATOR_NO_MAIN \
	-DTARGET_FEATURE_ARTIFACT_GENERATOR_NO_MAIN \
	-DTARGET_REGISTER_ARTIFACT_GENERATOR_NO_MAIN
REFRESH_TEST_DEFINES := $(REFRESH_DEFINES) -DTCTI_TARGET_REFRESH_NO_MAIN
REFRESH_SOURCES := target_refresh.c target_artifact_publisher.c \
	target_manifest_generator.c target_inventory_import.c \
	target_condition_serialization.c target_instruction_artifact_generator.c \
	target_feature_model.c target_feature_artifact_generator.c \
	target_register_model.c target_register_artifact_generator.c \
	target_asl_availability.c target_system_accessor_reconciliation.c

INSTRUCTIONS ?= $(ORLIX_AARCHMRS_INSTRUCTIONS)
FEATURES ?= $(ORLIX_AARCHMRS_FEATURES)
REGISTERS ?= $(ORLIX_AARCHMRS_REGISTERS)
CANONICAL_DIR := $(abspath ../../OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/tcti/isa)

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
	@$(CC) $(CFLAGS) target_inventory_import.c target_asl_availability.c \
		target_asl_availability_test.c -o '$(OUT)/target_asl_availability_test'
	@'$(OUT)/target_asl_availability_test' '$(INSTRUCTIONS)'
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
	@$(CC) $(CFLAGS) target_register_model.c \
		target_system_accessor_reconciliation.c \
		target_system_accessor_reconciliation_test.c \
		-o '$(OUT)/target_system_accessor_reconciliation_test'
	@'$(OUT)/target_system_accessor_reconciliation_test' '$(REGISTERS)'
	@$(CC) $(CFLAGS) target_register_model.c \
		target_system_accessor_reconciliation.c \
		target_system_accessor_reconciliation_generator.c \
		-o '$(OUT)/target_system_accessor_reconciliation_generator'
	@$(CC) $(CFLAGS) target_artifact_publisher_test.c \
		-o '$(OUT)/target_artifact_publisher_test'
	@'$(OUT)/target_artifact_publisher_test'
	@$(CC) $(CFLAGS) $(CONDITION_INCLUDE) $(REFRESH_TEST_DEFINES) \
		target_refresh_test.c $(REFRESH_SOURCES) \
		-o '$(OUT)/target_refresh_test'
	@'$(OUT)/target_refresh_test' '$(CANONICAL_DIR)' '$(INSTRUCTIONS)' '$(FEATURES)' '$(REGISTERS)'

refresh: source-check
	@mkdir -p '$(BUILD_ROOT)'
	@$(CC) $(CFLAGS) $(CONDITION_INCLUDE) $(REFRESH_DEFINES) \
		target_refresh.c $(filter-out target_refresh.c,$(REFRESH_SOURCES)) \
		-o '$(OUT)/target_refresh'
	@'$(OUT)/target_refresh' '$(BUILD_ROOT)' '$(CANONICAL_DIR)' '$(INSTRUCTIONS)' '$(FEATURES)' '$(REGISTERS)'
