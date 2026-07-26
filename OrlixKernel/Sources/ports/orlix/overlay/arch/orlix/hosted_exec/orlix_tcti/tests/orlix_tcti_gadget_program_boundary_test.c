// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <asm/ptrace.h>

#include "../decode_aarch64.h"
#include "../gadget_program.h"

/* ADD X0, X1, #1. */
#define ORLIX_TCTI_GADGET_BOUNDARY_ADD_X0_X1_1 0x91000420U

static void orlix_tcti_gadget_program_copies_decoded_storage_before_execute(
	struct kunit *test)
{
	struct orlix_tcti_decoded_instruction decoded =
		orlix_tcti_decode_aarch64(ORLIX_TCTI_GADGET_BOUNDARY_ADD_X0_X1_1);
	struct orlix_tcti_decoded_instruction stored = {};
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

	memcpy(&stored, &program[1], sizeof(stored));
	KUNIT_EXPECT_MEMEQ(test, &decoded, &stored, sizeof(decoded));

	regs.pc = 0x4000;
	regs.regs[1] = 0x123456789abcdef0ULL;
	ret = orlix_tcti_execute_gadget_program(NULL, &regs, program, word_count,
					  &fault_address);
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x123456789abcdef1ULL, regs.regs[0]);
	KUNIT_EXPECT_EQ(test, 0x4004ULL, regs.pc);
	KUNIT_EXPECT_EQ(test, 0UL, fault_address);
}

static struct kunit_case orlix_tcti_gadget_program_boundary_test_cases[] = {
	KUNIT_CASE(orlix_tcti_gadget_program_copies_decoded_storage_before_execute),
	{}
};

struct kunit_suite orlix_tcti_gadget_program_boundary_test_suite = {
	.name = "orlix-tcti-gadget-program-boundary",
	.test_cases = orlix_tcti_gadget_program_boundary_test_cases,
};

kunit_test_suite(orlix_tcti_gadget_program_boundary_test_suite);
