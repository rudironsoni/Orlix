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

struct tcti_baseline_decode_vector {
	const char *name;
	u32 instruction;
	enum tcti_decode_class expected_class;
};

/* One exact representative encoding from every decoder family retained from
 * 7c062898.  Classes which were intentionally generalized use their current
 * destination class, never a retired enum value. */
static const struct tcti_baseline_decode_vector tcti_baseline_vectors[] = {
	{ "svc", 0xd4000001U, TCTI_DECODE_SVC },
	{ "hint", 0xd503201fU, TCTI_DECODE_HINT },
	{ "add_sub_immediate", 0x910003e0U, TCTI_DECODE_ADD_SUB_IMMEDIATE },
	{ "add_sub_shifted", 0x8b000273U,
	  TCTI_DECODE_ADD_SUB_SHIFTED_REGISTER },
	{ "add_sub_extended", 0x8b284128U,
	  TCTI_DECODE_ADD_SUB_EXTENDED_REGISTER },
	{ "add_sub_carry", 0xfa03001fU, TCTI_DECODE_ADD_SUB_WITH_CARRY },
	{ "pc_relative", 0x90000021U, TCTI_DECODE_PC_RELATIVE_ADDRESS },
	{ "branch_immediate", 0x94002283U,
	  TCTI_DECODE_UNCONDITIONAL_BRANCH_IMMEDIATE },
	{ "branch_register", 0xd63f0260U,
	  TCTI_DECODE_UNCONDITIONAL_BRANCH_REGISTER },
	{ "compare_branch", 0xb40001d4U,
	  TCTI_DECODE_COMPARE_BRANCH_IMMEDIATE },
	{ "test_branch", 0xb6f80080U, TCTI_DECODE_TEST_BRANCH_IMMEDIATE },
	{ "conditional_branch", 0x54ffff00U,
	  TCTI_DECODE_CONDITIONAL_BRANCH_IMMEDIATE },
	{ "conditional_compare", 0xfa4019a4U,
	  TCTI_DECODE_CONDITIONAL_COMPARE },
	{ "conditional_select", 0x1a9fc517U,
	  TCTI_DECODE_CONDITIONAL_SELECT },
	{ "load_store_pair", 0xa9016ffcU, TCTI_DECODE_LOAD_STORE_PAIR },
	{ "load_store_unsigned", 0xf9456929U,
	  TCTI_DECODE_LOAD_STORE_UNSIGNED_IMMEDIATE },
	{ "load_store_signed", 0xf85d03a9U,
	  TCTI_DECODE_LOAD_STORE_SIGNED_IMMEDIATE },
	{ "load_store_register", 0xf8286920U,
	  TCTI_DECODE_LOAD_STORE_REGISTER_OFFSET },
	{ "logical_shifted", 0xea0b015fU,
	  TCTI_DECODE_LOGICAL_SHIFTED_REGISTER },
	{ "logical_immediate", 0x7200191fU, TCTI_DECODE_LOGICAL_IMMEDIATE },
	{ "bitfield", 0xd350ff09U, TCTI_DECODE_BITFIELD },
	{ "extract", 0x93c80508U, TCTI_DECODE_EXTRACT },
	{ "data_processing_one_source", 0xdac01109U,
	  TCTI_DECODE_DATA_PROCESSING_1SOURCE },
	{ "data_processing_two_source", 0x9acb082dU,
	  TCTI_DECODE_DATA_PROCESSING_2SOURCE },
	{ "multiply_add_sub", 0x9b0b85aeU,
	  TCTI_DECODE_MULTIPLY_ADD_SUB },
	{ "move_wide", 0xf2800319U, TCTI_DECODE_MOVE_WIDE_IMMEDIATE },
	{ "system_register", 0xd53b4401U, TCTI_DECODE_SYSTEM_REGISTER },
	{ "clrex", 0xd5033f5fU, TCTI_DECODE_EXCLUSIVE_MONITOR_CLEAR },
	{ "load_store_exclusive", 0x885ffe61U,
	  TCTI_DECODE_LOAD_STORE_EXCLUSIVE },
	{ "simd_modified_immediate", 0x0f002420U,
	  TCTI_DECODE_SIMD_MODIFIED_IMMEDIATE },
	{ "simd_element_move", 0x0e023ccfU,
	  TCTI_DECODE_SIMD_VECTOR_ELEMENT_MOVE },
	{ "simd_vector_logical", 0x4e211c01U,
	  TCTI_DECODE_SIMD_VECTOR_LOGICAL },
	{ "simd_vector_arithmetic", 0x4ea08440U,
	  TCTI_DECODE_SIMD_VECTOR_ARITHMETIC },
	{ "simd_vector_compare", 0x4ea09821U,
	  TCTI_DECODE_SIMD_VECTOR_COMPARE },
	{ "simd_vector_reduction", 0x4eb1b800U,
	  TCTI_DECODE_SIMD_VECTOR_REDUCTION },
	{ "simd_load_replicate", 0x4d40c900U,
	  TCTI_DECODE_SIMD_LOAD_REPLICATE },
	{ "fp_scalar_move", 0x1e2600abU, TCTI_DECODE_FP_SCALAR_MOVE },
	{ "fp_scalar_one_source", 0x1e60c020U,
	  TCTI_DECODE_FP_SCALAR_1SOURCE },
	{ "fp_scalar_two_source", 0x1e211800U,
	  TCTI_DECODE_FP_SCALAR_2SOURCE },
	{ "fp_scalar_three_source", 0x1f501cc7U,
	  TCTI_DECODE_FP_SCALAR_3SOURCE },
	{ "fp_scalar_compare", 0x1e602000U,
	  TCTI_DECODE_FP_SCALAR_COMPARE },
	{ "fp_conditional_select", 0x1e603c40U,
	  TCTI_DECODE_FP_CONDITIONAL_SELECT },
	{ "fp_integer_convert", 0x1e220101U,
	  TCTI_DECODE_FP_INT_CONVERT },
};

