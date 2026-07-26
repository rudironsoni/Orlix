// SPDX-License-Identifier: GPL-2.0-only
/*
 * Regression vectors retained from the decoder at 7c062898.  The current
 * decoder intentionally generalizes several of these encodings, particularly
 * AdvSIMD modified immediates.  Keep their source encodings explicit so a
 * future family expansion cannot silently stop recognizing the working base.
 */
#include <kunit/test.h>
#include <linux/sched.h>
#include <asm/processor.h>
#include <asm/ptrace.h>

#include "../decode_aarch64.h"
#include "../switch_debug.h"

struct orlix_tcti_baseline_decode_vector {
	const char *name;
	u32 instruction;
	enum orlix_tcti_decode_class expected_class;
};

/* One exact representative encoding from every decoder family retained from
 * 7c062898.  Classes which were intentionally generalized use their current
 * destination class, never a retired enum value. */
static const struct orlix_tcti_baseline_decode_vector orlix_tcti_baseline_vectors[] = {
	{ "svc", 0xd4000001U, ORLIX_TCTI_DECODE_SVC },
	{ "hint", 0xd503201fU, ORLIX_TCTI_DECODE_HINT },
	{ "add_sub_immediate", 0x910003e0U, ORLIX_TCTI_DECODE_ADD_SUB_IMMEDIATE },
	{ "add_sub_shifted", 0x8b000273U,
	  ORLIX_TCTI_DECODE_ADD_SUB_SHIFTED_REGISTER },
	{ "add_sub_extended", 0x8b284128U,
	  ORLIX_TCTI_DECODE_ADD_SUB_EXTENDED_REGISTER },
	{ "add_sub_carry", 0xfa03001fU, ORLIX_TCTI_DECODE_ADD_SUB_WITH_CARRY },
	{ "pc_relative", 0x90000021U, ORLIX_TCTI_DECODE_PC_RELATIVE_ADDRESS },
	{ "branch_immediate", 0x94002283U,
	  ORLIX_TCTI_DECODE_UNCONDITIONAL_BRANCH_IMMEDIATE },
	{ "branch_register", 0xd63f0260U,
	  ORLIX_TCTI_DECODE_UNCONDITIONAL_BRANCH_REGISTER },
	{ "compare_branch", 0xb40001d4U,
	  ORLIX_TCTI_DECODE_COMPARE_BRANCH_IMMEDIATE },
	{ "test_branch", 0xb6f80080U, ORLIX_TCTI_DECODE_TEST_BRANCH_IMMEDIATE },
	{ "conditional_branch", 0x54ffff00U,
	  ORLIX_TCTI_DECODE_CONDITIONAL_BRANCH_IMMEDIATE },
	{ "conditional_compare", 0xfa4019a4U,
	  ORLIX_TCTI_DECODE_CONDITIONAL_COMPARE },
	{ "conditional_select", 0x1a9fc517U,
	  ORLIX_TCTI_DECODE_CONDITIONAL_SELECT },
	{ "load_store_pair", 0xa9016ffcU, ORLIX_TCTI_DECODE_LOAD_STORE_PAIR },
	{ "load_store_unsigned", 0xf9456929U,
	  ORLIX_TCTI_DECODE_LOAD_STORE_UNSIGNED_IMMEDIATE },
	{ "load_store_signed", 0xf85d03a9U,
	  ORLIX_TCTI_DECODE_LOAD_STORE_SIGNED_IMMEDIATE },
	{ "load_store_register", 0xf8286920U,
	  ORLIX_TCTI_DECODE_LOAD_STORE_REGISTER_OFFSET },
	{ "logical_shifted", 0xea0b015fU,
	  ORLIX_TCTI_DECODE_LOGICAL_SHIFTED_REGISTER },
	{ "logical_immediate", 0x7200191fU, ORLIX_TCTI_DECODE_LOGICAL_IMMEDIATE },
	{ "bitfield", 0xd350ff09U, ORLIX_TCTI_DECODE_BITFIELD },
	{ "extract", 0x93c80508U, ORLIX_TCTI_DECODE_EXTRACT },
	{ "data_processing_one_source", 0xdac01109U,
	  ORLIX_TCTI_DECODE_DATA_PROCESSING_1SOURCE },
	{ "data_processing_two_source", 0x9acb082dU,
	  ORLIX_TCTI_DECODE_DATA_PROCESSING_2SOURCE },
	{ "multiply_add_sub", 0x9b0b85aeU,
	  ORLIX_TCTI_DECODE_MULTIPLY_ADD_SUB },
	{ "move_wide", 0xf2800319U, ORLIX_TCTI_DECODE_MOVE_WIDE_IMMEDIATE },
	{ "system_register", 0xd53b4401U, ORLIX_TCTI_DECODE_SYSTEM_REGISTER },
	{ "clrex", 0xd5033f5fU, ORLIX_TCTI_DECODE_EXCLUSIVE_MONITOR_CLEAR },
	{ "load_store_exclusive", 0x885ffe61U,
	  ORLIX_TCTI_DECODE_LOAD_STORE_EXCLUSIVE },
	{ "simd_modified_immediate", 0x0f002420U,
	  ORLIX_TCTI_DECODE_SIMD_MODIFIED_IMMEDIATE },
	{ "simd_element_move", 0x0e023ccfU,
	  ORLIX_TCTI_DECODE_SIMD_VECTOR_ELEMENT_MOVE },
	{ "simd_vector_logical", 0x4e211c01U,
	  ORLIX_TCTI_DECODE_SIMD_VECTOR_LOGICAL },
	{ "simd_vector_arithmetic", 0x4ea08440U,
	  ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC },
	{ "simd_vector_compare", 0x4ea09821U,
	  ORLIX_TCTI_DECODE_SIMD_VECTOR_COMPARE },
	{ "simd_vector_reduction", 0x4eb1b800U,
	  ORLIX_TCTI_DECODE_SIMD_VECTOR_REDUCTION },
	{ "simd_load_replicate", 0x4d40c900U,
	  ORLIX_TCTI_DECODE_SIMD_LOAD_REPLICATE },
	{ "fp_scalar_move", 0x1e2600abU, ORLIX_TCTI_DECODE_FP_SCALAR_MOVE },
	{ "fp_scalar_one_source", 0x1e60c020U,
	  ORLIX_TCTI_DECODE_FP_SCALAR_1SOURCE },
	{ "fp_scalar_two_source", 0x1e211800U,
	  ORLIX_TCTI_DECODE_FP_SCALAR_2SOURCE },
	{ "fp_scalar_three_source", 0x1f501cc7U,
	  ORLIX_TCTI_DECODE_FP_SCALAR_3SOURCE },
	{ "fp_scalar_compare", 0x1e602000U,
	  ORLIX_TCTI_DECODE_FP_SCALAR_COMPARE },
	{ "fp_conditional_select", 0x1e603c40U,
	  ORLIX_TCTI_DECODE_FP_CONDITIONAL_SELECT },
	{ "fp_integer_convert", 0x1e220101U,
	  ORLIX_TCTI_DECODE_FP_INT_CONVERT },
};

