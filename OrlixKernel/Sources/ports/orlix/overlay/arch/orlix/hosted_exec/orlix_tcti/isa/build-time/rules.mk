# SPDX-License-Identifier: GPL-2.0-only
#
# Private OrlixKernel build-time rules for validating and atomically refreshing
# the checked-in OrlixTCTI ISA artifacts. Developer entry points are owned by
# the parent OrlixKernel Make surface; this file is not a standalone Makefile.

ORLIX_TCTI_ISA_BUILD_TIME_ROOT := $(CURDIR)/OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/isa/build-time
ORLIX_TCTI_ISA_CANONICAL_ROOT := $(CURDIR)/OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/isa
ORLIX_TCTI_ISA_TEST_ROOT := $(CURDIR)/OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests
ORLIX_TCTI_ISA_MAINTAINER_OUT := $(ORLIX_BUILD_ROOT)/OrlixKernel/orlix-tcti-target-refresh
ORLIX_TCTI_ISA_MAINTAINER_CFLAGS ?= -std=c11 -Wall -Wextra -Werror -pedantic
ORLIX_TCTI_ISA_MAINTAINER_CPPFLAGS := -I$(ORLIX_TCTI_ISA_BUILD_TIME_ROOT)
ORLIX_TCTI_ISA_MAINTAINER_CONDITION_CPPFLAGS := -I$(ORLIX_TCTI_ISA_TEST_ROOT)
ORLIX_TCTI_ISA_MAINTAINER_COMPILE = cd '$(ORLIX_TCTI_ISA_BUILD_TIME_ROOT)' && $(ORLIX_KERNEL_HOSTCC) $(ORLIX_TCTI_ISA_MAINTAINER_CFLAGS)
ORLIX_TCTI_ISA_MAINTAINER_REFRESH_IDENTITY_INPUTS := \
	target_refresh.c target_refresh.h target_artifact_publisher.c \
	target_artifact_publisher.h target_refresh_artifacts.def \
	target_manifest_generator.c target_manifest_generator.h \
	target_inventory_import.c target_inventory_import.h \
	target_condition_serialization.c target_condition_serialization.h \
	target_instruction_artifact_generator.c target_instruction_artifact_generator.h \
	target_feature_model.c target_feature_model.h \
	target_feature_artifact_generator.c target_feature_artifact_generator.h \
	target_feature_field_domain_binding.c target_feature_field_domain_binding.h \
	target_feature_sat.c target_feature_sat.h \
	target_feature_applicability_generator.c target_feature_applicability_generator.h \
	target_feature_field_domain_binding_artifact_generator.c \
	target_feature_field_domain_binding_artifact_generator.h \
	target_runtime_capability_cohort_artifact_generator.c \
	target_runtime_capability_cohort_artifact_generator.h \
	target_register_model.c target_register_model.h \
	target_register_artifact_generator.c target_register_artifact_generator.h \
	target_arm_xml_package.c target_arm_xml_package.h \
	target_asl_availability.c target_asl_availability.h \
	target_system_accessor_reconciliation.c target_system_accessor_reconciliation.h
ORLIX_TCTI_ISA_MAINTAINER_REFRESH_PRODUCER_SHA256 := $(shell cd '$(ORLIX_TCTI_ISA_BUILD_TIME_ROOT)' && set -euo pipefail; { for input in $(ORLIX_TCTI_ISA_MAINTAINER_REFRESH_IDENTITY_INPUTS); do test -f "$$input"; digest_output="$$(shasum -a 256 < "$$input" && printf x)"; test "$${digest_output: -1}" = x; digest_output="$${digest_output%x}"; test "$${#digest_output}" -eq 68; digest="$${digest_output:0:64}"; [[ "$$digest" =~ ^[[:xdigit:]]{64}$$ ]]; test "$${digest_output:64}" = '  -'$$'\n'; printf '%s:%s\n' "$$input" "$$digest"; done; } | shasum -a 256 | awk 'NF == 2 && $$2 == "-" && length($$1) == 64 && $$1 !~ /[^[:xdigit:]]/ { print tolower($$1); count++ } END { if (count != 1) exit 1 }')
ifneq ($(.SHELLSTATUS),0)
$(error failed to compute the TCTI refresh producer identity)
endif
ifneq ($(words $(ORLIX_TCTI_ISA_MAINTAINER_REFRESH_PRODUCER_SHA256)),1)
$(error TCTI refresh producer identity must be one digest)
endif
ORLIX_TCTI_ISA_MAINTAINER_REFRESH_DEFINES := \
	-DORLIX_TCTI_TARGET_REFRESH_PRODUCER_SHA256=\"$(ORLIX_TCTI_ISA_MAINTAINER_REFRESH_PRODUCER_SHA256)\" \
	-DTARGET_MANIFEST_GENERATOR_NO_MAIN \
	-DTARGET_INSTRUCTION_ARTIFACT_GENERATOR_NO_MAIN \
	-DTARGET_FEATURE_ARTIFACT_GENERATOR_NO_MAIN \
	-DTARGET_FEATURE_FIELD_DOMAIN_BINDING_ARTIFACT_GENERATOR_NO_MAIN \
	-DTARGET_RUNTIME_CAPABILITY_COHORT_ARTIFACT_GENERATOR_NO_MAIN \
	-DTARGET_REGISTER_ARTIFACT_GENERATOR_NO_MAIN
