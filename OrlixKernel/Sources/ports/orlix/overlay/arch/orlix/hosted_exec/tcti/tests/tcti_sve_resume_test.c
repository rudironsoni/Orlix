// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
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

static u32 tcti_sve_predicated_binary_instruction(u32 encoding, u8 opcode,
						   u8 size, u8 pg, u8 zm, u8 zd)
{
	return encoding | ((u32)size << 22) | ((u32)opcode << 16) |
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

/*
 * The pinned source contains the five decoded SVE leaves, but the separately
 * pinned official shared-ASL corpus required to prove their semantics is not
 * available.  Until that source-to-semantics edge exists, all five must reach
 * the same production unsupported result without changing guest state.
 */
static void tcti_sve_resume_rejects_unproved_extension_leaves(struct kunit *test)
{
	static const struct {
		u32 encoding;
		u8 opcode;
	} leaves[] = {
		{ AARCH64_SVE_PREDICATED_ARITHMETIC, 0 }, /* add_z_p_zz_ */
		{ AARCH64_SVE_PREDICATED_ARITHMETIC, 1 }, /* sub_z_p_zz_ */
		{ AARCH64_SVE_PREDICATED_LOGICAL, 0 }, /* orr_z_p_zz_ */
		{ AARCH64_SVE_PREDICATED_LOGICAL, 1 }, /* eor_z_p_zz_ */
		{ AARCH64_SVE_PREDICATED_LOGICAL, 2 }, /* and_z_p_zz_ */
	};
	unsigned long instructions;
	size_t index;
	int ret;

	instructions = ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
					 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(instructions));

	for (index = 0; index < ARRAY_SIZE(leaves); index++) {
		u32 sve_instruction = tcti_sve_predicated_binary_instruction(
			leaves[index].encoding, leaves[index].opcode, 0, 0, 1, 0);
		const u32 program[] = { sve_instruction, TCTI_SVE_TEST_SVC };
		struct pt_regs regs = {};
		struct pt_regs before;
		struct tcti_sve_state sve_before;
		unsigned long simd_before[ARRAY_SIZE(current->thread.user_simd)];
		struct tcti_result result;

		ret = tcti_sve_write_program(instructions, program,
					     ARRAY_SIZE(program));
		KUNIT_ASSERT_EQ(test, 0, ret);

		memset(&current->thread.user_sve, 0x5a,
		       sizeof(current->thread.user_sve));
		memset(current->thread.user_simd, 0xa5,
		       sizeof(current->thread.user_simd));
		memcpy(&sve_before, &current->thread.user_sve, sizeof(sve_before));
		memcpy(simd_before, current->thread.user_simd, sizeof(simd_before));
		regs.pc = instructions;
		regs.pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_Z_BIT |
			PSR_C_BIT | PSR_V_BIT;
		regs.syscallno = NO_SYSCALL;
		before = regs;

		result = tcti_resume_user(current, &regs, current->mm);
		KUNIT_EXPECT_EQ(test, TCTI_EXIT_UNSUPPORTED_INSTRUCTION,
				result.reason);
		KUNIT_EXPECT_EQ(test, -EOPNOTSUPP, result.status);
		KUNIT_EXPECT_EQ(test, instructions, result.pc);
		KUNIT_EXPECT_EQ(test, sve_instruction, result.instruction);
		KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
		KUNIT_EXPECT_MEMEQ(test, &sve_before, &current->thread.user_sve,
				  sizeof(sve_before));
		KUNIT_EXPECT_MEMEQ(test, simd_before, current->thread.user_simd,
				  sizeof(simd_before));
	}

	KUNIT_EXPECT_EQ(test, 0, vm_munmap(instructions, PAGE_SIZE));
}

static struct kunit_case tcti_sve_resume_test_cases[] = {
	KUNIT_CASE(tcti_sve_resume_rejects_unproved_extension_leaves),
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
