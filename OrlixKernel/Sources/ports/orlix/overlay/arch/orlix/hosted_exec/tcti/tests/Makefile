# SPDX-License-Identifier: GPL-2.0-only

obj-$(CONFIG_ORLIX_TCTI_KUNIT_TEST) += orlix_tcti_test.o
orlix_tcti_test-y := tcti_decode_test.o runtime_projection.o \
	target_instruction_artifact.o \
	../mops_provenance.o \
	../isa/target_ordinal_ledger.o \
	target_instruction_artifact.o \
	target_ordinal_ledger_test.o \
	target_instruction_artifact.o \
	target_feature_artifact.o \
	target_instruction_artifact.o \
	target_runtime_capability_cohort_artifact.o \
	tcti_atomic_memory_test.o tcti_lse_decode_test.o tcti_uaccess_test.o \
	tcti_cssc_min_max_immediate_test.o \
	tcti_crypto_decode_boundary_test.o \
	tcti_crc32_decode_boundary_test.o \
	tcti_crc32_source_bound_test.o \
	tcti_crypto_extension_decode_boundary_test.o \
	tcti_atomic_128_rmw_test.o \
	target_lse_operation_catalog.o \
	tcti_lse_source_bound_test.o \
	tcti_lse128_noncas_test.o \
	tcti_lse128_resume_test.o \
	tcti_lse_resume_production_test.o \
	tcti_lse_scalar_rmw_resume_test.o \
	tcti_exclusive_resume_production_test.o \
	tcti_native_observation.o \
	tcti_native_observation_test.o \
	tcti_add_sub_immediate_test.o \
	tcti_logical_shifted_register_test.o \
	tcti_logical_immediate_source_bound_test.o \
	tcti_move_wide_source_bound_test.o \
	tcti_add_sub_register_source_bound_test.o \
	tcti_bitfield_extract_source_bound_test.o \
	tcti_scalar_bitops_source_bound_test.o \
	tcti_variable_shift_source_bound_test.o \
	tcti_advsimd_narrow_widen_source_bound_test.o \
	tcti_ordinary_single_load_store_source_bound_test.o \
	tcti_pair_load_store_source_bound_test.o \
	tcti_load_literal_resume_regression_test.o \
	tcti_branch_control_source_bound_test.o \
	tcti_source_leaf_classification_test.o \
	tcti_scalar_fp_semantics_test.o \
	tcti_baseline_decoder_regression_test.o \
	tcti_gadget_program_boundary_test.o \
	tcti_lse_caspal_atomicity_test.o \
	tcti_kthread_handoff_test.o \
	tcti_mapping_invalidation_test.o \
	tcti_pmull_semantic_test.o \
	tcti_integer_conditional_source_bound_test.o \
	tcti_sve_state_test.o \
	tcti_sve_decode_test.o \
	tcti_sve_resume_test.o \
	tcti_advsimd_modified_immediate_regression_test.o \
	tcti_advsimd_add_sub_source_bound_test.o \
	tcti_advsimd_halving_source_bound_test.o \
	tcti_advsimd_shift_right_source_bound_test.o \
	tcti_advsimd_fp_arithmetic_source_bound_test.o \
	tcti_advsimd_structure_load_store_source_bound_test.o \
	tcti_pauth_bti_gcs_obligation_test.o \
	tcti_sme_state_contract_test.o \
	tcti_mops_provenance_test.o

hostprogs := gen_inventory target_isa_kbuild_generator

quiet_cmd_tcti_inventory = GEN     $@
      cmd_tcti_inventory = $(obj)/gen_inventory $@

quiet_cmd_tcti_target_inventory = GEN     $@
      cmd_tcti_target_inventory = $(obj)/target_isa_kbuild_generator > $@.tmp && \
				   mv $@.tmp $@

targets += inventory.h target_inventory.h
clean-files += inventory.h target_inventory.h target_inventory.h.tmp

$(obj)/inventory.h: $(obj)/gen_inventory \
			    $(src)/../isa/inventory.def \
			    $(src)/../isa/source_manifest.def \
			    $(src)/../isa/target_classification.def FORCE
	$(call if_changed,tcti_inventory)

$(obj)/target_inventory.h: $(obj)/target_isa_kbuild_generator \
			   $(src)/../isa/source_manifest.def \
			   $(src)/../isa/target_classification.def \
			   $(src)/../isa/target_system_accessor_reconciliation.def FORCE
	$(call if_changed,tcti_target_inventory)

$(obj)/tcti_decode_test.o: $(obj)/inventory.h $(obj)/target_inventory.h
$(obj)/runtime_projection.o: $(obj)/target_inventory.h \
	$(src)/../isa/target_runtime_capability_cohort_artifact.def
$(obj)/target_runtime_capability_cohort_artifact.o: \
	$(src)/../isa/target_runtime_capability_cohort_artifact.def \
	$(src)/../isa/target_feature_artifact.def
$(obj)/target_feature_artifact.o: $(src)/../isa/target_feature_artifact.def

ccflags-y += -I$(obj)
