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

#include "../decode_aarch64.h"

#define TCTI_LSE128_SVC 0xd4000001U

struct tcti_lse128_value {
	u64 low;
	u64 high;
};

struct tcti_lse128_leaf {
	enum tcti_lse_atomic_op operation;
	u32 instruction;
};

static const struct tcti_lse128_leaf tcti_lse128_leaves[] = {
	{ TCTI_LSE_ATOMIC_CLR, 0x19201000U },
	{ TCTI_LSE_ATOMIC_SET, 0x19203000U },
	{ TCTI_LSE_ATOMIC_SWP, 0x19208000U },
	{ TCTI_LSE_ATOMIC_CLR, 0x19601000U },
	{ TCTI_LSE_ATOMIC_SET, 0x19603000U },
	{ TCTI_LSE_ATOMIC_SWP, 0x19608000U },
	{ TCTI_LSE_ATOMIC_CLR, 0x19a01000U },
	{ TCTI_LSE_ATOMIC_SET, 0x19a03000U },
	{ TCTI_LSE_ATOMIC_SWP, 0x19a08000U },
	{ TCTI_LSE_ATOMIC_CLR, 0x19e01000U },
	{ TCTI_LSE_ATOMIC_SET, 0x19e03000U },
	{ TCTI_LSE_ATOMIC_SWP, 0x19e08000U },
};

static int tcti_lse128_resume_test_init(struct kunit *test)
{
	struct mm_struct *mm = mm_alloc();

	if (!mm)
		return -ENOMEM;
	kthread_use_mm(mm);
	test->priv = mm;
	return 0;
}

static void tcti_lse128_resume_test_exit(struct kunit *test)
{
	struct mm_struct *mm = test->priv;

	if (!mm)
		return;
	kthread_unuse_mm(mm);
	mmput(mm);
}

static u32 tcti_lse128_instruction(const struct tcti_lse128_leaf *leaf,
				   u8 rt2, u8 rn, u8 rt)
{
	return leaf->instruction | ((u32)rt2 << 16) | ((u32)rn << 5) | rt;
}

static struct tcti_lse128_value
tcti_lse128_expected(const struct tcti_lse128_leaf *leaf,
			     const struct tcti_lse128_value *initial,
			     const struct tcti_lse128_value *operand)
{
	struct tcti_lse128_value expected;

	switch (leaf->operation) {
	case TCTI_LSE_ATOMIC_CLR:
		expected.low = initial->low & ~operand->low;
		expected.high = initial->high & ~operand->high;
		break;
	case TCTI_LSE_ATOMIC_SET:
		expected.low = initial->low | operand->low;
		expected.high = initial->high | operand->high;
		break;
	default:
		expected = *operand;
		break;
	}
	return expected;
}

static unsigned long tcti_lse128_map(struct kunit *test, int prot)
{
	unsigned long mapped;

	mapped = ksys_mmap_pgoff(0, PAGE_SIZE, prot,
				 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_EXPECT_FALSE(test, IS_ERR_VALUE(mapped));
	return mapped;
}

static int tcti_lse128_write_program(unsigned long address,
				    const u32 *program, size_t count)
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

static void tcti_lse128_resume_all_current_leaves(struct kunit *test)
{
	const struct tcti_lse128_value initial = {
		.low = 0x0123456789abcdefULL,
		.high = 0xfedcba9876543210ULL,
	};
	const struct tcti_lse128_value operand = {
		.low = 0x0f0f55ffaa5500ffULL,
		.high = 0x33cc0ff0f00f5aa5ULL,
	};
	unsigned long instructions = tcti_lse128_map(test, PROT_READ | PROT_WRITE);
	unsigned long data = tcti_lse128_map(test, PROT_READ | PROT_WRITE);
	size_t index;

	if (IS_ERR_VALUE(instructions) || IS_ERR_VALUE(data))
		return;
	for (index = 0; index < ARRAY_SIZE(tcti_lse128_leaves); index++) {
		const struct tcti_lse128_leaf *leaf = &tcti_lse128_leaves[index];
		const u32 program[] = {
			tcti_lse128_instruction(leaf, 6, 10, 8), TCTI_LSE128_SVC,
		};
		const struct tcti_lse128_value expected =
			tcti_lse128_expected(leaf, &initial, &operand);
		struct tcti_lse128_value observed = {};
		struct pt_regs regs = {};
		struct tcti_result result;
		int ret;

		ret = tcti_lse128_write_program(instructions, program,
						ARRAY_SIZE(program));
		KUNIT_ASSERT_EQ(test, 0, ret);
		ret = tcti_write_user_data(current->mm, data, &initial,
					   sizeof(initial));
		KUNIT_ASSERT_EQ(test, 0, ret);
		regs.regs[8] = operand.low;
		regs.regs[6] = operand.high;
		regs.regs[10] = data;
		regs.pc = instructions;
		regs.pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_Z_BIT |
			PSR_C_BIT | PSR_V_BIT;
		regs.syscallno = NO_SYSCALL;

		result = tcti_resume_user(current, &regs, current->mm);
		KUNIT_EXPECT_EQ_MSG(test, TCTI_EXIT_SYSCALL, result.reason,
			"leaf=%zu", index);
		KUNIT_EXPECT_EQ_MSG(test, 0L, result.status, "leaf=%zu", index);
		KUNIT_EXPECT_EQ_MSG(test, instructions + sizeof(u32), result.pc,
			"leaf=%zu", index);
		KUNIT_EXPECT_EQ_MSG(test, TCTI_LSE128_SVC, result.instruction,
			"leaf=%zu", index);
		KUNIT_EXPECT_EQ_MSG(test, initial.low, regs.regs[8], "leaf=%zu", index);
		KUNIT_EXPECT_EQ_MSG(test, initial.high, regs.regs[6], "leaf=%zu", index);
		KUNIT_EXPECT_EQ_MSG(test, instructions + sizeof(u32), regs.pc,
			"leaf=%zu", index);
		KUNIT_EXPECT_EQ_MSG(test, PSR_MODE_EL0t | PSR_N_BIT | PSR_Z_BIT |
			PSR_C_BIT | PSR_V_BIT, regs.pstate, "leaf=%zu", index);
		ret = tcti_read_user_data(current->mm, data, &observed,
					  sizeof(observed));
		KUNIT_ASSERT_EQ(test, 0, ret);
		KUNIT_EXPECT_MEMEQ(test, &expected, &observed, sizeof(observed));
	}

	KUNIT_EXPECT_EQ(test, 0, vm_munmap(data, PAGE_SIZE));
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(instructions, PAGE_SIZE));
}