static void orlix_tcti_baseline_decoder_families_remain_decodable(struct kunit *test)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_baseline_vectors); index++) {
		const struct orlix_tcti_baseline_decode_vector *vector =
			&orlix_tcti_baseline_vectors[index];
		struct orlix_tcti_decoded_instruction decoded =
			orlix_tcti_decode_aarch64(vector->instruction);

		KUNIT_EXPECT_EQ_MSG(test, vector->expected_class,
				    decoded.decode_class, "%s %#x", vector->name,
				    vector->instruction);
		KUNIT_EXPECT_EQ_MSG(test, vector->instruction, decoded.instruction,
				    "%s", vector->name);
	}
}

static void orlix_tcti_baseline_logical_immediate_uses_modified_immediate_dispatch(
	struct kunit *test)
{
	static const struct {
		u32 instruction;
		enum orlix_tcti_simd_modified_immediate_op operation;
		u64 immediate;
	} vectors[] = {
		/* old ORLIX_TCTI_DECODE_SIMD_VECTOR_LOGICAL_IMMEDIATE: ORR v0.4s,#0x30 */
		{ 0x4f011600U, ORLIX_TCTI_SIMD_MODIMM_ORR, 0x0000003000000030ULL },
		/* old ORLIX_TCTI_DECODE_SIMD_VECTOR_LOGICAL_IMMEDIATE: BIC v2.4s,#2 */
		{ 0x6f001442U, ORLIX_TCTI_SIMD_MODIMM_BIC, 0x0000000200000002ULL },
	};
	size_t index;

