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
enum tcti_isa_extension {
	TCTI_ISA_BASE = 0,
	TCTI_ISA_FP,
	TCTI_ISA_ASIMD,
	TCTI_ISA_AES,
	TCTI_ISA_SHA,
	TCTI_ISA_SHA512,
	TCTI_ISA_SHA3,
	TCTI_ISA_SM3,
	TCTI_ISA_SM4,
	TCTI_ISA_PMULL,
	TCTI_ISA_CRC32,
	TCTI_ISA_EXTENSION_COUNT,
};

enum tcti_isa_local_coverage_status {
	TCTI_ISA_LOCAL_COVERAGE_PARTIAL = 0,
	TCTI_ISA_LOCAL_COVERAGE_COMPLETE,
};

#define ORLIX_TCTI_ISA_FAMILIES(X) \
	X(PC_RELATIVE_ADDRESSING, BASE, COMPLETE, "pc-relative addressing", "TCTI_DECODE_PC_RELATIVE_ADDRESS", "tcti_gadget_executes_complete_pc_relative_address_family") \
	X(ADD_SUB_IMMEDIATE, BASE, COMPLETE, "add/subtract immediate", "TCTI_DECODE_ADD_SUB_IMMEDIATE", "tcti_gadget_executes_complete_add_sub_immediate_family") \
	X(LOGICAL_IMMEDIATE, BASE, COMPLETE, "logical immediate", "TCTI_DECODE_LOGICAL_IMMEDIATE", "tcti_gadget_executes_complete_logical_immediate_family") \
	X(MOVE_WIDE_IMMEDIATE, BASE, COMPLETE, "move wide immediate", "TCTI_DECODE_MOVE_WIDE_IMMEDIATE", "tcti_gadget_executes_complete_move_wide_immediate_family") \
	X(BITFIELD, BASE, COMPLETE, "bitfield", "TCTI_DECODE_BITFIELD", "tcti_gadget_executes_complete_bitfield_family") \
	X(EXTRACT, BASE, COMPLETE, "extract", "TCTI_DECODE_EXTRACT", "tcti_gadget_executes_complete_extract_family") \
	X(UNCONDITIONAL_BRANCH_IMMEDIATE, BASE, COMPLETE, "unconditional branch immediate", "TCTI_DECODE_UNCONDITIONAL_BRANCH_IMMEDIATE", "tcti_gadget_executes_complete_unconditional_branch_immediate_family") \
	X(CONDITIONAL_BRANCH_IMMEDIATE, BASE, COMPLETE, "conditional branch immediate", "TCTI_DECODE_CONDITIONAL_BRANCH_IMMEDIATE", "tcti_gadget_executes_complete_conditional_branch_immediate_family") \
	X(COMPARE_BRANCH_IMMEDIATE, BASE, COMPLETE, "compare and branch immediate", "TCTI_DECODE_COMPARE_BRANCH_IMMEDIATE", "tcti_gadget_executes_complete_compare_branch_immediate_family") \
	X(TEST_BRANCH_IMMEDIATE, BASE, COMPLETE, "test and branch immediate", "TCTI_DECODE_TEST_BRANCH_IMMEDIATE", "tcti_gadget_executes_complete_test_branch_immediate_family") \
	X(EXCEPTION_GENERATION, BASE, COMPLETE, "exception generation", \
	  "TCTI_DECODE_SVC/TCTI_DECODE_BRK/TCTI_DECODE_HLT", \
	  "tcti_decode_covers_complete_exception_generation_family;" \
	  "tcti_resume_user_reports_syscall_and_register_state;" \
	  "tcti_resume_user_reports_breakpoint_and_register_state;" \
	  "tcti_resume_user_reports_hlt_as_undefined") \
	X(SYSTEM_AND_HINT, BASE, COMPLETE, "system, hint, and barrier", \
	  "TCTI_DECODE_HINT/TCTI_DECODE_SYSTEM_REGISTER/" \
	  "TCTI_DECODE_BARRIER/TCTI_DECODE_CACHE_MAINTENANCE", \
	  "tcti_decode_exhaustive_system_register_family/" \
	  "tcti_decode_exhaustive_hint_barrier_cache_family/" \
	  "tcti_engine_reports_structured_hint_yields/tcti_system_probe") \
	X(UNCONDITIONAL_BRANCH_REGISTER, BASE, COMPLETE, "unconditional branch register", "TCTI_DECODE_UNCONDITIONAL_BRANCH_REGISTER", "tcti_gadget_executes_complete_unconditional_branch_register_family") \
	X(CONDITIONAL_COMPARE, BASE, COMPLETE, "conditional compare", "TCTI_DECODE_CONDITIONAL_COMPARE", "tcti_gadget_executes_complete_conditional_compare_family") \
	X(CONDITIONAL_SELECT, BASE, COMPLETE, "conditional select", "TCTI_DECODE_CONDITIONAL_SELECT", "tcti_gadget_executes_complete_conditional_select_family") \
	X(LOGICAL_SHIFTED_REGISTER, BASE, COMPLETE, "logical shifted register", "TCTI_DECODE_LOGICAL_SHIFTED_REGISTER", "tcti_gadget_executes_complete_logical_shifted_register_family") \
	X(ADD_SUB_SHIFTED_REGISTER, BASE, COMPLETE, "add/subtract shifted register", "TCTI_DECODE_ADD_SUB_SHIFTED_REGISTER", "tcti_gadget_executes_complete_add_sub_shifted_register_family") \
	X(ADD_SUB_EXTENDED_REGISTER, BASE, COMPLETE, "add/subtract extended register", "TCTI_DECODE_ADD_SUB_EXTENDED_REGISTER", "tcti_gadget_executes_complete_add_sub_extended_register_family") \
	X(ADD_SUB_WITH_CARRY, BASE, COMPLETE, "add/subtract with carry", "TCTI_DECODE_ADD_SUB_WITH_CARRY", "tcti_gadget_executes_complete_add_sub_with_carry_family") \
	X(DATA_PROCESSING_1SOURCE, BASE, COMPLETE, "data processing one source", "TCTI_DECODE_DATA_PROCESSING_1SOURCE", "tcti_gadget_executes_complete_data_processing_1source_family") \
	X(DATA_PROCESSING_2SOURCE, BASE, COMPLETE, "data processing two source", "TCTI_DECODE_DATA_PROCESSING_2SOURCE", "tcti_gadget_executes_complete_data_processing_2source_family") \
	X(DATA_PROCESSING_3SOURCE, BASE, COMPLETE, "data processing three source", "TCTI_DECODE_MULTIPLY_ADD_SUB", "tcti_gadget_executes_complete_data_processing_3source_family") \
	X(LOAD_LITERAL, BASE, COMPLETE, "load register literal", \
	  "TCTI_DECODE_LOAD_LITERAL", \
	  "tcti_decode_exhaustive_load_literal_family;" \
	  "tcti_decode_load_literal_all_fields;" \
	  "tcti_decode_load_literal_fixed_mask_boundaries;" \
	  "tcti_gadget_executes_complete_load_literal_family;" \
	  "tcti_gadget_reports_load_literal_faults") \
	X(LOAD_STORE_PAIR, BASE, COMPLETE, "load/store register pair", \
	  "TCTI_DECODE_LOAD_STORE_PAIR", \
	  "tcti_decode_exhaustive_load_store_pair_family;" \
	  "tcti_decode_load_store_pair_all_register_fields;" \
	  "tcti_decode_load_store_pair_fixed_mask_boundaries;" \
	  "tcti_gadget_executes_complete_load_store_pair_family;" \
	  "tcti_gadget_reports_load_store_pair_second_fault") \
	X(LOAD_STORE_REGISTER, BASE, COMPLETE, "load/store register", \
	  "TCTI_DECODE_LOAD_STORE_*/TCTI_DECODE_HINT", \
	  "tcti_decode_exhaustive_load_store_unsigned_immediate_family;" \
	  "tcti_decode_exhaustive_load_store_signed_immediate_family;" \
	  "tcti_decode_exhaustive_load_store_register_offset_family;" \
	  "tcti_decode_load_store_all_register_fields;" \
	  "tcti_gadget_executes_complete_load_store_register_family") \
	X(LOAD_STORE_EXCLUSIVE, BASE, COMPLETE, "load/store exclusive", \
	  "TCTI_DECODE_LOAD_STORE_EXCLUSIVE", \
	  "tcti_decode_exhaustive_load_store_exclusive_family;" \
	  "tcti_decode_load_store_exclusive_all_register_fields;" \
	  "tcti_gadget_executes_complete_load_store_exclusive_family") \
	X(ASIMD_STRUCTURE_LOAD_STORE, ASIMD, COMPLETE, \
	  "AdvSIMD structure load/store", "TCTI_DECODE_SIMD_LOAD_STORE_*", \
	  "tcti_decode_exhaustive_simd_single_structure_family;" \
	  "tcti_decode_exhaustive_simd_multiple_structure_family;" \
	  "tcti_decode_simd_structure_all_register_fields;" \
	  "tcti_switch_exhausts_simd_single_structure_execution;" \
	  "tcti_switch_exhausts_simd_multiple_structure_execution;" \
	  "tcti_switch_reports_simd_multiple_structure_fault_order") \
	X(ASIMD_COPY, ASIMD, COMPLETE, "AdvSIMD copy", \
	  "TCTI_DECODE_SIMD_VECTOR_ELEMENT_MOVE", \
	  "tcti_gadget_executes_complete_simd_copy_family") \
	X(ASIMD_TABLE_LOOKUP, ASIMD, COMPLETE, "AdvSIMD table lookup", "TCTI_DECODE_SIMD_TABLE_LOOKUP", "tcti_gadget_executes_complete_simd_table_lookup_family") \
	X(ASIMD_PERMUTE, ASIMD, COMPLETE, "AdvSIMD permute", "TCTI_DECODE_SIMD_VECTOR_ELEMENT_MOVE", "tcti_gadget_executes_complete_simd_permute_family") \
	X(ASIMD_EXTRACT, ASIMD, COMPLETE, "AdvSIMD extract", "TCTI_DECODE_SIMD_VECTOR_ELEMENT_MOVE", "tcti_gadget_executes_complete_simd_ext_family") \
	X(ASIMD_MODIFIED_IMMEDIATE, ASIMD, COMPLETE, \
	  "AdvSIMD modified immediate", \
	  "TCTI_DECODE_SIMD_MODIFIED_IMMEDIATE", \
	  "tcti_gadget_executes_exhaustive_simd_modimm") \
	X(ASIMD_SHIFT_IMMEDIATE, ASIMD, COMPLETE, \
	  "AdvSIMD shift by immediate", \
	  "TCTI_DECODE_SIMD_VECTOR_ARITHMETIC", \
	  "tcti_switch_executes_complete_simd_saturating_shift_left_immediate_family;" \
	  "tcti_switch_executes_complete_simd_scalar_shift_immediate_family;" \
	  "tcti_switch_executes_complete_simd_shift_left_insert_immediate_family;" \
	  "tcti_switch_executes_complete_simd_shift_right_immediate_family;" \
	  "tcti_switch_executes_complete_simd_shift_narrow_family;" \
	  "tcti_switch_executes_complete_simd_scalar_shift_narrow_family;" \
	  "tcti_switch_executes_complete_simd_scalar_saturating_narrow_family;" \
	  "tcti_switch_executes_complete_simd_shift_left_long_family") \
	X(ASIMD_SCALAR, ASIMD, COMPLETE, "AdvSIMD scalar data processing", \
	  "TCTI_DECODE_SIMD_VECTOR_*", \
	  "tcti_decode_exhaustive_simd_fp_scalar_unary;" \
	  "tcti_decode_recognizes_complete_simd_scalar_add_sub_family;" \
	  "tcti_decode_recognizes_complete_simd_scalar_saturating_add_sub_family;" \
	  "tcti_decode_recognizes_complete_simd_scalar_saturating_mul_high_family;" \
	  "tcti_decode_exhaustive_simd_scalar_sqdm_long;" \
	  "tcti_decode_exhaustive_simd_mixed_saturating_add;" \
	  "tcti_decode_recognizes_complete_simd_scalar_integer_unary_family;" \
	  "tcti_decode_recognizes_complete_simd_scalar_compare_zero_family;" \
	  "tcti_decode_recognizes_complete_simd_scalar_register_compare_family;" \
	  "tcti_decode_recognizes_complete_simd_scalar_shift_by_register_family;" \
	  "tcti_decode_recognizes_complete_simd_scalar_shift_immediate_family;" \
	  "tcti_decode_recognizes_complete_simd_scalar_shift_narrow_family;" \
	  "tcti_decode_recognizes_complete_simd_scalar_saturating_narrow_family;" \
	  "tcti_gadget_executes_complete_simd_fp_scalar_unary_family;" \
	  "tcti_gadget_executes_complete_simd_fp_three_same_family;" \
	  "tcti_gadget_executes_complete_simd_fp_pairwise_family") \
	X(ASIMD_VECTOR_3SAME, ASIMD, COMPLETE, "AdvSIMD vector three same", \
	  "TCTI_DECODE_SIMD_VECTOR_ARITHMETIC/" \
	  "TCTI_DECODE_SIMD_VECTOR_COMPARE/" \
	  "TCTI_DECODE_SIMD_VECTOR_LOGICAL", \
	  "tcti_decode_recognizes_complete_simd_vector_logical_family;" \
	  "tcti_decode_recognizes_complete_simd_add_sub_family;" \
	  "tcti_decode_recognizes_complete_simd_halving_add_family;" \
	  "tcti_decode_recognizes_complete_simd_halving_sub_family;" \
	  "tcti_decode_recognizes_complete_simd_pairwise_add_family;" \
	  "tcti_decode_recognizes_complete_simd_pairwise_min_max_family;" \
	  "tcti_decode_recognizes_complete_simd_saturating_add_sub_family;" \
	  "tcti_decode_recognizes_complete_simd_saturating_mul_high_family;" \
	  "tcti_decode_recognizes_complete_simd_mul_family;" \
	  "tcti_decode_recognizes_complete_simd_mla_mls_family;" \
	  "tcti_decode_recognizes_complete_simd_min_max_family;" \
	  "tcti_decode_recognizes_complete_simd_absolute_difference_family;" \
	  "tcti_decode_recognizes_complete_simd_register_compare_family;" \
	  "tcti_decode_recognizes_complete_simd_shift_by_register_family;" \
	  "tcti_decode_exhaustive_simd_fp_three_same;" \
	  "tcti_gadget_executes_complete_simd_fp_vector_three_same_family;" \
	  "tcti_gadget_executes_complete_polynomial_multiply_family") \
	X(ASIMD_VECTOR_3DIFFERENT, ASIMD, COMPLETE, \
	  "AdvSIMD vector three different", \
	  "TCTI_DECODE_SIMD_VECTOR_ARITHMETIC", \
	  "tcti_decode_recognizes_complete_simd_add_sub_wide_family;" \
	  "tcti_decode_recognizes_complete_simd_add_sub_long_family;" \
	  "tcti_decode_recognizes_complete_simd_multiply_long_family;" \
	  "tcti_decode_recognizes_complete_simd_pmull_family;" \
	  "tcti_decode_recognizes_complete_simd_add_sub_narrow_high_family;" \
	  "tcti_decode_recognizes_complete_simd_absolute_difference_long_family;" \
	  "tcti_decode_exhaustive_simd_vector_sqdm_long;" \
	  "tcti_switch_executes_complete_simd_add_sub_wide_family;" \
	  "tcti_switch_executes_complete_simd_add_sub_long_family;" \
	  "tcti_switch_executes_complete_simd_multiply_long_family;" \
	  "tcti_switch_executes_simd_pmull_known_vectors;" \
	  "tcti_switch_executes_complete_simd_add_sub_narrow_high_family;" \
	  "tcti_switch_executes_complete_simd_absolute_difference_long_family;" \
	  "tcti_gadget_executes_simd_vector_sqdm_long") \
	X(ASIMD_VECTOR_2REG_MISC, ASIMD, COMPLETE, \
	  "AdvSIMD vector two-register miscellaneous", \
	  "TCTI_DECODE_SIMD_VECTOR_ARITHMETIC", \
	  "tcti_decode_exhaustive_simd_fp_int_convert_family;" \
	  "tcti_decode_rejects_reserved_simd_fp_int_convert_shapes;" \
	  "tcti_switch_executes_complete_simd_fp_int_convert_family;" \
	  "tcti_decode_recognizes_complete_simd_xtn_family;" \
	  "tcti_switch_executes_complete_simd_xtn_family;" \
	  "tcti_decode_recognizes_complete_simd_shift_left_long_family;" \
	  "tcti_switch_executes_complete_simd_shift_left_long_family;" \
	  "tcti_decode_exhaustive_simd_mixed_saturating_add;" \
	  "tcti_gadget_executes_simd_vector_mixed_saturating_add;" \
	  "tcti_decode_exhaustive_simd_pairwise_long;" \
	  "tcti_gadget_executes_complete_simd_pairwise_long_family;" \
	  "tcti_decode_recognizes_complete_simd_saturating_narrow_family;" \
	  "tcti_switch_executes_complete_simd_saturating_narrow_family;" \
	  "tcti_decode_recognizes_complete_simd_integer_unary_family;" \
	  "tcti_switch_executes_complete_simd_integer_unary_family;" \
	  "tcti_decode_recognizes_complete_simd_compare_zero_family;" \
	  "tcti_switch_executes_complete_simd_compare_zero_family;" \
	  "tcti_decode_exhaustive_simd_vector_two_register_fp_family;" \
	  "tcti_gadget_executes_simd_vector_two_register_fp_family;" \
	  "tcti_simd_vector_two_register_rejects_invalid_runtime_shapes") \
	X(ASIMD_VECTOR_ACROSS_LANES, ASIMD, COMPLETE, \
	  "AdvSIMD vector across lanes", \
	  "TCTI_DECODE_SIMD_VECTOR_REDUCTION", \
	  "tcti_decode_recognizes_complete_simd_min_maxv_family;" \
	  "tcti_switch_executes_complete_simd_min_maxv_family;" \
	  "tcti_decode_recognizes_complete_simd_add_longv_family;" \
	  "tcti_switch_executes_complete_simd_add_longv_family;" \
	  "tcti_decode_recognizes_complete_simd_addv_family;" \
	  "tcti_switch_executes_complete_simd_addv_family;" \
	  "tcti_decode_exhaustive_simd_fp_across_lanes_family;" \
	  "tcti_decode_simd_across_lanes_all_register_fields;" \
	  "tcti_gadget_executes_complete_simd_fp_across_lanes_family;" \
	  "tcti_simd_fp_across_lanes_rejects_invalid_runtime_shapes") \
	X(ASIMD_VECTOR_INDEXED_ELEMENT, ASIMD, COMPLETE, \
	  "AdvSIMD vector by indexed element", \
	  "TCTI_DECODE_SIMD_VECTOR_ARITHMETIC", \
	  "tcti_decode_exhaustive_simd_indexed_operation_family;" \
	  "tcti_gadget_executes_complete_simd_indexed_family;" \
	  "tcti_gadget_executes_all_simd_indexed_shapes;" \
	  "tcti_simd_indexed_sets_qc_on_saturation;" \
	  "tcti_simd_indexed_preserves_source_aliasing;" \
	  "tcti_simd_indexed_rejects_invalid_runtime_shapes") \
	X(FP_FIXED_POINT_CONVERT, FP, COMPLETE, "floating-point fixed-point conversion", \
	  "TCTI_DECODE_FP_INT_CONVERT", \
	  "tcti_decode_exhaustive_fixed_point_convert_family;" \
	  "tcti_decode_rejects_reserved_fixed_point_convert_shapes;" \
	  "tcti_switch_executes_complete_fixed_point_convert_family;" \
	  "tcti_switch_fixed_point_preserves_signed_and_fault_semantics") \
	X(FP_INTEGER_CONVERT, FP, COMPLETE, "floating-point integer conversion", \
	  "TCTI_DECODE_FP_INT_CONVERT", \
	  "tcti_decode_recognizes_complete_fp_to_gpr_family;" \
	  "tcti_decode_exhaustive_fp_round_to_gpr_family;" \
	  "tcti_switch_executes_fp_round_to_gpr_family;" \
	  "tcti_decode_exhaustive_simd_fp_int_convert_family;" \
	  "tcti_decode_rejects_reserved_simd_fp_int_convert_shapes;" \
	  "tcti_decode_rejects_disabled_fp16_simd_int_conversions;" \
	  "tcti_switch_executes_complete_simd_fp_int_convert_family") \
	X(FP_1SOURCE, FP, COMPLETE, "floating-point one source", \
	  "TCTI_DECODE_FP_SCALAR_1SOURCE/TCTI_DECODE_FP_SCALAR_MOVE", \
	  "tcti_decode_recognizes_complete_fp_scalar_1source_family;" \
	  "tcti_gadget_executes_complete_fp_scalar_move_family;" \
	  "tcti_decode_exhaustive_fp_scalar_transfer_family;" \
	  "tcti_gadget_executes_complete_fp_scalar_transfer_family;" \
	  "tcti_gadget_fp_scalar_high_transfer_honors_zero_register;" \
	  "tcti_switch_executes_complete_fp_scalar_frint_family;" \
	  "tcti_gadget_executes_complete_fp_scalar_native_1source_family") \
	X(FP_COMPARE, FP, COMPLETE, "floating-point compare", "TCTI_DECODE_FP_SCALAR_COMPARE", "tcti_gadget_executes_exhaustive_fp_compare_family") \
	X(FP_IMMEDIATE, FP, COMPLETE, "floating-point immediate", "TCTI_DECODE_FP_SCALAR_IMMEDIATE", "tcti_gadget_executes_complete_fp_immediate_family") \
	X(FP_CONDITIONAL_COMPARE, FP, COMPLETE, "floating-point conditional compare", "TCTI_DECODE_FP_SCALAR_COMPARE", "tcti_gadget_executes_complete_fp_conditional_compare_family") \
	X(FP_CONDITIONAL_SELECT, FP, COMPLETE, "floating-point conditional select", "TCTI_DECODE_FP_CONDITIONAL_SELECT", "tcti_gadget_executes_complete_fp_conditional_select_family") \
	X(FP_2SOURCE, FP, COMPLETE, "floating-point two source", \
	  "TCTI_DECODE_FP_SCALAR_2SOURCE", \
	  "tcti_decode_exhaustive_fp_scalar_2source_family;" \
	  "tcti_decode_fp_scalar_2source_all_register_fields;" \
	  "tcti_decode_fp_scalar_2source_fixed_mask_boundaries;" \
	  "tcti_gadget_executes_complete_fp_scalar_2source_family;" \
	  "tcti_gadget_fp_scalar_2source_preserves_source_aliasing;" \
	  "tcti_switch_executes_fp_scalar_2source_minmax_family;" \
	  "tcti_switch_executes_scalar_fp2_ieee754_cases") \
	X(FP_3SOURCE, FP, COMPLETE, "floating-point three source", "TCTI_DECODE_FP_SCALAR_3SOURCE", "tcti_gadget_executes_exhaustive_fp_3source_family") \
	X(AES, AES, COMPLETE, "AES instructions", "TCTI_SIMD_ARITH_AES*", "tcti_gadget_executes_complete_aes_family") \
	X(SHA1_SHA256, SHA, COMPLETE, "SHA1 and SHA256 instructions", \
	  "TCTI_SIMD_ARITH_SHA1*/SHA256*", \
	  "tcti_decode_recognizes_complete_simd_sha1_family;" \
	  "tcti_decode_recognizes_complete_simd_sha256_family;" \
	  "tcti_decode_exhaustive_sha1_sha256_family;" \
	  "tcti_switch_executes_complete_simd_sha1_family;" \
	  "tcti_switch_executes_complete_simd_sha256_family") \
	X(SHA512, SHA512, COMPLETE, "SHA512 instructions", \
	  "TCTI_SIMD_ARITH_SHA512*", \
	  "tcti_decode_exhaustive_simd_sha512_family;" \
	  "tcti_switch_executes_complete_simd_sha512_family;" \
	  "tcti_switch_crypto_preserves_aliasing_and_unrelated_state") \
	X(SHA3, SHA3, COMPLETE, "SHA3 instructions", \
	  "TCTI_SIMD_ARITH_EOR3/RAX1/XAR/BCAX", \
	  "tcti_decode_exhaustive_simd_sha3_family;" \
	  "tcti_switch_executes_complete_simd_sha3_family;" \
	  "tcti_switch_crypto_preserves_aliasing_and_unrelated_state") \
	X(SM3, SM3, COMPLETE, "SM3 instructions", \
	  "TCTI_SIMD_ARITH_SM3*", \
	  "tcti_decode_exhaustive_simd_sm3_family;" \
	  "tcti_switch_executes_complete_simd_sm3_family;" \
	  "tcti_switch_crypto_preserves_aliasing_and_unrelated_state") \
	X(SM4, SM4, COMPLETE, "SM4 instructions", \
	  "TCTI_SIMD_ARITH_SM4E/TCTI_SIMD_ARITH_SM4EKEY", \
	  "tcti_decode_exhaustive_simd_sm4_family;" \
	  "tcti_switch_executes_complete_simd_sm4_family;" \
	  "tcti_switch_crypto_preserves_aliasing_and_unrelated_state") \
	X(POLYNOMIAL_MULTIPLY, PMULL, COMPLETE, "polynomial multiply", "TCTI_SIMD_ARITH_PMUL/TCTI_SIMD_ARITH_PMULL", "tcti_gadget_executes_complete_polynomial_multiply_family") \
	X(CRC32, CRC32, COMPLETE, "CRC32 and CRC32C", "TCTI_DP2_CRC32/TCTI_DP2_CRC32C", "tcti_gadget_executes_complete_crc32_family")

