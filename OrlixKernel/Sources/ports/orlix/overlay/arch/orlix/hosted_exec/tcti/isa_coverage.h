/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_ISA_COVERAGE_H
#define ORLIX_TCTI_ISA_COVERAGE_H

#include <linux/types.h>

/*
 * Architectural A64 encoding families required by the Orlix guest profile.
 * This inventory is independent of the current decoder. A family becomes
 * complete only after its legal and unallocated encoding boundaries, operand
 * extraction, architectural state transitions, and structured exits have
 * direct KUnit evidence through the production TCTI path.
 *
 * AES, SHA, polynomial multiply, and CRC remain completion requirements even
 * while their HWCAP bits are withheld from userspace.
 */
enum tcti_isa_extension {
	TCTI_ISA_BASE = 0,
	TCTI_ISA_FP,
	TCTI_ISA_ASIMD,
	TCTI_ISA_AES,
	TCTI_ISA_SHA,
	TCTI_ISA_PMULL,
	TCTI_ISA_CRC32,
};

enum tcti_isa_coverage_status {
	TCTI_ISA_COVERAGE_PARTIAL = 0,
	TCTI_ISA_COVERAGE_COMPLETE,
};

#define ORLIX_TCTI_ISA_FAMILIES(X) \
	X(PC_RELATIVE_ADDRESSING, BASE, COMPLETE, "pc-relative addressing", "TCTI_DECODE_PC_RELATIVE_ADDRESS", "tcti_gadget_executes_complete_pc_relative_address_family") \
	X(ADD_SUB_IMMEDIATE, BASE, PARTIAL, "add/subtract immediate", "TCTI_DECODE_ADD_SUB_IMMEDIATE", "tcti_decode_recognizes_add_sub_immediate_class") \
	X(LOGICAL_IMMEDIATE, BASE, PARTIAL, "logical immediate", "TCTI_DECODE_LOGICAL_IMMEDIATE", "tcti_decode_recognizes_logical_immediate_class") \
	X(MOVE_WIDE_IMMEDIATE, BASE, PARTIAL, "move wide immediate", "TCTI_DECODE_MOVE_WIDE_IMMEDIATE", "tcti_decode_recognizes_move_wide_immediate_class") \
	X(BITFIELD, BASE, PARTIAL, "bitfield", "TCTI_DECODE_BITFIELD", "tcti_decode_recognizes_bitfield_class") \
	X(EXTRACT, BASE, PARTIAL, "extract", "TCTI_DECODE_EXTRACT", "tcti_gadget_program_executes_extract") \
	X(UNCONDITIONAL_BRANCH_IMMEDIATE, BASE, COMPLETE, "unconditional branch immediate", "TCTI_DECODE_UNCONDITIONAL_BRANCH_IMMEDIATE", "tcti_gadget_executes_complete_unconditional_branch_immediate_family") \
	X(CONDITIONAL_BRANCH_IMMEDIATE, BASE, COMPLETE, "conditional branch immediate", "TCTI_DECODE_CONDITIONAL_BRANCH_IMMEDIATE", "tcti_gadget_executes_complete_conditional_branch_immediate_family") \
	X(COMPARE_BRANCH_IMMEDIATE, BASE, COMPLETE, "compare and branch immediate", "TCTI_DECODE_COMPARE_BRANCH_IMMEDIATE", "tcti_gadget_executes_complete_compare_branch_immediate_family") \
	X(TEST_BRANCH_IMMEDIATE, BASE, COMPLETE, "test and branch immediate", "TCTI_DECODE_TEST_BRANCH_IMMEDIATE", "tcti_gadget_executes_complete_test_branch_immediate_family") \
	X(EXCEPTION_GENERATION, BASE, PARTIAL, "exception generation", "TCTI_DECODE_SVC/TCTI_DECODE_BRK", "tcti_decode_recognizes_svc_zero") \
	X(SYSTEM_AND_HINT, BASE, PARTIAL, "system, hint, and barrier", "TCTI_DECODE_HINT/TCTI_DECODE_SYSTEM_REGISTER", "tcti_decode_recognizes_system_register_class") \
	X(UNCONDITIONAL_BRANCH_REGISTER, BASE, COMPLETE, "unconditional branch register", "TCTI_DECODE_UNCONDITIONAL_BRANCH_REGISTER", "tcti_gadget_executes_complete_unconditional_branch_register_family") \
	X(CONDITIONAL_COMPARE, BASE, COMPLETE, "conditional compare", "TCTI_DECODE_CONDITIONAL_COMPARE", "tcti_gadget_executes_complete_conditional_compare_family") \
	X(CONDITIONAL_SELECT, BASE, COMPLETE, "conditional select", "TCTI_DECODE_CONDITIONAL_SELECT", "tcti_gadget_executes_complete_conditional_select_family") \
	X(LOGICAL_SHIFTED_REGISTER, BASE, PARTIAL, "logical shifted register", "TCTI_DECODE_LOGICAL_SHIFTED_REGISTER", "tcti_decode_recognizes_logical_shifted_register_class") \
	X(ADD_SUB_SHIFTED_REGISTER, BASE, PARTIAL, "add/subtract shifted register", "TCTI_DECODE_ADD_SUB_SHIFTED_REGISTER", "tcti_decode_recognizes_add_sub_shifted_register_class") \
	X(ADD_SUB_EXTENDED_REGISTER, BASE, PARTIAL, "add/subtract extended register", "TCTI_DECODE_ADD_SUB_EXTENDED_REGISTER", "tcti_decode_recognizes_add_sub_extended_register_class") \
	X(ADD_SUB_WITH_CARRY, BASE, PARTIAL, "add/subtract with carry", "TCTI_DECODE_ADD_SUB_WITH_CARRY", "") \
	X(DATA_PROCESSING_1SOURCE, BASE, PARTIAL, "data processing one source", "TCTI_DECODE_DATA_PROCESSING_1SOURCE", "tcti_decode_recognizes_data_processing_1source_class") \
	X(DATA_PROCESSING_2SOURCE, BASE, PARTIAL, "data processing two source", "TCTI_DECODE_DATA_PROCESSING_2SOURCE", "tcti_decode_recognizes_data_processing_2source_class") \
	X(DATA_PROCESSING_3SOURCE, BASE, PARTIAL, "data processing three source", "TCTI_DECODE_MULTIPLY_ADD_SUB", "tcti_decode_recognizes_multiply_add_sub_class") \
	X(LOAD_LITERAL, BASE, COMPLETE, "load register literal", "TCTI_DECODE_LOAD_LITERAL", "tcti_gadget_executes_load_literal_state_transitions") \
	X(LOAD_STORE_PAIR, BASE, COMPLETE, "load/store register pair", "TCTI_DECODE_LOAD_STORE_PAIR", "tcti_gadget_executes_non_temporal_simd_pair") \
	X(LOAD_STORE_REGISTER, BASE, PARTIAL, "load/store register", "TCTI_DECODE_LOAD_STORE_*", "") \
	X(LOAD_STORE_EXCLUSIVE, BASE, PARTIAL, "load/store exclusive", "TCTI_DECODE_LOAD_STORE_EXCLUSIVE", "tcti_decode_recognizes_load_store_exclusive_class") \
	X(ASIMD_STRUCTURE_LOAD_STORE, ASIMD, PARTIAL, "AdvSIMD structure load/store", "TCTI_DECODE_SIMD_LOAD_STORE_*", "") \
	X(ASIMD_COPY, ASIMD, PARTIAL, "AdvSIMD copy", "TCTI_DECODE_SIMD_VECTOR_ELEMENT_MOVE", "") \
	X(ASIMD_TABLE_LOOKUP, ASIMD, PARTIAL, "AdvSIMD table lookup", "TCTI_DECODE_SIMD_TABLE_LOOKUP", "") \
	X(ASIMD_PERMUTE, ASIMD, PARTIAL, "AdvSIMD permute", "TCTI_DECODE_SIMD_VECTOR_ELEMENT_MOVE", "") \
	X(ASIMD_EXTRACT, ASIMD, PARTIAL, "AdvSIMD extract", "TCTI_DECODE_SIMD_VECTOR_ELEMENT_MOVE", "") \
	X(ASIMD_MODIFIED_IMMEDIATE, ASIMD, PARTIAL, "AdvSIMD modified immediate", "TCTI_DECODE_SIMD_MODIFIED_IMMEDIATE", "tcti_decode_recognizes_simd_modified_immediates") \
	X(ASIMD_SHIFT_IMMEDIATE, ASIMD, PARTIAL, "AdvSIMD shift by immediate", "TCTI_DECODE_SIMD_VECTOR_ARITHMETIC", "") \
	X(ASIMD_SCALAR, ASIMD, PARTIAL, "AdvSIMD scalar data processing", "TCTI_DECODE_SIMD_VECTOR_*", "") \
	X(ASIMD_VECTOR_3SAME, ASIMD, PARTIAL, "AdvSIMD vector three same", "TCTI_DECODE_SIMD_VECTOR_*", "") \
	X(ASIMD_VECTOR_3DIFFERENT, ASIMD, PARTIAL, "AdvSIMD vector three different", "TCTI_DECODE_SIMD_VECTOR_ARITHMETIC", "") \
	X(ASIMD_VECTOR_2REG_MISC, ASIMD, PARTIAL, "AdvSIMD vector two-register miscellaneous", "TCTI_DECODE_SIMD_VECTOR_ARITHMETIC", "") \
	X(ASIMD_VECTOR_ACROSS_LANES, ASIMD, PARTIAL, "AdvSIMD vector across lanes", "TCTI_DECODE_SIMD_VECTOR_REDUCTION", "") \
	X(ASIMD_VECTOR_INDEXED_ELEMENT, ASIMD, PARTIAL, "AdvSIMD vector by indexed element", "TCTI_DECODE_SIMD_VECTOR_ARITHMETIC", "") \
	X(FP_FIXED_POINT_CONVERT, FP, PARTIAL, "floating-point fixed-point conversion", "TCTI_DECODE_FP_INT_CONVERT", "") \
	X(FP_INTEGER_CONVERT, FP, PARTIAL, "floating-point integer conversion", "TCTI_DECODE_FP_INT_CONVERT", "tcti_decode_recognizes_complete_fp_to_gpr_family") \
	X(FP_1SOURCE, FP, PARTIAL, "floating-point one source", "TCTI_DECODE_FP_SCALAR_1SOURCE", "") \
	X(FP_COMPARE, FP, COMPLETE, "floating-point compare", "TCTI_DECODE_FP_SCALAR_COMPARE", "tcti_gadget_executes_exhaustive_fp_compare_family") \
	X(FP_IMMEDIATE, FP, COMPLETE, "floating-point immediate", "TCTI_DECODE_FP_SCALAR_IMMEDIATE", "tcti_gadget_executes_complete_fp_immediate_family") \
	X(FP_CONDITIONAL_COMPARE, FP, COMPLETE, "floating-point conditional compare", "TCTI_DECODE_FP_SCALAR_COMPARE", "tcti_gadget_executes_complete_fp_conditional_compare_family") \
	X(FP_CONDITIONAL_SELECT, FP, COMPLETE, "floating-point conditional select", "TCTI_DECODE_FP_CONDITIONAL_SELECT", "tcti_gadget_executes_complete_fp_conditional_select_family") \
	X(FP_2SOURCE, FP, COMPLETE, "floating-point two source", "TCTI_DECODE_FP_SCALAR_2SOURCE", "tcti_decode_recognizes_complete_fp_scalar_2source_family") \
	X(FP_3SOURCE, FP, COMPLETE, "floating-point three source", "TCTI_DECODE_FP_SCALAR_3SOURCE", "tcti_gadget_executes_exhaustive_fp_3source_family") \
	X(AES, AES, PARTIAL, "AES instructions", "TCTI_SIMD_ARITH_AES*", "") \
	X(SHA1_SHA256, SHA, PARTIAL, "SHA1 and SHA256 instructions", "TCTI_SIMD_ARITH_SHA1*/SHA256*", "") \
	X(POLYNOMIAL_MULTIPLY, PMULL, PARTIAL, "polynomial multiply", "TCTI_SIMD_ARITH_PMUL/TCTI_SIMD_ARITH_PMULL", "") \
	X(CRC32, CRC32, PARTIAL, "CRC32 and CRC32C", "TCTI_DP2_CRC32/TCTI_DP2_CRC32C", "tcti_switch_executes_crc32_family")

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
	enum tcti_isa_coverage_status status;
	const char *name;
	const char *decoder;
	const char *kunit;
};

static const struct tcti_isa_family_coverage tcti_isa_coverage[] = {
#define TCTI_ISA_FAMILY_ROW(id, extension, status, name, decoder, kunit) \
	{ TCTI_ISA_FAMILY_##id, TCTI_ISA_##extension, \
	  TCTI_ISA_COVERAGE_##status, name, decoder, kunit },
	ORLIX_TCTI_ISA_FAMILIES(TCTI_ISA_FAMILY_ROW)
#undef TCTI_ISA_FAMILY_ROW
};

/* Ratchet this to zero only by closing rows with direct owning KUnit proof. */
#define ORLIX_TCTI_ISA_EXPECTED_GAPS	36

#endif /* ORLIX_TCTI_ISA_COVERAGE_H */