ORLIX_TCTI_ISA_MAINTAINER_REFRESH_TEST_DEFINES := \
	$(ORLIX_TCTI_ISA_MAINTAINER_REFRESH_DEFINES) \
	-DORLIX_TCTI_TARGET_REFRESH_NO_MAIN \
	-DORLIX_TCTI_TARGET_REFRESH_TEST_CACHE
ORLIX_TCTI_ISA_MAINTAINER_REFRESH_SOURCES := \
	target_refresh.c target_artifact_publisher.c target_manifest_generator.c \
	target_inventory_import.c target_condition_serialization.c \
	target_instruction_artifact_generator.c target_feature_model.c \
	target_feature_artifact_generator.c target_feature_field_domain_binding.c \
	target_feature_sat.c target_feature_applicability_generator.c \
	target_feature_field_domain_binding_artifact_generator.c \
	target_runtime_capability_cohort_artifact_generator.c \
	target_register_model.c target_register_artifact_generator.c \
	target_arm_xml_package.c target_asl_availability.c \
	target_system_accessor_reconciliation.c

include $(ORLIX_TCTI_ISA_BUILD_TIME_ROOT)/instruction-artifact-contributors.mk

.PHONY: __tcti-isa-check __tcti-isa-refresh __tcti-isa-producer-identity-check __tcti-isa-system-accessor-reconciliation-check __tcti-instruction-source-check __tcti-operational-note-pipeline-test

__tcti-isa-producer-identity-check:
	@set -eu; \
	cd '$(ORLIX_TCTI_ISA_BUILD_TIME_ROOT)'; \
	digest="$$( set -euo pipefail; { for input in $(ORLIX_TCTI_ISA_MAINTAINER_REFRESH_IDENTITY_INPUTS); do test -f "$$input"; digest_output="$$(shasum -a 256 < "$$input" && printf x)"; test "$${digest_output: -1}" = x; digest_output="$${digest_output%x}"; test "$${#digest_output}" -eq 68; digest="$${digest_output:0:64}"; [[ "$$digest" =~ ^[[:xdigit:]]{64}$$ ]]; test "$${digest_output:64}" = '  -'$$'\n'; printf '%s:%s\n' "$$input" "$$digest"; done; } | shasum -a 256 | awk 'NF == 2 && $$2 == "-" && length($$1) == 64 && $$1 !~ /[^[:xdigit:]]/ { print tolower($$1); count++ } END { if (count != 1) exit 1 }' )"; \
	test "$$digest" = '$(ORLIX_TCTI_ISA_MAINTAINER_REFRESH_PRODUCER_SHA256)'; \
	test "$${#digest}" -eq 64; \
	case "$$digest" in *[!0-9a-f]*) exit 1;; esac; \
	echo "PASS TCTI refresh producer identity $$digest"

__tcti-isa-system-accessor-reconciliation-check:
	@test -n '$(ORLIX_AARCHMRS_REGISTERS)' || { echo 'ORLIX_AARCHMRS_REGISTERS must point to the pinned AARCHMRS Registers.json' >&2; exit 2; }
	@test -f '$(ORLIX_AARCHMRS_REGISTERS)' || { echo 'missing pinned AARCHMRS input: $(ORLIX_AARCHMRS_REGISTERS)' >&2; exit 2; }
	@mkdir -p '$(ORLIX_TCTI_ISA_MAINTAINER_OUT)'
	@$(ORLIX_TCTI_ISA_MAINTAINER_COMPILE) $(ORLIX_TCTI_ISA_MAINTAINER_CPPFLAGS) \
		target_register_model.c target_system_accessor_reconciliation.c \
		target_system_accessor_reconciliation_test.c \
		-o '$(ORLIX_TCTI_ISA_MAINTAINER_OUT)/target_system_accessor_reconciliation_test'
	@'$(ORLIX_TCTI_ISA_MAINTAINER_OUT)/target_system_accessor_reconciliation_test' \
		'$(ORLIX_AARCHMRS_REGISTERS)'

