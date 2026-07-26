/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_ISA_COVERAGE_H
#define ORLIX_TCTI_ISA_COVERAGE_H

#include <linux/types.h>

/*
 * Legacy local decoder-family regression catalog.
 *
 * A covered row means only that this named decoder family has the listed local
 * KUnit coverage. It does not classify or prove every pinned AARCHMRS leaf in
 * that family, define the 4,350-leaf completion target, or authorize HWCAP or
 * HWCAP2. The checked target inventory, completion audit, proof registry, and
 * runtime projection own those decisions.
 */
enum orlix_tcti_isa_extension {
	ORLIX_TCTI_ISA_BASE = 0,
	ORLIX_TCTI_ISA_FP,
	ORLIX_TCTI_ISA_ASIMD,
	ORLIX_TCTI_ISA_AES,
	ORLIX_TCTI_ISA_SHA,
	ORLIX_TCTI_ISA_SHA512,
	ORLIX_TCTI_ISA_SHA3,
	ORLIX_TCTI_ISA_SM3,
	ORLIX_TCTI_ISA_SM4,
	ORLIX_TCTI_ISA_PMULL,
	ORLIX_TCTI_ISA_CRC32,
	ORLIX_TCTI_ISA_EXTENSION_COUNT,
};

enum orlix_tcti_isa_local_coverage_status {
	ORLIX_TCTI_ISA_LOCAL_COVERAGE_PARTIAL = 0,
	ORLIX_TCTI_ISA_LOCAL_COVERAGE_COMPLETE,
};

