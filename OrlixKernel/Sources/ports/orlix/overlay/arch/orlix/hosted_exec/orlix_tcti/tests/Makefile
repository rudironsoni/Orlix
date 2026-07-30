# SPDX-License-Identifier: GPL-2.0-only

include $(ORLIX_TCTI_PROOF_REGISTRY_PROVENANCE_MAKEFILE)

# These variables are the direct Kbuild consumers of durable ISA artifacts.
# Keep the paths in the real prerequisite lists so the source contract can
# compare the canonical contributors with the consumer set.
override ORLIX_TCTI_DIRECT_ARTIFACT_INPUT_VARIABLES := \
	ORLIX_TCTI_INVENTORY_ARTIFACT_INPUTS \
	ORLIX_TCTI_TARGET_INVENTORY_ARTIFACT_INPUTS \
	ORLIX_TCTI_RUNTIME_PROJECTION_ARTIFACT_INPUTS \
	ORLIX_TCTI_RUNTIME_COHORT_ARTIFACT_INPUTS \
	ORLIX_TCTI_EXECUTION_SLICE_MAP_ARTIFACT_INPUTS \
	ORLIX_TCTI_FEATURE_ARTIFACT_INPUTS \
	ORLIX_TCTI_FEATURE_FIELD_DOMAIN_ARTIFACT_INPUTS \
	ORLIX_TCTI_FEATURE_APPLICABILITY_ARTIFACT_INPUTS \
	ORLIX_TCTI_PROOF_REGISTRY_ARTIFACT_INPUTS
$(foreach variable,$(ORLIX_TCTI_DIRECT_ARTIFACT_INPUT_VARIABLES),$(if $(filter undefined file,$(origin $(variable))),,$(error $(variable) cannot be overridden; origin=$(origin $(variable)))))

override ORLIX_TCTI_INVENTORY_ARTIFACT_INPUTS = \
	$(src)/../isa/inventory.def \
	$(src)/../isa/generations/current/source_manifest.def \
	$(src)/../isa/target_classification.def
override ORLIX_TCTI_TARGET_INVENTORY_ARTIFACT_INPUTS = \
	$(src)/../isa/generations/current/source_manifest.def \
	$(src)/../isa/target_classification.def \
	$(src)/../isa/generations/current/target_system_accessor_reconciliation.def
override ORLIX_TCTI_RUNTIME_PROJECTION_ARTIFACT_INPUTS = \
	$(src)/../isa/generations/current/target_runtime_capability_cohort_artifact.def
override ORLIX_TCTI_RUNTIME_COHORT_ARTIFACT_INPUTS = \
	$(src)/../isa/generations/current/target_runtime_capability_cohort_artifact.def \
	$(src)/../isa/generations/current/target_feature_artifact.def
override ORLIX_TCTI_EXECUTION_SLICE_MAP_ARTIFACT_INPUTS = \
	$(src)/../isa/target_execution_slice_map.def \
	$(src)/../isa/generations/current/source_manifest.def
override ORLIX_TCTI_FEATURE_ARTIFACT_INPUTS = \
	$(src)/../isa/generations/current/target_feature_artifact.def
override ORLIX_TCTI_FEATURE_FIELD_DOMAIN_ARTIFACT_INPUTS = \
	$(src)/../isa/generations/current/target_feature_field_domain_binding.def \
	$(src)/../isa/generations/current/target_feature_artifact.def
override ORLIX_TCTI_FEATURE_APPLICABILITY_ARTIFACT_INPUTS = \
	$(src)/../isa/generations/current/target_feature_applicability.def
override ORLIX_TCTI_PROOF_REGISTRY_ARTIFACT_INPUTS = \
	$(src)/../isa/source_bound_proof.def \
	$(src)/../isa/proof_registry_projection.def