__tcti-instruction-source-check:
	@mkdir -p '$(ORLIX_TCTI_ISA_MAINTAINER_OUT)'
	@$(ORLIX_TCTI_ISA_MAINTAINER_COMPILE) \
		$(ORLIX_TCTI_ISA_MAINTAINER_CPPFLAGS) \
		target_inventory_import.c target_inventory_import_test.c \
		-o '$(ORLIX_TCTI_ISA_MAINTAINER_OUT)/target_inventory_import_test'
	@'$(ORLIX_TCTI_ISA_MAINTAINER_OUT)/target_inventory_import_test' \
		'$(ORLIX_AARCHMRS_INSTRUCTIONS)'

__tcti-operational-note-pipeline-test:
	@mkdir -p '$(ORLIX_TCTI_ISA_MAINTAINER_OUT)'
	@$(RM) \
		'$(ORLIX_TCTI_ISA_MAINTAINER_OUT)/operational-note-Instructions.json' \
		'$(ORLIX_TCTI_ISA_MAINTAINER_OUT)/target_instruction_artifact_generated.h'
	@$(ORLIX_TCTI_ISA_MAINTAINER_COMPILE) \
		-DTARGET_INSTRUCTION_ARTIFACT_GENERATOR_NO_MAIN \
		$(ORLIX_TCTI_ISA_MAINTAINER_CPPFLAGS) \
		$(ORLIX_TCTI_ISA_MAINTAINER_CONDITION_CPPFLAGS) \
		target_inventory_import.c target_condition_serialization.c \
		target_instruction_artifact_generator.c \
		target_operational_note_pipeline_test.c \
		-o '$(ORLIX_TCTI_ISA_MAINTAINER_OUT)/target_operational_note_pipeline_generate_test'
	@'$(ORLIX_TCTI_ISA_MAINTAINER_OUT)/target_operational_note_pipeline_generate_test' \
		'$(ORLIX_AARCHMRS_INSTRUCTIONS)' \
		'$(ORLIX_TCTI_ISA_MAINTAINER_OUT)/operational-note-Instructions.json' \
		'$(ORLIX_TCTI_ISA_MAINTAINER_OUT)/target_instruction_artifact_generated.h'
	@$(ORLIX_TCTI_ISA_MAINTAINER_COMPILE) \
		-DORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_EXTERNAL_GENERATED \
		-I'$(ORLIX_TCTI_ISA_MAINTAINER_OUT)' \
		$(ORLIX_TCTI_ISA_MAINTAINER_CPPFLAGS) \
		$(ORLIX_TCTI_ISA_MAINTAINER_CONDITION_CPPFLAGS) \
		target_inventory_import.c \
		'$(ORLIX_TCTI_ISA_TEST_ROOT)/target_instruction_artifact.c' \
		'$(ORLIX_TCTI_ISA_TEST_ROOT)/target_proof_registry.c' \
		target_operational_note_pipeline_validate_test.c \
		-o '$(ORLIX_TCTI_ISA_MAINTAINER_OUT)/target_operational_note_pipeline_validate_test'
	@'$(ORLIX_TCTI_ISA_MAINTAINER_OUT)/target_operational_note_pipeline_validate_test' \
		'$(ORLIX_TCTI_ISA_MAINTAINER_OUT)/operational-note-Instructions.json'
	@$(RM) \
		'$(ORLIX_TCTI_ISA_MAINTAINER_OUT)/operational-note-Instructions.json' \
		'$(ORLIX_TCTI_ISA_MAINTAINER_OUT)/target_instruction_artifact_generated.h'