#define ORLIX_TCTI_ISA_FAMILIES(X) \
	X(PC_RELATIVE_ADDRESSING, BASE, COMPLETE, "pc-relative addressing", "ORLIX_TCTI_DECODE_PC_RELATIVE_ADDRESS", "orlix_tcti_gadget_executes_complete_pc_relative_address_family") \
	X(ADD_SUB_IMMEDIATE, BASE, COMPLETE, "add/subtract immediate", "ORLIX_TCTI_DECODE_ADD_SUB_IMMEDIATE", "orlix_tcti_gadget_executes_complete_add_sub_immediate_family") \
	X(LOGICAL_IMMEDIATE, BASE, COMPLETE, "logical immediate", "ORLIX_TCTI_DECODE_LOGICAL_IMMEDIATE", "orlix_tcti_gadget_executes_complete_logical_immediate_family") \
	X(MOVE_WIDE_IMMEDIATE, BASE, COMPLETE, "move wide immediate", "ORLIX_TCTI_DECODE_MOVE_WIDE_IMMEDIATE", "orlix_tcti_gadget_executes_complete_move_wide_immediate_family") \
	X(BITFIELD, BASE, COMPLETE, "bitfield", "ORLIX_TCTI_DECODE_BITFIELD", "orlix_tcti_gadget_executes_complete_bitfield_family") \
	X(EXTRACT, BASE, COMPLETE, "extract", "ORLIX_TCTI_DECODE_EXTRACT", "orlix_tcti_gadget_executes_complete_extract_family") \
	X(UNCONDITIONAL_BRANCH_IMMEDIATE, BASE, COMPLETE, "unconditional branch immediate", "ORLIX_TCTI_DECODE_UNCONDITIONAL_BRANCH_IMMEDIATE", "orlix_tcti_gadget_executes_complete_unconditional_branch_immediate_family") \
	X(CONDITIONAL_BRANCH_IMMEDIATE, BASE, COMPLETE, "conditional branch immediate", "ORLIX_TCTI_DECODE_CONDITIONAL_BRANCH_IMMEDIATE", "orlix_tcti_gadget_executes_complete_conditional_branch_immediate_family") \
	X(COMPARE_BRANCH_IMMEDIATE, BASE, COMPLETE, "compare and branch immediate", "ORLIX_TCTI_DECODE_COMPARE_BRANCH_IMMEDIATE", "orlix_tcti_gadget_executes_complete_compare_branch_immediate_family") \
	X(TEST_BRANCH_IMMEDIATE, BASE, COMPLETE, "test and branch immediate", "ORLIX_TCTI_DECODE_TEST_BRANCH_IMMEDIATE", "orlix_tcti_gadget_executes_complete_test_branch_immediate_family") \
	X(EXCEPTION_GENERATION, BASE, COMPLETE, "exception generation", \
	  "ORLIX_TCTI_DECODE_SVC/ORLIX_TCTI_DECODE_BRK/ORLIX_TCTI_DECODE_HLT", \
	  "orlix_tcti_decode_covers_complete_exception_generation_family;" \
	  "orlix_tcti_resume_user_reports_syscall_and_register_state;" \
	  "orlix_tcti_resume_user_reports_breakpoint_and_register_state;" \
	  "orlix_tcti_resume_user_reports_hlt_as_undefined") \
	X(SYSTEM_AND_HINT, BASE, COMPLETE, "system, hint, and barrier", \
	  "ORLIX_TCTI_DECODE_HINT/ORLIX_TCTI_DECODE_SYSTEM_REGISTER/" \
	  "ORLIX_TCTI_DECODE_BARRIER/ORLIX_TCTI_DECODE_CACHE_MAINTENANCE", \
	  "orlix_tcti_decode_exhaustive_system_register_family/" \
	  "orlix_tcti_decode_exhaustive_hint_barrier_cache_family/" \
	  "orlix_tcti_engine_reports_structured_hint_yields/orlix_tcti_system_probe") \
	X(UNCONDITIONAL_BRANCH_REGISTER, BASE, COMPLETE, "unconditional branch register", "ORLIX_TCTI_DECODE_UNCONDITIONAL_BRANCH_REGISTER", "orlix_tcti_gadget_executes_complete_unconditional_branch_register_family") \
	X(CONDITIONAL_COMPARE, BASE, COMPLETE, "conditional compare", "ORLIX_TCTI_DECODE_CONDITIONAL_COMPARE", "orlix_tcti_gadget_executes_complete_conditional_compare_family") \
	X(CONDITIONAL_SELECT, BASE, COMPLETE, "conditional select", "ORLIX_TCTI_DECODE_CONDITIONAL_SELECT", "orlix_tcti_gadget_executes_complete_conditional_select_family") \
	X(LOGICAL_SHIFTED_REGISTER, BASE, COMPLETE, "logical shifted register", "ORLIX_TCTI_DECODE_LOGICAL_SHIFTED_REGISTER", "orlix_tcti_gadget_executes_complete_logical_shifted_register_family") \
	X(ADD_SUB_SHIFTED_REGISTER, BASE, COMPLETE, "add/subtract shifted register", "ORLIX_TCTI_DECODE_ADD_SUB_SHIFTED_REGISTER", "orlix_tcti_gadget_executes_complete_add_sub_shifted_register_family") \
	X(ADD_SUB_EXTENDED_REGISTER, BASE, COMPLETE, "add/subtract extended register", "ORLIX_TCTI_DECODE_ADD_SUB_EXTENDED_REGISTER", "orlix_tcti_gadget_executes_complete_add_sub_extended_register_family") \
	X(ADD_SUB_WITH_CARRY, BASE, COMPLETE, "add/subtract with carry", "ORLIX_TCTI_DECODE_ADD_SUB_WITH_CARRY", "orlix_tcti_gadget_executes_complete_add_sub_with_carry_family") \
	X(DATA_PROCESSING_1SOURCE, BASE, COMPLETE, "data processing one source", "ORLIX_TCTI_DECODE_DATA_PROCESSING_1SOURCE", "orlix_tcti_gadget_executes_complete_data_processing_1source_family") \
	X(DATA_PROCESSING_2SOURCE, BASE, COMPLETE, "data processing two source", "ORLIX_TCTI_DECODE_DATA_PROCESSING_2SOURCE", "orlix_tcti_gadget_executes_complete_data_processing_2source_family") \
	X(DATA_PROCESSING_3SOURCE, BASE, COMPLETE, "data processing three source", "ORLIX_TCTI_DECODE_MULTIPLY_ADD_SUB", "orlix_tcti_gadget_executes_complete_data_processing_3source_family") \
	X(LOAD_LITERAL, BASE, COMPLETE, "load register literal", \
	  "ORLIX_TCTI_DECODE_LOAD_LITERAL", \
	  "orlix_tcti_decode_exhaustive_load_literal_family;" \
	  "orlix_tcti_decode_load_literal_all_fields;" \
	  "orlix_tcti_decode_load_literal_fixed_mask_boundaries;" \
	  "orlix_tcti_gadget_executes_complete_load_literal_family;" \
	  "orlix_tcti_gadget_reports_load_literal_faults") \
	X(LOAD_STORE_PAIR, BASE, COMPLETE, "load/store register pair", \
	  "ORLIX_TCTI_DECODE_LOAD_STORE_PAIR", \
	  "orlix_tcti_decode_exhaustive_load_store_pair_family;" \
	  "orlix_tcti_decode_load_store_pair_all_register_fields;" \
	  "orlix_tcti_decode_load_store_pair_fixed_mask_boundaries;" \
	  "orlix_tcti_gadget_executes_complete_load_store_pair_family;" \
	  "orlix_tcti_gadget_reports_load_store_pair_second_fault") \
	X(LOAD_STORE_REGISTER, BASE, COMPLETE, "load/store register", \
	  "ORLIX_TCTI_DECODE_LOAD_STORE_*/ORLIX_TCTI_DECODE_HINT", \
	  "orlix_tcti_decode_exhaustive_load_store_unsigned_immediate_family;" \
	  "orlix_tcti_decode_exhaustive_load_store_signed_immediate_family;" \
	  "orlix_tcti_decode_exhaustive_load_store_register_offset_family;" \
	  "orlix_tcti_decode_load_store_all_register_fields;" \
	  "orlix_tcti_gadget_executes_complete_load_store_register_family") \
	X(LOAD_STORE_EXCLUSIVE, BASE, COMPLETE, "load/store exclusive", \
	  "ORLIX_TCTI_DECODE_LOAD_STORE_EXCLUSIVE", \
	  "orlix_tcti_decode_exhaustive_load_store_exclusive_family;" \
	  "orlix_tcti_decode_load_store_exclusive_all_register_fields;" \
	  "orlix_tcti_gadget_executes_complete_load_store_exclusive_family") \
	X(ASIMD_STRUCTURE_LOAD_STORE, ASIMD, COMPLETE, \
	  "AdvSIMD structure load/store", "ORLIX_TCTI_DECODE_SIMD_LOAD_STORE_*", \
	  "orlix_tcti_decode_exhaustive_simd_single_structure_family;" \
	  "orlix_tcti_decode_exhaustive_simd_multiple_structure_family;" \
	  "orlix_tcti_decode_simd_structure_all_register_fields;" \
	  "orlix_tcti_switch_exhausts_simd_single_structure_execution;" \
	  "orlix_tcti_switch_exhausts_simd_multiple_structure_execution;" \
	  "orlix_tcti_switch_reports_simd_multiple_structure_fault_order") \
	X(ASIMD_COPY, ASIMD, COMPLETE, "AdvSIMD copy", \
	  "ORLIX_TCTI_DECODE_SIMD_VECTOR_ELEMENT_MOVE", \
	  "orlix_tcti_gadget_executes_complete_simd_copy_family") \
	X(ASIMD_TABLE_LOOKUP, ASIMD, COMPLETE, "AdvSIMD table lookup", "ORLIX_TCTI_DECODE_SIMD_TABLE_LOOKUP", "orlix_tcti_gadget_executes_complete_simd_table_lookup_family") \
	X(ASIMD_PERMUTE, ASIMD, COMPLETE, "AdvSIMD permute", "ORLIX_TCTI_DECODE_SIMD_VECTOR_ELEMENT_MOVE", "orlix_tcti_gadget_executes_complete_simd_permute_family") \
	X(ASIMD_EXTRACT, ASIMD, COMPLETE, "AdvSIMD extract", "ORLIX_TCTI_DECODE_SIMD_VECTOR_ELEMENT_MOVE", "orlix_tcti_gadget_executes_complete_simd_ext_family") \
	X(ASIMD_MODIFIED_IMMEDIATE, ASIMD, COMPLETE, \
	  "AdvSIMD modified immediate", \
	  "ORLIX_TCTI_DECODE_SIMD_MODIFIED_IMMEDIATE", \
	  "orlix_tcti_gadget_executes_exhaustive_simd_modimm") \
	X(ASIMD_SHIFT_IMMEDIATE, ASIMD, COMPLETE, \
	  "AdvSIMD shift by immediate", \
	  "ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC", \
	  "orlix_tcti_switch_executes_complete_simd_saturating_shift_left_immediate_family;" \
	  "orlix_tcti_switch_executes_complete_simd_scalar_shift_immediate_family;" \
	  "orlix_tcti_switch_executes_complete_simd_shift_left_insert_immediate_family;" \
	  "orlix_tcti_switch_executes_complete_simd_shift_right_immediate_family;" \
	  "orlix_tcti_switch_executes_complete_simd_shift_narrow_family;" \
	  "orlix_tcti_switch_executes_complete_simd_scalar_shift_narrow_family;" \
	  "orlix_tcti_switch_executes_complete_simd_scalar_saturating_narrow_family;" \
	  "orlix_tcti_switch_executes_complete_simd_shift_left_long_family") \
	X(ASIMD_SCALAR, ASIMD, COMPLETE, "AdvSIMD scalar data processing", \
	  "ORLIX_TCTI_DECODE_SIMD_VECTOR_*", \
	  "orlix_tcti_decode_exhaustive_simd_fp_scalar_unary;" \
	  "orlix_tcti_decode_recognizes_complete_simd_scalar_add_sub_family;" \
	  "orlix_tcti_decode_recognizes_complete_simd_scalar_saturating_add_sub_family;" \
	  "orlix_tcti_decode_recognizes_complete_simd_scalar_saturating_mul_high_family;" \
	  "orlix_tcti_decode_exhaustive_simd_scalar_sqdm_long;" \
	  "orlix_tcti_decode_exhaustive_simd_mixed_saturating_add;" \
	  "orlix_tcti_decode_recognizes_complete_simd_scalar_integer_unary_family;" \
	  "orlix_tcti_decode_recognizes_complete_simd_scalar_compare_zero_family;" \
	  "orlix_tcti_decode_recognizes_complete_simd_scalar_register_compare_family;" \
	  "orlix_tcti_decode_recognizes_complete_simd_scalar_shift_by_register_family;" \
	  "orlix_tcti_decode_recognizes_complete_simd_scalar_shift_immediate_family;" \
	  "orlix_tcti_decode_recognizes_complete_simd_scalar_shift_narrow_family;" \
	  "orlix_tcti_decode_recognizes_complete_simd_scalar_saturating_narrow_family;" \
	  "orlix_tcti_gadget_executes_complete_simd_fp_scalar_unary_family;" \
	  "orlix_tcti_gadget_executes_complete_simd_fp_three_same_family;" \
	  "orlix_tcti_gadget_executes_complete_simd_fp_pairwise_family") \
	X(ASIMD_VECTOR_3SAME, ASIMD, COMPLETE, "AdvSIMD vector three same", \
	  "ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC/" \
	  "ORLIX_TCTI_DECODE_SIMD_VECTOR_COMPARE/" \
	  "ORLIX_TCTI_DECODE_SIMD_VECTOR_LOGICAL", \
	  "orlix_tcti_decode_recognizes_complete_simd_vector_logical_family;" \
	  "orlix_tcti_decode_recognizes_complete_simd_add_sub_family;" \
	  "orlix_tcti_decode_recognizes_complete_simd_halving_add_family;" \
	  "orlix_tcti_decode_recognizes_complete_simd_halving_sub_family;" \
	  "orlix_tcti_decode_recognizes_complete_simd_pairwise_add_family;" \
	  "orlix_tcti_decode_recognizes_complete_simd_pairwise_min_max_family;" \
	  "orlix_tcti_decode_recognizes_complete_simd_saturating_add_sub_family;" \
	  "orlix_tcti_decode_recognizes_complete_simd_saturating_mul_high_family;" \
	  "orlix_tcti_decode_recognizes_complete_simd_mul_family;" \
	  "orlix_tcti_decode_recognizes_complete_simd_mla_mls_family;" \
	  "orlix_tcti_decode_recognizes_complete_simd_min_max_family;" \
	  "orlix_tcti_decode_recognizes_complete_simd_absolute_difference_family;" \
	  "orlix_tcti_decode_recognizes_complete_simd_register_compare_family;" \
	  "orlix_tcti_decode_recognizes_complete_simd_shift_by_register_family;" \
	  "orlix_tcti_decode_exhaustive_simd_fp_three_same;" \
	  "orlix_tcti_gadget_executes_complete_simd_fp_vector_three_same_family;" \
	  "orlix_tcti_gadget_executes_complete_polynomial_multiply_family") \
	X(ASIMD_VECTOR_3DIFFERENT, ASIMD, COMPLETE, \
	  "AdvSIMD vector three different", \
	  "ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC", \
	  "orlix_tcti_decode_recognizes_complete_simd_add_sub_wide_family;" \
	  "orlix_tcti_decode_recognizes_complete_simd_add_sub_long_family;" \
	  "orlix_tcti_decode_recognizes_complete_simd_multiply_long_family;" \
	  "orlix_tcti_decode_recognizes_complete_simd_pmull_family;" \
	  "orlix_tcti_decode_recognizes_complete_simd_add_sub_narrow_high_family;" \
	  "orlix_tcti_decode_recognizes_complete_simd_absolute_difference_long_family;" \
	  "orlix_tcti_decode_exhaustive_simd_vector_sqdm_long;" \
	  "orlix_tcti_switch_executes_complete_simd_add_sub_wide_family;" \
	  "orlix_tcti_switch_executes_complete_simd_add_sub_long_family;" \
	  "orlix_tcti_switch_executes_complete_simd_multiply_long_family;" \
	  "orlix_tcti_switch_executes_simd_pmull_known_vectors;" \
	  "orlix_tcti_switch_executes_complete_simd_add_sub_narrow_high_family;" \
	  "orlix_tcti_switch_executes_complete_simd_absolute_difference_long_family;" \
	  "orlix_tcti_gadget_executes_simd_vector_sqdm_long") \
	X(ASIMD_VECTOR_2REG_MISC, ASIMD, COMPLETE, \
	  "AdvSIMD vector two-register miscellaneous", \
	  "ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC", \
	  "orlix_tcti_decode_exhaustive_simd_fp_int_convert_family;" \
	  "orlix_tcti_decode_rejects_reserved_simd_fp_int_convert_shapes;" \
	  "orlix_tcti_switch_executes_complete_simd_fp_int_convert_family;" \
	  "orlix_tcti_decode_recognizes_complete_simd_xtn_family;" \
	  "orlix_tcti_switch_executes_complete_simd_xtn_family;" \
	  "orlix_tcti_decode_recognizes_complete_simd_shift_left_long_family;" \
	  "orlix_tcti_switch_executes_complete_simd_shift_left_long_family;" \
	  "orlix_tcti_decode_exhaustive_simd_mixed_saturating_add;" \
	  "orlix_tcti_gadget_executes_simd_vector_mixed_saturating_add;" \
	  "orlix_tcti_decode_exhaustive_simd_pairwise_long;" \
	  "orlix_tcti_gadget_executes_complete_simd_pairwise_long_family;" \
	  "orlix_tcti_decode_recognizes_complete_simd_saturating_narrow_family;" \
	  "orlix_tcti_switch_executes_complete_simd_saturating_narrow_family;" \
	  "orlix_tcti_decode_recognizes_complete_simd_integer_unary_family;" \
	  "orlix_tcti_switch_executes_complete_simd_integer_unary_family;" \
	  "orlix_tcti_decode_recognizes_complete_simd_compare_zero_family;" \
	  "orlix_tcti_switch_executes_complete_simd_compare_zero_family;" \
	  "orlix_tcti_decode_exhaustive_simd_vector_two_register_fp_family;" \
	  "orlix_tcti_gadget_executes_simd_vector_two_register_fp_family;" \
	  "orlix_tcti_simd_vector_two_register_rejects_invalid_runtime_shapes") \
	X(ASIMD_VECTOR_ACROSS_LANES, ASIMD, COMPLETE, \
	  "AdvSIMD vector across lanes", \
	  "ORLIX_TCTI_DECODE_SIMD_VECTOR_REDUCTION", \
	  "orlix_tcti_decode_recognizes_complete_simd_min_maxv_family;" \
	  "orlix_tcti_switch_executes_complete_simd_min_maxv_family;" \
	  "orlix_tcti_decode_recognizes_complete_simd_add_longv_family;" \
	  "orlix_tcti_switch_executes_complete_simd_add_longv_family;" \
	  "orlix_tcti_decode_recognizes_complete_simd_addv_family;" \
	  "orlix_tcti_switch_executes_complete_simd_addv_family;" \
	  "orlix_tcti_decode_exhaustive_simd_fp_across_lanes_family;" \
	  "orlix_tcti_decode_simd_across_lanes_all_register_fields;" \
	  "orlix_tcti_gadget_executes_complete_simd_fp_across_lanes_family;" \
	  "orlix_tcti_simd_fp_across_lanes_rejects_invalid_runtime_shapes") \
	X(ASIMD_VECTOR_INDEXED_ELEMENT, ASIMD, COMPLETE, \
	  "AdvSIMD vector by indexed element", \
	  "ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC", \
	  "orlix_tcti_decode_exhaustive_simd_indexed_operation_family;" \
	  "orlix_tcti_gadget_executes_complete_simd_indexed_family;" \
	  "orlix_tcti_gadget_executes_all_simd_indexed_shapes;" \
	  "orlix_tcti_simd_indexed_sets_qc_on_saturation;" \
	  "orlix_tcti_simd_indexed_preserves_source_aliasing;" \
	  "orlix_tcti_simd_indexed_rejects_invalid_runtime_shapes") \
	X(FP_FIXED_POINT_CONVERT, FP, COMPLETE, "floating-point fixed-point conversion", \
	  "ORLIX_TCTI_DECODE_FP_INT_CONVERT", \
	  "orlix_tcti_decode_exhaustive_fixed_point_convert_family;" \
	  "orlix_tcti_decode_rejects_reserved_fixed_point_convert_shapes;" \
	  "orlix_tcti_switch_executes_complete_fixed_point_convert_family;" \
	  "orlix_tcti_switch_fixed_point_preserves_signed_and_fault_semantics") \
	X(FP_INTEGER_CONVERT, FP, COMPLETE, "floating-point integer conversion", \
	  "ORLIX_TCTI_DECODE_FP_INT_CONVERT", \
	  "orlix_tcti_decode_recognizes_complete_fp_to_gpr_family;" \
	  "orlix_tcti_decode_exhaustive_fp_round_to_gpr_family;" \
	  "orlix_tcti_switch_executes_fp_round_to_gpr_family;" \
	  "orlix_tcti_decode_exhaustive_simd_fp_int_convert_family;" \
	  "orlix_tcti_decode_rejects_reserved_simd_fp_int_convert_shapes;" \
	  "orlix_tcti_decode_rejects_disabled_fp16_simd_int_conversions;" \
	  "orlix_tcti_switch_executes_complete_simd_fp_int_convert_family") \
	X(FP_1SOURCE, FP, COMPLETE, "floating-point one source", \
	  "ORLIX_TCTI_DECODE_FP_SCALAR_1SOURCE/ORLIX_TCTI_DECODE_FP_SCALAR_MOVE", \
	  "orlix_tcti_decode_recognizes_complete_fp_scalar_1source_family;" \
	  "orlix_tcti_gadget_executes_complete_fp_scalar_move_family;" \
	  "orlix_tcti_decode_exhaustive_fp_scalar_transfer_family;" \
	  "orlix_tcti_gadget_executes_complete_fp_scalar_transfer_family;" \
	  "orlix_tcti_gadget_fp_scalar_high_transfer_honors_zero_register;" \
	  "orlix_tcti_switch_executes_complete_fp_scalar_frint_family;" \
	  "orlix_tcti_gadget_executes_complete_fp_scalar_native_1source_family") \
	X(FP_COMPARE, FP, COMPLETE, "floating-point compare", "ORLIX_TCTI_DECODE_FP_SCALAR_COMPARE", "orlix_tcti_gadget_executes_exhaustive_fp_compare_family") \
	X(FP_IMMEDIATE, FP, COMPLETE, "floating-point immediate", "ORLIX_TCTI_DECODE_FP_SCALAR_IMMEDIATE", "orlix_tcti_gadget_executes_complete_fp_immediate_family") \
	X(FP_CONDITIONAL_COMPARE, FP, COMPLETE, "floating-point conditional compare", "ORLIX_TCTI_DECODE_FP_SCALAR_COMPARE", "orlix_tcti_gadget_executes_complete_fp_conditional_compare_family") \
	X(FP_CONDITIONAL_SELECT, FP, COMPLETE, "floating-point conditional select", "ORLIX_TCTI_DECODE_FP_CONDITIONAL_SELECT", "orlix_tcti_gadget_executes_complete_fp_conditional_select_family") \
	X(FP_2SOURCE, FP, COMPLETE, "floating-point two source", \
	  "ORLIX_TCTI_DECODE_FP_SCALAR_2SOURCE", \
	  "orlix_tcti_decode_exhaustive_fp_scalar_2source_family;" \
	  "orlix_tcti_decode_fp_scalar_2source_all_register_fields;" \
	  "orlix_tcti_decode_fp_scalar_2source_fixed_mask_boundaries;" \
	  "orlix_tcti_gadget_executes_complete_fp_scalar_2source_family;" \
	  "orlix_tcti_gadget_fp_scalar_2source_preserves_source_aliasing;" \
	  "orlix_tcti_switch_executes_fp_scalar_2source_minmax_family;" \
	  "orlix_tcti_switch_executes_scalar_fp2_ieee754_cases") \
	X(FP_3SOURCE, FP, COMPLETE, "floating-point three source", "ORLIX_TCTI_DECODE_FP_SCALAR_3SOURCE", "orlix_tcti_gadget_executes_exhaustive_fp_3source_family") \
	X(AES, AES, COMPLETE, "AES instructions", "ORLIX_TCTI_SIMD_ARITH_AES*", "orlix_tcti_gadget_executes_complete_aes_family") \
	X(SHA1_SHA256, SHA, COMPLETE, "SHA1 and SHA256 instructions", \
	  "ORLIX_TCTI_SIMD_ARITH_SHA1*/SHA256*", \
	  "orlix_tcti_decode_recognizes_complete_simd_sha1_family;" \
	  "orlix_tcti_decode_recognizes_complete_simd_sha256_family;" \
	  "orlix_tcti_decode_exhaustive_sha1_sha256_family;" \
	  "orlix_tcti_switch_executes_complete_simd_sha1_family;" \
	  "orlix_tcti_switch_executes_complete_simd_sha256_family") \
	X(SHA512, SHA512, COMPLETE, "SHA512 instructions", \
	  "ORLIX_TCTI_SIMD_ARITH_SHA512*", \
	  "orlix_tcti_decode_exhaustive_simd_sha512_family;" \
	  "orlix_tcti_switch_executes_complete_simd_sha512_family;" \
	  "orlix_tcti_switch_crypto_preserves_aliasing_and_unrelated_state") \
	X(SHA3, SHA3, COMPLETE, "SHA3 instructions", \
	  "ORLIX_TCTI_SIMD_ARITH_EOR3/RAX1/XAR/BCAX", \
	  "orlix_tcti_decode_exhaustive_simd_sha3_family;" \
	  "orlix_tcti_switch_executes_complete_simd_sha3_family;" \
	  "orlix_tcti_switch_crypto_preserves_aliasing_and_unrelated_state") \
	X(SM3, SM3, COMPLETE, "SM3 instructions", \
	  "ORLIX_TCTI_SIMD_ARITH_SM3*", \
	  "orlix_tcti_decode_exhaustive_simd_sm3_family;" \
	  "orlix_tcti_switch_executes_complete_simd_sm3_family;" \
	  "orlix_tcti_switch_crypto_preserves_aliasing_and_unrelated_state") \
	X(SM4, SM4, COMPLETE, "SM4 instructions", \
	  "ORLIX_TCTI_SIMD_ARITH_SM4E/ORLIX_TCTI_SIMD_ARITH_SM4EKEY", \
	  "orlix_tcti_decode_exhaustive_simd_sm4_family;" \
	  "orlix_tcti_switch_executes_complete_simd_sm4_family;" \
	  "orlix_tcti_switch_crypto_preserves_aliasing_and_unrelated_state") \
	X(POLYNOMIAL_MULTIPLY, PMULL, COMPLETE, "polynomial multiply", "ORLIX_TCTI_SIMD_ARITH_PMUL/ORLIX_TCTI_SIMD_ARITH_PMULL", "orlix_tcti_gadget_executes_complete_polynomial_multiply_family") \
	X(CRC32, CRC32, COMPLETE, "CRC32 and CRC32C", "ORLIX_TCTI_DP2_CRC32/ORLIX_TCTI_DP2_CRC32C", "orlix_tcti_gadget_executes_complete_crc32_family")