obj-$(CONFIG_ORLIX_TCTI_KUNIT_TEST) += orlix_tcti_test.o
orlix_tcti_test-y := orlix_tcti_decode_test.o runtime_projection.o \
	target_instruction_artifact.o \
	../mops_provenance.o \
	../isa/target_ordinal_ledger.o \
	target_ordinal_ledger_test.o \
	target_feature_artifact.o \
	target_feature_domain.o \
	target_feature_field_domain_binding_artifact.o \
	target_feature_applicability_artifact.o \
	target_runtime_capability_cohort_artifact.o \
	target_execution_slice_map.o \
	target_proof_registry.o \
	target_native_proof_registry.o \
	target_execution_slice_map_test.o \
	orlix_tcti_atomic_memory_test.o orlix_tcti_lse_decode_test.o orlix_tcti_uaccess_test.o \
	orlix_tcti_cssc_min_max_immediate_test.o \
	orlix_tcti_crypto_decode_boundary_test.o \
	orlix_tcti_crc32_decode_boundary_test.o \
	orlix_tcti_crc32_source_bound_test.o \
	orlix_tcti_crypto_extension_decode_boundary_test.o \
	orlix_tcti_atomic_128_rmw_test.o \
	target_lse_operation_catalog.o \
	orlix_tcti_lse_source_bound_test.o \
	orlix_tcti_lse128_noncas_test.o \
	orlix_tcti_lse128_resume_test.o \
	orlix_tcti_lse_resume_production_test.o \
	orlix_tcti_lse_scalar_rmw_resume_test.o \
	orlix_tcti_exclusive_resume_production_test.o \
	target_native_proof_contract.o \
	target_proof_ingestion.o \
	orlix_tcti_native_observation_test.o \
	orlix_tcti_native_observation_production_test.o \
	orlix_tcti_tls_repair_test.o \
	orlix_tcti_add_sub_immediate_test.o \
	orlix_tcti_logical_shifted_register_test.o \
	orlix_tcti_logical_immediate_source_bound_test.o \
	orlix_tcti_move_wide_source_bound_test.o \
	orlix_tcti_add_sub_register_source_bound_test.o \
	orlix_tcti_bitfield_extract_source_bound_test.o \
	orlix_tcti_scalar_bitops_source_bound_test.o \
	orlix_tcti_variable_shift_source_bound_test.o \
	orlix_tcti_advsimd_narrow_widen_source_bound_test.o \
	orlix_tcti_ordinary_single_load_store_source_bound_test.o \
	orlix_tcti_pair_load_store_source_bound_test.o \
	orlix_tcti_load_literal_resume_regression_test.o \
	orlix_tcti_branch_control_source_bound_test.o \
	orlix_tcti_source_leaf_classification_test.o \
	orlix_tcti_translation_change_source_bound_test.o \
	orlix_tcti_scalar_fp_semantics_test.o \
	orlix_tcti_baseline_decoder_regression_test.o \
	orlix_tcti_gadget_program_boundary_test.o \
	orlix_tcti_lse_caspal_atomicity_test.o \
	orlix_tcti_kthread_handoff_test.o \
	orlix_tcti_mapping_invalidation_test.o \
	orlix_tcti_pmull_semantic_test.o \
	orlix_tcti_integer_conditional_source_bound_test.o \
	orlix_tcti_flag_manipulation_source_bound_test.o \
	orlix_tcti_sve_state_test.o \
	orlix_tcti_sve_decode_test.o \
	orlix_tcti_sve_resume_test.o \
	orlix_tcti_advsimd_modified_immediate_regression_test.o \
	orlix_tcti_advsimd_logical_source_bound_test.o \
	orlix_tcti_advsimd_compare_register_source_bound_test.o \
	orlix_tcti_advsimd_add_sub_source_bound_test.o \
	orlix_tcti_advsimd_mul_source_bound_test.o \
	orlix_tcti_advsimd_integer_min_max_source_bound_test.o \
	orlix_tcti_advsimd_integer_minmax_reduction_source_bound_test.o \
	orlix_tcti_advsimd_pairwise_minmax_source_bound_test.o \
	orlix_tcti_advsimd_halving_source_bound_test.o \
	orlix_tcti_advsimd_compare_zero_source_bound_test.o \
	orlix_tcti_advsimd_shift_right_source_bound_test.o \
	orlix_tcti_advsimd_permute_source_bound_test.o \
	orlix_tcti_advsimd_table_lookup_source_bound_test.o \
	orlix_tcti_simd_fp_register_offset_source_bound_test.o \
	orlix_tcti_advsimd_fp_arithmetic_source_bound_test.o \
	orlix_tcti_advsimd_structure_load_store_source_bound_test.o \
	orlix_tcti_pauth_bti_gcs_obligation_test.o \
	pauth_qemu_oracle.o \
	orlix_tcti_sme_state_contract_test.o \
	orlix_tcti_mops_provenance_test.o \
	orlix_tcti_mops_copy_test.o

