# SPDX-License-Identifier: GPL-2.0-only

obj-$(CONFIG_ORLIX_TCTI_KUNIT_TEST) += orlix_tcti_test.o
orlix_tcti_test-y := orlix_tcti_decode_test.o runtime_projection.o \
	target_instruction_artifact.o \
	../mops_provenance.o \
	../isa/target_ordinal_ledger.o \
	target_ordinal_ledger_test.o \
	target_feature_artifact.o \
	target_runtime_capability_cohort_artifact.o \
	target_execution_slice_map.o \
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
	orlix_tcti_native_observation.o \
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
	orlix_tcti_scalar_fp_semantics_test.o \
	orlix_tcti_baseline_decoder_regression_test.o \
	orlix_tcti_gadget_program_boundary_test.o \
	orlix_tcti_lse_caspal_atomicity_test.o \
	orlix_tcti_kthread_handoff_test.o \
	orlix_tcti_mapping_invalidation_test.o \
	orlix_tcti_pmull_semantic_test.o \
	orlix_tcti_integer_conditional_source_bound_test.o \
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
	orlix_tcti_sme_state_contract_test.o \
	orlix_tcti_mops_provenance_test.o

hostprogs := gen_inventory target_isa_kbuild_generator

quiet_cmd_orlix_tcti_inventory = GEN     $@
      cmd_orlix_tcti_inventory = $(obj)/gen_inventory $@

quiet_cmd_orlix_tcti_target_inventory = GEN     $@
      cmd_orlix_tcti_target_inventory = $(obj)/target_isa_kbuild_generator > $@.tmp && \
				   mv $@.tmp $@

targets += inventory.h target_inventory.h
clean-files += inventory.h target_inventory.h target_inventory.h.tmp

$(obj)/inventory.h: $(obj)/gen_inventory \
			    $(src)/../isa/inventory.def \
			    $(src)/../isa/generations/current/source_manifest.def \
			    $(src)/../isa/target_classification.def FORCE
	$(call if_changed,orlix_tcti_inventory)

$(obj)/target_inventory.h: $(obj)/target_isa_kbuild_generator \
			   $(src)/../isa/generations/current/source_manifest.def \
			   $(src)/../isa/target_classification.def \
			   $(src)/../isa/generations/current/target_system_accessor_reconciliation.def FORCE
	$(call if_changed,orlix_tcti_target_inventory)

$(obj)/orlix_tcti_decode_test.o: $(obj)/inventory.h $(obj)/target_inventory.h
$(obj)/runtime_projection.o: $(obj)/target_inventory.h \
	$(src)/../isa/generations/current/target_runtime_capability_cohort_artifact.def
$(obj)/target_runtime_capability_cohort_artifact.o: \
	$(src)/../isa/generations/current/target_runtime_capability_cohort_artifact.def \
	$(src)/../isa/generations/current/target_feature_artifact.def
$(obj)/target_execution_slice_map.o: \
	$(src)/../isa/target_execution_slice_map.def \
	$(src)/../isa/generations/current/source_manifest.def
$(obj)/target_feature_artifact.o: $(src)/../isa/generations/current/target_feature_artifact.def

ccflags-y += -I$(obj)