enum tcti_isa_family_id {
#define TCTI_ISA_FAMILY_ENUM(id, extension, status, name, decoder, kunit) \
	TCTI_ISA_FAMILY_##id,
	ORLIX_TCTI_ISA_FAMILIES(TCTI_ISA_FAMILY_ENUM)
#undef TCTI_ISA_FAMILY_ENUM
	TCTI_ISA_FAMILY_COUNT,
};

struct tcti_isa_family_coverage {
	enum tcti_isa_family_id id;
	enum tcti_isa_extension extension;
	enum tcti_isa_local_coverage_status status;
	const char *name;
	const char *decoder;
	const char *kunit;
};

static const struct tcti_isa_family_coverage tcti_isa_coverage[] = {
#define TCTI_ISA_FAMILY_ROW(id, extension, status, name, decoder, kunit) \
	{ TCTI_ISA_FAMILY_##id, TCTI_ISA_##extension, \
	  TCTI_ISA_LOCAL_COVERAGE_##status, name, decoder, kunit },
	ORLIX_TCTI_ISA_FAMILIES(TCTI_ISA_FAMILY_ROW)
#undef TCTI_ISA_FAMILY_ROW
};

/* Local catalog consistency only. Never use this as target completion proof. */
#define ORLIX_TCTI_ISA_EXPECTED_LOCAL_DECODER_GAPS	0

#endif /* ORLIX_TCTI_ISA_COVERAGE_H */
