# SPDX-License-Identifier: GPL-2.0-only
#
# This directory is an explicit maintainer refresh tool.  It reads the pinned
# Arm source release and emits or verifies checked-in C artifacts.  Normal
# kernel builds and normal ISA audits must not invoke it.

CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -pedantic
ORLIX_TCTI_KERNEL_BUILD_TIME := $(abspath ../../OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/isa/build-time)
INVENTORY_CPPFLAGS := -I$(ORLIX_TCTI_KERNEL_BUILD_TIME)
TARGET_INVENTORY_IMPORT_SOURCE := $(ORLIX_TCTI_KERNEL_BUILD_TIME)/target_inventory_import.c
TARGET_INVENTORY_IMPORT_TEST := $(ORLIX_TCTI_KERNEL_BUILD_TIME)/target_inventory_import_test.c
TARGET_CLASSIFICATION_GENERATOR := $(ORLIX_TCTI_KERNEL_BUILD_TIME)/target_classification_generator.c

ORLIX_BUILD_ROOT ?= ../../Build
OUT := $(abspath $(ORLIX_BUILD_ROOT)/Tools/orlix-tcti-isa-maintainer)
CONDITION_INCLUDE := -I../../OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests
REFRESH_DEFINES := -DTARGET_MANIFEST_GENERATOR_NO_MAIN \
	-DTARGET_INSTRUCTION_ARTIFACT_GENERATOR_NO_MAIN \
	-DTARGET_FEATURE_ARTIFACT_GENERATOR_NO_MAIN \
	-DTARGET_FEATURE_FIELD_DOMAIN_BINDING_ARTIFACT_GENERATOR_NO_MAIN \
	-DTARGET_RUNTIME_CAPABILITY_COHORT_ARTIFACT_GENERATOR_NO_MAIN \
	-DTARGET_REGISTER_ARTIFACT_GENERATOR_NO_MAIN
REFRESH_TEST_DEFINES := $(REFRESH_DEFINES) -DORLIX_TCTI_TARGET_REFRESH_NO_MAIN
REFRESH_SOURCES := target_refresh.c target_artifact_publisher.c \
	target_manifest_generator.c $(TARGET_INVENTORY_IMPORT_SOURCE) \
	target_condition_serialization.c target_instruction_artifact_generator.c \
	target_feature_model.c target_feature_artifact_generator.c \
	target_feature_field_domain_binding.c \
	target_feature_field_domain_binding_artifact_generator.c \
	target_runtime_capability_cohort_artifact_generator.c \
	target_register_model.c target_register_artifact_generator.c \
	target_arm_xml_package.c target_asl_availability.c \
	target_system_accessor_reconciliation.c

INSTRUCTIONS ?= $(ORLIX_AARCHMRS_INSTRUCTIONS)
FEATURES ?= $(ORLIX_AARCHMRS_FEATURES)
REGISTERS ?= $(ORLIX_AARCHMRS_REGISTERS)
ARM_XML_ARCHIVE ?= $(ORLIX_A64_ISA_XML_ARCHIVE)
ARM_XML_RELEASE ?= $(ORLIX_A64_ISA_XML_RELEASE)
CANONICAL_DIR := $(abspath ../../OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/isa)

.PHONY: check source-check refresh

check: source-check