hostprogs := gen_inventory target_isa_kbuild_generator
always-y += inventory.h target_inventory.h target_proof_registry_provenance.h

quiet_cmd_orlix_tcti_inventory = GEN     $@
      cmd_orlix_tcti_inventory = $(obj)/gen_inventory $@

quiet_cmd_orlix_tcti_target_inventory = GEN     $@
      cmd_orlix_tcti_target_inventory = $(obj)/target_isa_kbuild_generator > $@.tmp && \
				   mv $@.tmp $@

targets += inventory.h target_inventory.h
clean-files += inventory.h target_inventory.h target_inventory.h.tmp

$(obj)/inventory.h: $(obj)/gen_inventory \
			    $(ORLIX_TCTI_INVENTORY_ARTIFACT_INPUTS) FORCE
	$(call if_changed,orlix_tcti_inventory)

$(obj)/target_inventory.h: $(obj)/target_isa_kbuild_generator \
			   $(ORLIX_TCTI_TARGET_INVENTORY_ARTIFACT_INPUTS) FORCE
	$(call if_changed,orlix_tcti_target_inventory)

$(obj)/orlix_tcti_decode_test.o: $(obj)/inventory.h $(obj)/target_inventory.h
$(obj)/orlix_tcti_source_leaf_classification_test.o: $(obj)/target_inventory.h
$(obj)/orlix_tcti_test.o: $(obj)/inventory.h $(obj)/target_inventory.h \
	$(obj)/target_proof_registry_provenance.h
$(obj)/runtime_projection.o: $(obj)/target_inventory.h \
	$(ORLIX_TCTI_RUNTIME_PROJECTION_ARTIFACT_INPUTS)
$(obj)/target_runtime_capability_cohort_artifact.o: \
	$(ORLIX_TCTI_RUNTIME_COHORT_ARTIFACT_INPUTS)
$(obj)/target_execution_slice_map.o: \
	$(ORLIX_TCTI_EXECUTION_SLICE_MAP_ARTIFACT_INPUTS)
$(obj)/target_feature_artifact.o: $(ORLIX_TCTI_FEATURE_ARTIFACT_INPUTS)
$(obj)/target_feature_field_domain_binding_artifact.o: \
	$(ORLIX_TCTI_FEATURE_FIELD_DOMAIN_ARTIFACT_INPUTS)
$(obj)/target_feature_applicability_artifact.o: $(ORLIX_TCTI_FEATURE_APPLICABILITY_ARTIFACT_INPUTS)
$(obj)/target_proof_registry_provenance.h: $(src)/orlix_tcti_decode_test.c $(src)/orlix_tcti_system_accessor_partition_test.h $(src)/Makefile FORCE
	@$(call orlix_tcti_write_proof_registry_provenance,$@,$(src)/orlix_tcti_decode_test.c,$(src)/orlix_tcti_system_accessor_partition_test.h,$(src)/Makefile)

$(obj)/target_proof_registry.o: $(ORLIX_TCTI_PROOF_REGISTRY_ARTIFACT_INPUTS) \
	$(obj)/target_proof_registry_provenance.h
$(obj)/target_proof_registry_test.o: \
	$(src)/proof_registry_projection_synthetic_source_bound.def

ccflags-y += -I$(obj)
