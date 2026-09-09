// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include <linux/errno.h>
#include <asm/ptrace.h>

#include "../decode_aarch64.h"
#include "../gadget_program.h"

/* ADD X0, X1, #1. */
#define ORLIX_TCTI_GADGET_BOUNDARY_ADD_X0_X1_1 0x91000420U
/* SUBS X0, X0, #1. */
#define ORLIX_TCTI_GADGET_BOUNDARY_SUBS_X0_X0_1 0xf1000400U
/* B.NE to the previous instruction. */
#define ORLIX_TCTI_GADGET_BOUNDARY_B_NE_PREV 0x54ffffe1U

static void orlix_tcti_gadget_program_stores_compact_micro_op(
	struct kunit *test)
{
	struct orlix_tcti_decoded_instruction decoded =
		orlix_tcti_decode_aarch64(ORLIX_TCTI_GADGET_BOUNDARY_ADD_X0_X1_1);
	struct orlix_tcti_gadget_word
		program[ORLIX_TCTI_SINGLE_INSTRUCTION_PROGRAM_WORDS] = {};
	struct pt_regs regs = {};
	size_t word_count = 0;
	unsigned long fault_address = 0;
	int ret;

	KUNIT_ASSERT_EQ(test, ORLIX_TCTI_DECODE_ADD_SUB_IMMEDIATE,
			decoded.decode_class);
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_lower_decoded_instruction(
				&decoded, program, ARRAY_SIZE(program), &word_count));
	KUNIT_ASSERT_EQ(test, (size_t)ORLIX_TCTI_SINGLE_INSTRUCTION_PROGRAM_WORDS,
			word_count);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_GADGET_BOUNDARY_ADD_X0_X1_1,
			orlix_tcti_program_instruction_at(program, word_count, 0));
	KUNIT_EXPECT_NE(test, sizeof(decoded),
			(ORLIX_TCTI_MICRO_OP_WORDS - 1) *
				sizeof(struct orlix_tcti_gadget_word));

	regs.pc = 0x4000;
	regs.regs[1] = 0x123456789abcdef0ULL;
	ret = orlix_tcti_execute_gadget_program(NULL, &regs, program, word_count,
					  &fault_address);
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x123456789abcdef1ULL, regs.regs[0]);
	KUNIT_EXPECT_EQ(test, 0x4004ULL, regs.pc);
	KUNIT_EXPECT_EQ(test, 0UL, fault_address);
}

static void orlix_tcti_gadget_program_runs_fused_subs_b_cond(struct kunit *test)
{
	struct orlix_tcti_decoded_instruction subs =
		orlix_tcti_decode_aarch64(ORLIX_TCTI_GADGET_BOUNDARY_SUBS_X0_X0_1);
	struct orlix_tcti_decoded_instruction b_ne =
		orlix_tcti_decode_aarch64(ORLIX_TCTI_GADGET_BOUNDARY_B_NE_PREV);
	struct orlix_tcti_gadget_word
		program[ORLIX_TCTI_PROGRAM_WORDS_FOR_INSTRUCTIONS(2)] = {};
	struct pt_regs regs = {};
	size_t word_count = 0;
	unsigned long fault_address = 0;
	int ret;

	KUNIT_ASSERT_EQ(test, ORLIX_TCTI_DECODE_ADD_SUB_IMMEDIATE,
			subs.decode_class);
	KUNIT_ASSERT_EQ(test, ORLIX_TCTI_DECODE_CONDITIONAL_BRANCH_IMMEDIATE,
			b_ne.decode_class);
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_append_decoded_instruction(
				&subs, program, ARRAY_SIZE(program), &word_count));
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_append_decoded_instruction(
				&b_ne, program, ARRAY_SIZE(program), &word_count));
	KUNIT_ASSERT_EQ(test, (size_t)ORLIX_TCTI_PROGRAM_WORDS_FOR_INSTRUCTIONS(2),
			word_count);
	KUNIT_EXPECT_TRUE(test, orlix_tcti_gadget_program_fuses_subs_b_cond(
					program, word_count));
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_GADGET_BOUNDARY_SUBS_X0_X0_1,
			orlix_tcti_program_instruction_at(program, word_count, 0));
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_GADGET_BOUNDARY_B_NE_PREV,
			orlix_tcti_program_instruction_at(program, word_count, 1));

	regs.pc = 0x4000;
	regs.regs[0] = 2;
	ret = orlix_tcti_execute_gadget_program(NULL, &regs, program, word_count,
					  &fault_address);
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 1ULL, regs.regs[0]);
	KUNIT_EXPECT_EQ(test, 0x4000ULL, regs.pc);
	KUNIT_EXPECT_EQ(test, 0UL, fault_address);
}

static struct kunit_case orlix_tcti_gadget_program_boundary_test_cases[] = {
	KUNIT_CASE(orlix_tcti_gadget_program_stores_compact_micro_op),
	KUNIT_CASE(orlix_tcti_gadget_program_runs_fused_subs_b_cond),
	{}
};

struct kunit_suite orlix_tcti_gadget_program_boundary_test_suite = {
	.name = "orlix-tcti-gadget-program-boundary",
	.test_cases = orlix_tcti_gadget_program_boundary_test_cases,
};

kunit_test_suite(orlix_tcti_gadget_program_boundary_test_suite);