static void tcti_lse128_resume_fault_does_not_mutate(struct kunit *test)
{
	const struct tcti_lse128_value initial = {
		.low = 0x1111222233334444ULL,
		.high = 0xaaaabbbbccccddddULL,
	};
	const struct tcti_lse128_leaf leaf = {
		TCTI_LSE_ATOMIC_SWP, 0x19e08000U,
	};
	const u32 program[] = {
		tcti_lse128_instruction(&leaf, 6, 10, 8), TCTI_LSE128_SVC,
	};
	unsigned long instructions = tcti_lse128_map(test, PROT_READ | PROT_WRITE);
	unsigned long data = tcti_lse128_map(test, PROT_READ | PROT_WRITE);
	struct tcti_lse128_value observed = {};
	struct pt_regs regs = {};
	struct pt_regs before;
	struct tcti_result result;
	int ret;

	if (IS_ERR_VALUE(instructions) || IS_ERR_VALUE(data))
		return;
	ret = tcti_lse128_write_program(instructions, program,
					ARRAY_SIZE(program));
	KUNIT_ASSERT_EQ(test, 0, ret);
	ret = tcti_write_user_data(current->mm, data, &initial, sizeof(initial));
	KUNIT_ASSERT_EQ(test, 0, ret);
	regs.regs[8] = U64_MAX;
	regs.regs[6] = U64_MAX - 1;
	regs.regs[10] = data;
	regs.pc = instructions;
	regs.pstate = PSR_MODE_EL0t | PSR_N_BIT;
	regs.syscallno = NO_SYSCALL;
	before = regs;
	KUNIT_ASSERT_EQ(test, 0, sys_mprotect(data, PAGE_SIZE, PROT_READ));

	result = tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, TCTI_EXIT_USER_FAULT, result.reason);
	KUNIT_EXPECT_EQ(test, -EACCES, result.status);
	KUNIT_EXPECT_EQ(test, data, result.fault_address);
	KUNIT_EXPECT_EQ(test, TCTI_ACCESS_WRITE, result.fault_access);
	KUNIT_EXPECT_EQ(test, instructions, result.pc);
	KUNIT_EXPECT_EQ(test, program[0], result.instruction);
	KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
	ret = tcti_read_user_data(current->mm, data, &observed, sizeof(observed));
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_MEMEQ(test, &initial, &observed, sizeof(observed));
	KUNIT_EXPECT_EQ(test, 0, sys_mprotect(data, PAGE_SIZE,
					PROT_READ | PROT_WRITE));
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(data, PAGE_SIZE));
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(instructions, PAGE_SIZE));
}

static struct kunit_case tcti_lse128_resume_test_cases[] = {
	KUNIT_CASE(tcti_lse128_resume_all_current_leaves),
	KUNIT_CASE(tcti_lse128_resume_fault_does_not_mutate),
	{}
};

static struct kunit_suite tcti_lse128_resume_test_suite = {
	.name = "orlix-tcti-lse128-resume",
	.init = tcti_lse128_resume_test_init,
	.exit = tcti_lse128_resume_test_exit,
	.test_cases = tcti_lse128_resume_test_cases,
};
kunit_test_suite(tcti_lse128_resume_test_suite);

MODULE_LICENSE("GPL");