	for (index = 0; index < ARRAY_SIZE(vectors); index++) {
		struct orlix_tcti_decoded_instruction decoded =
			orlix_tcti_decode_aarch64(vectors[index].instruction);
		struct pt_regs regs = { .pc = 0x1000UL };
		u64 old_low;
		u64 old_high;
		unsigned long old_valid;
		u64 expected_low;
		u64 expected_high;
		int ret;

		KUNIT_ASSERT_EQ(test, ORLIX_TCTI_DECODE_SIMD_MODIFIED_IMMEDIATE,
				decoded.decode_class);
		KUNIT_ASSERT_EQ(test, vectors[index].operation,
				decoded.simd_modified_immediate_op);
		KUNIT_ASSERT_EQ(test, vectors[index].immediate,
				decoded.logical_immediate);

		old_low = current->thread.user_simd[decoded.rd * 2];
		old_high = current->thread.user_simd[decoded.rd * 2 + 1];
		old_valid = current->thread.user_simd_valid;
		current->thread.user_simd[decoded.rd * 2] =
			0x1122334455667788ULL;
		current->thread.user_simd[decoded.rd * 2 + 1] =
			0x8877665544332211ULL;
		current->thread.user_simd_valid = 0;

		expected_low = vectors[index].operation == ORLIX_TCTI_SIMD_MODIMM_ORR ?
			0x1122334455667788ULL | vectors[index].immediate :
			0x1122334455667788ULL & ~vectors[index].immediate;
		expected_high = vectors[index].operation == ORLIX_TCTI_SIMD_MODIMM_ORR ?
			0x8877665544332211ULL | vectors[index].immediate :
			0x8877665544332211ULL & ~vectors[index].immediate;
		ret = orlix_tcti_switch_debug_execute_decoded(NULL, &regs, &decoded,
						       NULL);
		KUNIT_EXPECT_EQ(test, 0, ret);
		KUNIT_EXPECT_EQ(test, expected_low,
				current->thread.user_simd[decoded.rd * 2]);
		KUNIT_EXPECT_EQ(test, expected_high,
				current->thread.user_simd[decoded.rd * 2 + 1]);
		KUNIT_EXPECT_EQ(test, 1UL, current->thread.user_simd_valid);
		KUNIT_EXPECT_EQ(test, 0x1004UL, regs.pc);

		current->thread.user_simd[decoded.rd * 2] = old_low;
		current->thread.user_simd[decoded.rd * 2 + 1] = old_high;
		current->thread.user_simd_valid = old_valid;
	}
}

static struct kunit_case orlix_tcti_baseline_decoder_regression_cases[] = {
	KUNIT_CASE(orlix_tcti_baseline_decoder_families_remain_decodable),
	KUNIT_CASE(orlix_tcti_baseline_logical_immediate_uses_modified_immediate_dispatch),
	{}
};

static struct kunit_suite orlix_tcti_baseline_decoder_regression_test_suite = {
	.name = "orlix-tcti-baseline-decoder-regression",
	.test_cases = orlix_tcti_baseline_decoder_regression_cases,
};

kunit_test_suite(orlix_tcti_baseline_decoder_regression_test_suite);