static void tcti_baseline_decoder_families_remain_decodable(struct kunit *test)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(tcti_baseline_vectors); index++) {
		const struct tcti_baseline_decode_vector *vector =
			&tcti_baseline_vectors[index];
		struct tcti_decoded_instruction decoded =
			tcti_decode_aarch64(vector->instruction);

		KUNIT_EXPECT_EQ_MSG(test, vector->expected_class,
				    decoded.decode_class, "%s %#x", vector->name,
				    vector->instruction);
		KUNIT_EXPECT_EQ_MSG(test, vector->instruction, decoded.instruction,
				    "%s", vector->name);
	}
}

static void tcti_baseline_logical_immediate_uses_modified_immediate_dispatch(
	struct kunit *test)
{
	static const struct {
		u32 instruction;
		enum tcti_simd_modified_immediate_op operation;
		u64 immediate;
	} vectors[] = {
		/* old TCTI_DECODE_SIMD_VECTOR_LOGICAL_IMMEDIATE: ORR v0.4s,#0x30 */
		{ 0x4f011600U, TCTI_SIMD_MODIMM_ORR, 0x0000003000000030ULL },
		/* old TCTI_DECODE_SIMD_VECTOR_LOGICAL_IMMEDIATE: BIC v2.4s,#2 */
		{ 0x6f001442U, TCTI_SIMD_MODIMM_BIC, 0x0000000200000002ULL },
	};
	size_t index;

	for (index = 0; index < ARRAY_SIZE(vectors); index++) {
		struct tcti_decoded_instruction decoded =
			tcti_decode_aarch64(vectors[index].instruction);
		struct pt_regs regs = { .pc = 0x1000UL };
		u64 old_low;
		u64 old_high;
		unsigned long old_valid;
		u64 expected_low;
		u64 expected_high;
		int ret;

		KUNIT_ASSERT_EQ(test, TCTI_DECODE_SIMD_MODIFIED_IMMEDIATE,
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

		expected_low = vectors[index].operation == TCTI_SIMD_MODIMM_ORR ?
			0x1122334455667788ULL | vectors[index].immediate :
			0x1122334455667788ULL & ~vectors[index].immediate;
		expected_high = vectors[index].operation == TCTI_SIMD_MODIMM_ORR ?
			0x8877665544332211ULL | vectors[index].immediate :
			0x8877665544332211ULL & ~vectors[index].immediate;
		ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded,
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

static struct kunit_case tcti_baseline_decoder_regression_cases[] = {
	KUNIT_CASE(tcti_baseline_decoder_families_remain_decodable),
	KUNIT_CASE(tcti_baseline_logical_immediate_uses_modified_immediate_dispatch),
	{}
};

static struct kunit_suite tcti_baseline_decoder_regression_test_suite = {
	.name = "orlix-tcti-baseline-decoder-regression",
	.test_cases = tcti_baseline_decoder_regression_cases,
};

kunit_test_suite(tcti_baseline_decoder_regression_test_suite);