enum orlix_tcti_isa_family_id {
#define ORLIX_TCTI_ISA_FAMILY_ENUM(id, extension, status, name, decoder, kunit) \
	ORLIX_TCTI_ISA_FAMILY_##id,
	ORLIX_TCTI_ISA_FAMILIES(ORLIX_TCTI_ISA_FAMILY_ENUM)
#undef ORLIX_TCTI_ISA_FAMILY_ENUM
	ORLIX_TCTI_ISA_FAMILY_COUNT,
};

struct orlix_tcti_isa_family_coverage {
	enum orlix_tcti_isa_family_id id;
	enum orlix_tcti_isa_extension extension;
	enum orlix_tcti_isa_local_coverage_status status;
	const char *name;
	const char *decoder;
	const char *kunit;
};

static const struct orlix_tcti_isa_family_coverage orlix_tcti_isa_coverage[] = {
#define ORLIX_TCTI_ISA_FAMILY_ROW(id, extension, status, name, decoder, kunit) \
	{ ORLIX_TCTI_ISA_FAMILY_##id, ORLIX_TCTI_ISA_##extension, \
	  ORLIX_TCTI_ISA_LOCAL_COVERAGE_##status, name, decoder, kunit },
	ORLIX_TCTI_ISA_FAMILIES(ORLIX_TCTI_ISA_FAMILY_ROW)
#undef ORLIX_TCTI_ISA_FAMILY_ROW
};

/* Local catalog consistency only. Never use this as target completion proof. */
#define ORLIX_TCTI_ISA_EXPECTED_LOCAL_DECODER_GAPS	0

#endif /* ORLIX_TCTI_ISA_COVERAGE_H */
