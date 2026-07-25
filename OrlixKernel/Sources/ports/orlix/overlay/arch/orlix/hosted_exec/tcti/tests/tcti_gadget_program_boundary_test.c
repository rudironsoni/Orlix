// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <asm/ptrace.h>

#include "../decode_aarch64.h"
#include "../gadget_program.h"

/* ADD X0, X1, #1. */
#define TCTI_GADGET_BOUNDARY_ADD_X0_X1_1 0x91000420U

static void tcti_gadget_program_copies_decoded_storage_before_execute(
	struct kunit *test)
{
	struct tcti_decoded_instruction decoded =
		tcti_decode_aarch64(TCTI_GADGET_BOUNDARY_ADD_X0_X1_1);
	struct tcti_decoded_instruction stored = {};
	struct tcti_gadget_word
		program[TCTI_SINGLE_INSTRUCTION_PROGRAM_WORDS] = {};
	struct pt_regs regs = {};
	size_t word_count = 0;
	unsigned long fault_address = 0;
	int ret;

	KUNIT_ASSERT_EQ(test, TCTI_DECODE_ADD_SUB_IMMEDIATE,
			decoded.decode_class);
	KUNIT_ASSERT_EQ(test, 0, tcti_lower_decoded_instruction(
				&decoded, program, ARRAY_SIZE(program), &word_count));
	KUNIT_ASSERT_EQ(test, (size_t)TCTI_SINGLE_INSTRUCTION_PROGRAM_WORDS,
			word_count);

	memcpy(&stored, &program[1], sizeof(stored));
	KUNIT_EXPECT_MEMEQ(test, &decoded, &stored, sizeof(decoded));

	regs.pc = 0x4000;
	regs.regs[1] = 0x123456789abcdef0ULL;
	ret = tcti_execute_gadget_program(NULL, &regs, program, word_count,
					  &fault_address);
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x123456789abcdef1ULL, regs.regs[0]);
	KUNIT_EXPECT_EQ(test, 0x4004ULL, regs.pc);
	KUNIT_EXPECT_EQ(test, 0UL, fault_address);
}

static struct kunit_case tcti_gadget_program_boundary_test_cases[] = {
	KUNIT_CASE(tcti_gadget_program_copies_decoded_storage_before_execute),
	{}
};

struct kunit_suite tcti_gadget_program_boundary_test_suite = {
	.name = "orlix-tcti-gadget-program-boundary",
	.test_cases = tcti_gadget_program_boundary_test_cases,
};

kunit_test_suite(tcti_gadget_program_boundary_test_suite);
