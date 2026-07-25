// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include <linux/bitops.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/sched/mm.h>
#include <linux/string.h>
#include <linux/syscalls.h>

#include <asm/ptrace.h>
#include <asm/tcti.h>

#include "../sve_decode.h"

#define TCTI_SVE_TEST_SVC 0xd4000001U

struct tcti_sve_resume_context {
	struct mm_struct *mm;
	struct tcti_sve_state sve;
	unsigned long simd[ARRAY_SIZE(current->thread.user_simd)];
};

static int tcti_sve_resume_test_init(struct kunit *test)
{
	struct tcti_sve_resume_context *context;
	struct mm_struct *mm;

	context = kunit_kzalloc(test, sizeof(*context), GFP_KERNEL);
	if (!context)
		return -ENOMEM;
	mm = mm_alloc();
	if (!mm)
		return -ENOMEM;
	memcpy(&context->sve, &current->thread.user_sve,
	       sizeof(context->sve));
	memcpy(context->simd, current->thread.user_simd, sizeof(context->simd));
	kthread_use_mm(mm);
	context->mm = mm;
	test->priv = context;
	return 0;
}

static void tcti_sve_resume_test_exit(struct kunit *test)
{
	struct tcti_sve_resume_context *context = test->priv;
	struct mm_struct *mm;

	if (!context)
		return;
	memcpy(&current->thread.user_sve, &context->sve,
	       sizeof(context->sve));
	memcpy(current->thread.user_simd, context->simd, sizeof(context->simd));
	mm = context->mm;
	kthread_unuse_mm(mm);
	mmput(mm);
}

static u32 tcti_sve_predicated_binary_instruction(u8 opcode,
						   u8 size, u8 pg, u8 zm, u8 zd)
{
	return AARCH64_SVE_PREDICATED_INTEGER_BINARY |
		((u32)size << 22) | ((u32)opcode << 16) |
		((u32)pg << 10) | ((u32)zm << 5) | zd;
}

static int tcti_sve_write_program(unsigned long address, const u32 *program,
				  size_t count)
{
	int ret;

	ret = sys_mprotect(address, PAGE_SIZE, PROT_READ | PROT_WRITE);
	if (ret)
		return ret;
	ret = tcti_write_user_data(current->mm, address, program,
				   count * sizeof(*program));
	if (ret)
		return ret;
	return sys_mprotect(address, PAGE_SIZE, PROT_READ | PROT_EXEC);
}

static void tcti_sve_resume_executes_predicated_integer_family(struct kunit *test)
{
	static const struct {
		u32 source_ordinal;
		u8 opcode;
		u64 expected;
		bool masked_expected;
	} leaves[] = {
		{ 0U, 0, 9, false }, { 1U, 1, 3, false },
		{ 2U, 3, 2, true }, { 5U, 8, 6, false },
		{ 6U, 10, 3, false }, { 7U, 12, 3, false },
		{ 8U, 9, 6, false }, { 9U, 11, 3, false },
		{ 10U, 13, 3, false }, { 11U, 16, 18, false },
		{ 12U, 18, 0, false }, { 13U, 19, 0, false },
		{ 14U, 20, 2, false }, { 15U, 22, 0, false },
		{ 16U, 21, 2, false }, { 17U, 23, 0, false },
		{ 18U, 24, 7, false }, { 19U, 25, 5, false },
		{ 20U, 26, 2, false }, { 21U, 27, 4, false },
	};
	unsigned long instructions;
	size_t index;
	int ret;

	instructions = ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
					 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(instructions));

	for (index = 0; index < ARRAY_SIZE(leaves); index++) {
		u8 size;

		for (size = 0; size < 4; size++) {
			u8 element_bytes = 1U << size;
			u64 mask = element_bytes == sizeof(u64) ? U64_MAX :
				GENMASK_ULL(element_bytes * BITS_PER_BYTE - 1, 0);
			u32 sve_instruction = tcti_sve_predicated_binary_instruction(
				leaves[index].opcode, size, 0, 1, 0);
			const u32 program[] = { sve_instruction, TCTI_SVE_TEST_SVC };
			struct pt_regs regs = {};
			struct tcti_result result;
			u16 inactive_offset = 16;

			ret = tcti_sve_write_program(instructions, program,
						     ARRAY_SIZE(program));
			KUNIT_ASSERT_EQ(test, 0, ret);
			KUNIT_ASSERT_EQ(test, 0, tcti_sve_state_reset(
				&current->thread.user_sve, current->thread.user_simd, 32));
			current->thread.user_simd[0] = 6;
			current->thread.user_simd[2] = 3;
			current->thread.user_sve.z[0][inactive_offset] = 0xa5;
			regs.pc = instructions;
			regs.pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_Z_BIT |
				PSR_C_BIT | PSR_V_BIT;
			regs.syscallno = NO_SYSCALL;
			current->thread.user_sve.p[0][0] = BIT(0);

			result = tcti_resume_user(current, &regs, current->mm);
			KUNIT_EXPECT_EQ_MSG(test, TCTI_EXIT_SYSCALL, result.reason,
				"source ordinal %u", leaves[index].source_ordinal);
			KUNIT_EXPECT_EQ(test, 0L, result.status);
			KUNIT_EXPECT_EQ(test, instructions + sizeof(u32), result.pc);
			KUNIT_EXPECT_EQ(test, TCTI_SVE_TEST_SVC, result.instruction);
			KUNIT_EXPECT_EQ(test, leaves[index].masked_expected ?
				mask - leaves[index].expected : leaves[index].expected,
				current->thread.user_simd[0] & mask);
			KUNIT_EXPECT_EQ(test, 0xa5,
				current->thread.user_sve.z[0][inactive_offset]);
		}
	}

	KUNIT_EXPECT_EQ(test, 0, vm_munmap(instructions, PAGE_SIZE));
}

static struct kunit_case tcti_sve_resume_test_cases[] = {
	KUNIT_CASE(tcti_sve_resume_executes_predicated_integer_family),
	{}
};

static struct kunit_suite tcti_sve_resume_test_suite = {
	.name = "orlix-tcti-sve-resume",
	.init = tcti_sve_resume_test_init,
	.exit = tcti_sve_resume_test_exit,
	.test_cases = tcti_sve_resume_test_cases,
};
kunit_test_suite(tcti_sve_resume_test_suite);

MODULE_LICENSE("GPL");