source-check:
	@set -eu; \
	for pair in "INSTRUCTIONS:$(INSTRUCTIONS):Instructions" "FEATURES:$(FEATURES):Features" "REGISTERS:$(REGISTERS):Registers"; do \
		name=$${pair%%:*}; rest=$${pair#*:}; path=$${rest%%:*}; kind=$${rest#*:}; \
		[ -n "$$path" ] || { echo "$$name must name the pinned Arm $$kind source" >&2; exit 2; }; \
		[ -f "$$path" ] || { echo "Arm $$kind source not found: $$path" >&2; exit 2; }; \
	done
	@[ -n '$(ARM_XML_ARCHIVE)' ] || { echo "ARM_XML_ARCHIVE must name ISA_A64_xml_A_profile-2026-06.tar.gz" >&2; exit 2; }
	@[ -f '$(ARM_XML_ARCHIVE)' ] || { echo "Arm A64 ISA XML archive not found: $(ARM_XML_ARCHIVE)" >&2; exit 2; }
	@[ -n '$(ARM_XML_RELEASE)' ] || { echo "ARM_XML_RELEASE must name ISA_A64_xml_A_profile-2026-06" >&2; exit 2; }
	@[ -d '$(ARM_XML_RELEASE)' ] || { echo "Arm A64 ISA XML release not found: $(ARM_XML_RELEASE)" >&2; exit 2; }
	@mkdir -p '$(OUT)'
	@$(CC) $(CFLAGS) target_artifact_publisher.c target_arm_xml_package.c \
		target_arm_xml_package_test.c -o '$(OUT)/target_arm_xml_package_test'
	@'$(OUT)/target_arm_xml_package_test' '$(ARM_XML_ARCHIVE)' '$(ARM_XML_RELEASE)'
	@$(CC) $(CFLAGS) $(INVENTORY_CPPFLAGS) $(TARGET_INVENTORY_IMPORT_SOURCE) $(TARGET_INVENTORY_IMPORT_TEST) \
		-o '$(OUT)/target_inventory_import_test'
	@'$(OUT)/target_inventory_import_test' '$(INSTRUCTIONS)'
	@$(CC) $(CFLAGS) $(INVENTORY_CPPFLAGS) $(TARGET_INVENTORY_IMPORT_SOURCE) \
		$(TARGET_CLASSIFICATION_GENERATOR) \
		-o '$(OUT)/target_classification_generator'
	@'$(OUT)/target_classification_generator' '$(INSTRUCTIONS)' \
		> '$(OUT)/target_classification.generated.def'
	@$(CC) $(CFLAGS) $(INVENTORY_CPPFLAGS) $(TARGET_INVENTORY_IMPORT_SOURCE) \
		target_artifact_publisher.c target_arm_xml_package.c target_asl_availability.c \
		target_asl_availability_test.c -o '$(OUT)/target_asl_availability_test'
	@'$(OUT)/target_asl_availability_test' '$(INSTRUCTIONS)' \
		'$(ARM_XML_ARCHIVE)' '$(ARM_XML_RELEASE)'
	@$(CC) $(CFLAGS) $(INVENTORY_CPPFLAGS) target_feature_model.c target_feature_model_test.c \
		-o '$(OUT)/target_feature_model_test'
	@'$(OUT)/target_feature_model_test' '$(FEATURES)'
	@$(CC) $(CFLAGS) $(INVENTORY_CPPFLAGS) target_feature_model.c target_register_model.c \
		target_feature_field_domain_binding.c \
		target_feature_field_domain_binding_test.c \
		-o '$(OUT)/target_feature_field_domain_binding_test'
	@'$(OUT)/target_feature_field_domain_binding_test' '$(FEATURES)' '$(REGISTERS)'
	@$(CC) $(CFLAGS) $(INVENTORY_CPPFLAGS) -DTARGET_FEATURE_FIELD_DOMAIN_BINDING_ARTIFACT_GENERATOR_NO_MAIN \
		target_feature_model.c target_register_model.c \
		target_feature_field_domain_binding.c \
		target_feature_field_domain_binding_artifact_generator.c \
		target_feature_field_domain_binding_artifact_generator_test.c \
		-o '$(OUT)/target_feature_field_domain_binding_artifact_generator_test'
	@'$(OUT)/target_feature_field_domain_binding_artifact_generator_test' '$(FEATURES)' '$(REGISTERS)'
	@$(CC) $(CFLAGS) $(INVENTORY_CPPFLAGS) $(TARGET_INVENTORY_IMPORT_SOURCE) target_feature_model.c \
		target_runtime_capability_cohort_artifact_generator_test.c \
		-o '$(OUT)/target_runtime_capability_cohort_artifact_generator_test'
	@'$(OUT)/target_runtime_capability_cohort_artifact_generator_test' '$(INSTRUCTIONS)' '$(FEATURES)'
	@$(CC) $(CFLAGS) $(INVENTORY_CPPFLAGS) target_register_model.c target_register_model_test.c \
		-o '$(OUT)/target_register_model_test'
	@'$(OUT)/target_register_model_test' '$(REGISTERS)'
	@$(CC) $(CFLAGS) $(INVENTORY_CPPFLAGS) target_register_model.c \
		target_register_artifact_generator_test.c \
		-o '$(OUT)/target_register_artifact_generator_test'
	@'$(OUT)/target_register_artifact_generator_test' '$(REGISTERS)'
	@$(CC) $(CFLAGS) $(INVENTORY_CPPFLAGS) target_register_model.c \
		target_system_accessor_reconciliation.c \
		target_system_accessor_reconciliation_test.c \
		-o '$(OUT)/target_system_accessor_reconciliation_test'
	@'$(OUT)/target_system_accessor_reconciliation_test' '$(REGISTERS)'
	@$(CC) $(CFLAGS) target_register_model.c \
		target_system_accessor_reconciliation.c \
		target_system_accessor_reconciliation_generator.c \
		-o '$(OUT)/target_system_accessor_reconciliation_generator'
	@$(CC) $(CFLAGS) $(INVENTORY_CPPFLAGS) target_artifact_publisher_test.c \
		-o '$(OUT)/target_artifact_publisher_test'
	@'$(OUT)/target_artifact_publisher_test'
	@$(CC) $(CFLAGS) $(INVENTORY_CPPFLAGS) $(CONDITION_INCLUDE) $(REFRESH_TEST_DEFINES) \
		target_refresh_test.c $(REFRESH_SOURCES) \
		-o '$(OUT)/target_refresh_test'
	@'$(OUT)/target_refresh_test' '$(INSTRUCTIONS)' '$(FEATURES)' '$(REGISTERS)' \
		'$(ARM_XML_ARCHIVE)' '$(ARM_XML_RELEASE)'

refresh: source-check
	@mkdir -p '$(ORLIX_BUILD_ROOT)'
	@$(CC) $(CFLAGS) $(INVENTORY_CPPFLAGS) $(CONDITION_INCLUDE) $(REFRESH_DEFINES) \
		target_refresh.c $(filter-out target_refresh.c,$(REFRESH_SOURCES)) \
		-o '$(OUT)/target_refresh'
	@'$(OUT)/target_refresh' '$(CANONICAL_DIR)' '$(INSTRUCTIONS)' '$(FEATURES)' '$(REGISTERS)' \
		'$(ARM_XML_ARCHIVE)' '$(ARM_XML_RELEASE)'