__tcti-isa-check:
	@set -eu; \
	for input in \
		"ORLIX_AARCHMRS_INSTRUCTIONS:$(ORLIX_AARCHMRS_INSTRUCTIONS):Instructions" \
		"ORLIX_AARCHMRS_FEATURES:$(ORLIX_AARCHMRS_FEATURES):Features" \
		"ORLIX_AARCHMRS_REGISTERS:$(ORLIX_AARCHMRS_REGISTERS):Registers"; do \
		variable="$${input%%:*}"; remainder="$${input#*:}"; \
		path="$${remainder%%:*}"; label="$${remainder#*:}"; \
		test -n "$$path" || { echo "$$variable must point to the pinned AARCHMRS $$label.json" >&2; exit 2; }; \
		test -f "$$path" || { echo "missing pinned AARCHMRS input: $$path" >&2; exit 2; }; \
	done; \
	test -n '$(ORLIX_A64_ISA_XML_ARCHIVE)' || { echo "ORLIX_A64_ISA_XML_ARCHIVE must point to ISA_A64_xml_A_profile-2026-06.tar.gz" >&2; exit 2; }; \
	test -f '$(ORLIX_A64_ISA_XML_ARCHIVE)' || { echo "missing pinned Arm A-profile ISA XML archive: $(ORLIX_A64_ISA_XML_ARCHIVE)" >&2; exit 2; }; \
	test -n '$(ORLIX_A64_ISA_XML_RELEASE)' || { echo "ORLIX_A64_ISA_XML_RELEASE must point to extracted ISA_A64_xml_A_profile-2026-06" >&2; exit 2; }; \
	test -d '$(ORLIX_A64_ISA_XML_RELEASE)' || { echo "missing extracted Arm A-profile ISA XML release: $(ORLIX_A64_ISA_XML_RELEASE)" >&2; exit 2; }; \
	mkdir -p '$(ORLIX_TCTI_ISA_MAINTAINER_OUT)'
	@$(ORLIX_TCTI_ISA_MAINTAINER_COMPILE) \
		target_artifact_publisher.c target_arm_xml_package.c target_arm_xml_package_test.c \
		-o '$(ORLIX_TCTI_ISA_MAINTAINER_OUT)/target_arm_xml_package_test'
	@'$(ORLIX_TCTI_ISA_MAINTAINER_OUT)/target_arm_xml_package_test' \
		'$(ORLIX_A64_ISA_XML_ARCHIVE)' '$(ORLIX_A64_ISA_XML_RELEASE)'
	@$(ORLIX_TCTI_ISA_MAINTAINER_COMPILE) $(ORLIX_TCTI_ISA_MAINTAINER_CPPFLAGS) \
		target_inventory_import.c target_inventory_import_test.c \
		-o '$(ORLIX_TCTI_ISA_MAINTAINER_OUT)/target_inventory_import_test'
	@'$(ORLIX_TCTI_ISA_MAINTAINER_OUT)/target_inventory_import_test' '$(ORLIX_AARCHMRS_INSTRUCTIONS)'
	@$(ORLIX_TCTI_ISA_MAINTAINER_COMPILE) $(ORLIX_TCTI_ISA_MAINTAINER_CPPFLAGS) \
		target_inventory_import.c target_classification_generator.c \
		-o '$(ORLIX_TCTI_ISA_MAINTAINER_OUT)/target_classification_generator'
	@'$(ORLIX_TCTI_ISA_MAINTAINER_OUT)/target_classification_generator' \
		'$(ORLIX_AARCHMRS_INSTRUCTIONS)' \
		> '$(ORLIX_TCTI_ISA_MAINTAINER_OUT)/target_classification.generated.def'
	@$(ORLIX_TCTI_ISA_MAINTAINER_COMPILE) $(ORLIX_TCTI_ISA_MAINTAINER_CPPFLAGS) \
		target_inventory_import.c target_artifact_publisher.c target_arm_xml_package.c \
		target_asl_availability.c target_asl_availability_test.c \
		-o '$(ORLIX_TCTI_ISA_MAINTAINER_OUT)/target_asl_availability_test'
	@'$(ORLIX_TCTI_ISA_MAINTAINER_OUT)/target_asl_availability_test' \
		'$(ORLIX_AARCHMRS_INSTRUCTIONS)' '$(ORLIX_A64_ISA_XML_ARCHIVE)' '$(ORLIX_A64_ISA_XML_RELEASE)'
	@$(ORLIX_TCTI_ISA_MAINTAINER_COMPILE) $(ORLIX_TCTI_ISA_MAINTAINER_CPPFLAGS) \
		target_feature_model.c target_feature_model_test.c \
		-o '$(ORLIX_TCTI_ISA_MAINTAINER_OUT)/target_feature_model_test'
	@'$(ORLIX_TCTI_ISA_MAINTAINER_OUT)/target_feature_model_test' '$(ORLIX_AARCHMRS_FEATURES)'
	@$(ORLIX_TCTI_ISA_MAINTAINER_COMPILE) $(ORLIX_TCTI_ISA_MAINTAINER_CPPFLAGS) \
		target_feature_model.c target_register_model.c target_feature_field_domain_binding.c \
		target_feature_field_domain_binding_test.c \
		-o '$(ORLIX_TCTI_ISA_MAINTAINER_OUT)/target_feature_field_domain_binding_test'
	@'$(ORLIX_TCTI_ISA_MAINTAINER_OUT)/target_feature_field_domain_binding_test' \
		'$(ORLIX_AARCHMRS_FEATURES)' '$(ORLIX_AARCHMRS_REGISTERS)'
	@$(ORLIX_TCTI_ISA_MAINTAINER_COMPILE) $(ORLIX_TCTI_ISA_MAINTAINER_CPPFLAGS) \
		-DTARGET_FEATURE_FIELD_DOMAIN_BINDING_ARTIFACT_GENERATOR_NO_MAIN \
		target_feature_model.c target_register_model.c target_feature_field_domain_binding.c \
		target_feature_field_domain_binding_artifact_generator.c \
		target_feature_field_domain_binding_artifact_generator_test.c \
		-o '$(ORLIX_TCTI_ISA_MAINTAINER_OUT)/target_feature_field_domain_binding_artifact_generator_test'
	@'$(ORLIX_TCTI_ISA_MAINTAINER_OUT)/target_feature_field_domain_binding_artifact_generator_test' \
		'$(ORLIX_AARCHMRS_FEATURES)' '$(ORLIX_AARCHMRS_REGISTERS)'
	@$(ORLIX_TCTI_ISA_MAINTAINER_COMPILE) -O2 $(ORLIX_TCTI_ISA_MAINTAINER_CPPFLAGS) \
		target_inventory_import.c target_feature_model.c target_register_model.c \
		target_feature_field_domain_binding.c target_feature_sat.c \
		target_feature_sat_test.c \
		-o '$(ORLIX_TCTI_ISA_MAINTAINER_OUT)/target_feature_sat_test'
	@'$(ORLIX_TCTI_ISA_MAINTAINER_OUT)/target_feature_sat_test' \
		'$(ORLIX_AARCHMRS_FEATURES)' '$(ORLIX_AARCHMRS_INSTRUCTIONS)' \
		'$(ORLIX_AARCHMRS_REGISTERS)'
	@$(ORLIX_TCTI_ISA_MAINTAINER_COMPILE) $(ORLIX_TCTI_ISA_MAINTAINER_CPPFLAGS) \
		$(ORLIX_TCTI_ISA_MAINTAINER_CONDITION_CPPFLAGS) \
		target_feature_model.c target_condition_serialization.c target_feature_sat.c \
		target_feature_applicability_generator.c \
		target_feature_applicability_generator_test.c \
		-o '$(ORLIX_TCTI_ISA_MAINTAINER_OUT)/target_feature_applicability_generator_test'
	@'$(ORLIX_TCTI_ISA_MAINTAINER_OUT)/target_feature_applicability_generator_test'
	@$(ORLIX_TCTI_ISA_MAINTAINER_COMPILE) $(ORLIX_TCTI_ISA_MAINTAINER_CPPFLAGS) \
		target_inventory_import.c target_feature_model.c \
		target_runtime_capability_cohort_artifact_generator_test.c \
		-o '$(ORLIX_TCTI_ISA_MAINTAINER_OUT)/target_runtime_capability_cohort_artifact_generator_test'
	@'$(ORLIX_TCTI_ISA_MAINTAINER_OUT)/target_runtime_capability_cohort_artifact_generator_test' \
		'$(ORLIX_AARCHMRS_INSTRUCTIONS)' '$(ORLIX_AARCHMRS_FEATURES)'
	@$(ORLIX_TCTI_ISA_MAINTAINER_COMPILE) $(ORLIX_TCTI_ISA_MAINTAINER_CPPFLAGS) \
		target_register_model.c target_register_model_test.c \
		-o '$(ORLIX_TCTI_ISA_MAINTAINER_OUT)/target_register_model_test'
	@'$(ORLIX_TCTI_ISA_MAINTAINER_OUT)/target_register_model_test' '$(ORLIX_AARCHMRS_REGISTERS)'
	@$(ORLIX_TCTI_ISA_MAINTAINER_COMPILE) $(ORLIX_TCTI_ISA_MAINTAINER_CPPFLAGS) \
		target_register_model.c target_register_artifact_generator_test.c \
		-o '$(ORLIX_TCTI_ISA_MAINTAINER_OUT)/target_register_artifact_generator_test'
	@'$(ORLIX_TCTI_ISA_MAINTAINER_OUT)/target_register_artifact_generator_test' '$(ORLIX_AARCHMRS_REGISTERS)'
	@$(ORLIX_TCTI_ISA_MAINTAINER_COMPILE) $(ORLIX_TCTI_ISA_MAINTAINER_CPPFLAGS) \
		target_register_model.c target_system_accessor_reconciliation.c \
		target_system_accessor_reconciliation_test.c \
		-o '$(ORLIX_TCTI_ISA_MAINTAINER_OUT)/target_system_accessor_reconciliation_test'
	@'$(ORLIX_TCTI_ISA_MAINTAINER_OUT)/target_system_accessor_reconciliation_test' '$(ORLIX_AARCHMRS_REGISTERS)'
	@$(ORLIX_TCTI_ISA_MAINTAINER_COMPILE) \
		target_register_model.c target_system_accessor_reconciliation.c \
		target_system_accessor_reconciliation_generator.c \
		-o '$(ORLIX_TCTI_ISA_MAINTAINER_OUT)/target_system_accessor_reconciliation_generator'
	@$(ORLIX_TCTI_ISA_MAINTAINER_COMPILE) $(ORLIX_TCTI_ISA_MAINTAINER_CPPFLAGS) \
		target_artifact_publisher_test.c \
		-o '$(ORLIX_TCTI_ISA_MAINTAINER_OUT)/target_artifact_publisher_test'
	@'$(ORLIX_TCTI_ISA_MAINTAINER_OUT)/target_artifact_publisher_test'
	@$(ORLIX_TCTI_ISA_MAINTAINER_COMPILE) -O2 $(ORLIX_TCTI_ISA_MAINTAINER_CPPFLAGS) \
		$(ORLIX_TCTI_ISA_MAINTAINER_CONDITION_CPPFLAGS) \
		$(ORLIX_TCTI_ISA_MAINTAINER_REFRESH_TEST_DEFINES) \
		target_refresh_test.c $(ORLIX_TCTI_ISA_MAINTAINER_REFRESH_SOURCES) \
		-o '$(ORLIX_TCTI_ISA_MAINTAINER_OUT)/target_refresh_test'
	@'$(ORLIX_TCTI_ISA_MAINTAINER_OUT)/target_refresh_test' \
		'$(ORLIX_AARCHMRS_INSTRUCTIONS)' '$(ORLIX_AARCHMRS_FEATURES)' '$(ORLIX_AARCHMRS_REGISTERS)' \
		'$(ORLIX_A64_ISA_XML_ARCHIVE)' '$(ORLIX_A64_ISA_XML_RELEASE)'

__tcti-isa-refresh: __tcti-isa-check
	@$(ORLIX_TCTI_ISA_MAINTAINER_COMPILE) -O2 $(ORLIX_TCTI_ISA_MAINTAINER_CPPFLAGS) \
		$(ORLIX_TCTI_ISA_MAINTAINER_CONDITION_CPPFLAGS) \
		$(ORLIX_TCTI_ISA_MAINTAINER_REFRESH_DEFINES) \
		target_refresh.c $(filter-out target_refresh.c,$(ORLIX_TCTI_ISA_MAINTAINER_REFRESH_SOURCES)) \
		-o '$(ORLIX_TCTI_ISA_MAINTAINER_OUT)/target_refresh'
	@'$(ORLIX_TCTI_ISA_MAINTAINER_OUT)/target_refresh' \
		'$(ORLIX_TCTI_ISA_CANONICAL_ROOT)' '$(ORLIX_AARCHMRS_INSTRUCTIONS)' \
		'$(ORLIX_AARCHMRS_FEATURES)' '$(ORLIX_AARCHMRS_REGISTERS)' \
		'$(ORLIX_A64_ISA_XML_ARCHIVE)' '$(ORLIX_A64_ISA_XML_RELEASE)'
