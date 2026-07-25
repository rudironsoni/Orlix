// SPDX-License-Identifier: GPL-2.0-only
/*
 * Source-bound production-path coverage for the base branch-control leaves.
 *
 * The rows below intentionally cover only the pinned AARCHMRS leaves whose
 * decode class and EL0 execution semantics are implemented by the production
 * TCTI decoder and executor. Pointer-authenticated, guarded-control-stack,
 * and other feature-conditioned branch variants remain outside this table
 * until their distinct architecture-defined semantics have owning evidence.
 */
#include <asm/processor.h>
#include <asm/ptrace.h>
#include <asm/tcti.h>
#include <kunit/test.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/syscalls.h>

#include "../decode_aarch64.h"
#include "tcti_test_suites.h"

#define BCS_SVC_NOT_TAKEN 0xd4000021U
#define BCS_SVC_TAKEN 0xd4000041U

enum bcs_kind {
	BCS_B_COND,
	BCS_B,
	BCS_BL,
	BCS_BR,
	BCS_BLR,
	BCS_RET,
	BCS_CBZ,
	BCS_CBNZ,
	BCS_TBZ,
	BCS_TBNZ,
};

struct bcs_leaf {
	u16 ordinal;
	const char *name;
	const char *operation;
	u32 mask;
	u32 pattern;
	enum bcs_kind kind;
	bool wide;
};

/* Pinned source_manifest.def ordinals and source encodings. */
static const struct bcs_leaf bcs_leaves[] = {
	{ 2211, "B_only_condbranch", "B_cond", 0xff000010U, 0x54000000U,
	  BCS_B_COND, false },
	{ 2288, "BR_64_branch_reg", "BR", 0xfffffc1fU, 0xd61f0000U,
	  BCS_BR, true },
	{ 2291, "BLR_64_branch_reg", "BLR", 0xfffffc1fU, 0xd63f0000U,
	  BCS_BLR, true },
	{ 2294, "RET_64R_branch_reg", "RET", 0xfffffc1fU, 0xd65f0000U,
	  BCS_RET, true },
	{ 2312, "B_only_branch_imm", "B_uncond", 0xfc000000U, 0x14000000U,
	  BCS_B, false },
	{ 2313, "BL_only_branch_imm", "BL", 0xfc000000U, 0x94000000U,
	  BCS_BL, false },
	{ 2314, "CBZ_32_compbranch", "CBZ", 0xff000000U, 0x34000000U,
	  BCS_CBZ, false },
	{ 2315, "CBNZ_32_compbranch", "CBNZ", 0xff000000U, 0x35000000U,
	  BCS_CBNZ, false },
	{ 2316, "CBZ_64_compbranch", "CBZ", 0xff000000U, 0xb4000000U,
	  BCS_CBZ, true },
	{ 2317, "CBNZ_64_compbranch", "CBNZ", 0xff000000U, 0xb5000000U,
	  BCS_CBNZ, true },
	{ 2342, "TBZ_only_testbranch", "TBZ", 0x7f000000U, 0x36000000U,
	  BCS_TBZ, false },
	{ 2343, "TBNZ_only_testbranch", "TBNZ", 0x7f000000U, 0x37000000U,
	  BCS_TBNZ, false },
};

static enum tcti_decode_class bcs_decode_class(const struct bcs_leaf *leaf)
{
	switch (leaf->kind) {
	case BCS_B_COND:
		return TCTI_DECODE_CONDITIONAL_BRANCH_IMMEDIATE;
	case BCS_B:
	case BCS_BL:
		return TCTI_DECODE_UNCONDITIONAL_BRANCH_IMMEDIATE;
	case BCS_BR:
	case BCS_BLR:
	case BCS_RET:
		return TCTI_DECODE_UNCONDITIONAL_BRANCH_REGISTER;
	case BCS_CBZ:
	case BCS_CBNZ:
		return TCTI_DECODE_COMPARE_BRANCH_IMMEDIATE;
	case BCS_TBZ:
	case BCS_TBNZ:
		return TCTI_DECODE_TEST_BRANCH_IMMEDIATE;
	}

	return TCTI_DECODE_UNSUPPORTED;
}

static u32 bcs_instruction(const struct bcs_leaf *leaf, bool taken)
{
	/* Both paths are valid, with the taken target at instruction index two. */
	u32 instruction = leaf->pattern;

	switch (leaf->kind) {
	case BCS_B_COND:
		return instruction | (2U << 5);
	case BCS_B:
	case BCS_BL:
		return instruction | 2U;
	case BCS_BR:
	case BCS_BLR:
	case BCS_RET:
		return instruction | (5U << 5);
	case BCS_CBZ:
	case BCS_CBNZ:
		return instruction | (2U << 5) | 5U;
	case BCS_TBZ:
	case BCS_TBNZ:
		return instruction | (2U << 5) | (3U << 19) | 5U;
	}

	return 0;
}

static unsigned long bcs_map_program(struct kunit *test, u32 instruction)
{
	u32 program[] = { instruction, BCS_SVC_NOT_TAKEN, BCS_SVC_TAKEN };
	unsigned long address;
	int ret;

	address = ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
				 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(address));
	ret = tcti_write_user_data(current->mm, address, program,
				   sizeof(program));
	KUNIT_ASSERT_EQ(test, 0, ret);
	ret = sys_mprotect(address, PAGE_SIZE, PROT_READ | PROT_EXEC);
	KUNIT_ASSERT_EQ(test, 0, ret);
	return address;
}

static void bcs_seed_regs(struct pt_regs *regs, unsigned long address,
			  const struct bcs_leaf *leaf, bool taken)
{
	unsigned int index;

	memset(regs, 0, sizeof(*regs));
	for (index = 0; index < ARRAY_SIZE(regs->regs); index++)
		regs->regs[index] = 0x0102030405060708ULL + index;
	regs->pc = address;
	regs->pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_C_BIT | PSR_V_BIT;
	regs->syscallno = NO_SYSCALL;
	regs->regs[5] = address + (taken ? 2 * sizeof(u32) : sizeof(u32));

	if (leaf->kind == BCS_B_COND) {
		/* EQ has condition code zero, so Z selects the taken path. */
		if (taken)
			regs->pstate |= PSR_Z_BIT;
		return;
	}
	if (leaf->kind == BCS_CBZ)
		regs->regs[5] = taken ? 0 : 1;
	else if (leaf->kind == BCS_CBNZ)
		regs->regs[5] = taken ? 1 : 0;
	else if (leaf->kind == BCS_TBZ)
		regs->regs[5] = taken ? 0 : BIT_ULL(3);
	else if (leaf->kind == BCS_TBNZ)
		regs->regs[5] = taken ? BIT_ULL(3) : 0;
}

static void bcs_expect_register_effects(struct kunit *test,
					const struct bcs_leaf *leaf,
					const struct pt_regs *before,
					const struct pt_regs *regs,
					unsigned long address)
{
	unsigned long expected[ARRAY_SIZE(regs->regs)];

	memcpy(expected, before->regs, sizeof(expected));
	if (leaf->kind == BCS_BL || leaf->kind == BCS_BLR)
		expected[30] = address + sizeof(u32);
	KUNIT_EXPECT_MEMEQ(test, expected, regs->regs, sizeof(regs->regs));
	KUNIT_EXPECT_EQ(test, before->pstate, regs->pstate);
}

static bool bcs_can_fall_through(const struct bcs_leaf *leaf)
{
	return leaf->kind == BCS_B_COND || leaf->kind == BCS_CBZ ||
	       leaf->kind == BCS_CBNZ || leaf->kind == BCS_TBZ ||
	       leaf->kind == BCS_TBNZ;
}

static void bcs_source_decode(struct kunit *test)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(bcs_leaves); index++) {
		const struct bcs_leaf *leaf = &bcs_leaves[index];
		struct tcti_decoded_instruction decoded =
			tcti_decode_aarch64(bcs_instruction(leaf, true));

		KUNIT_EXPECT_TRUE_MSG(test, leaf->pattern ==
				      (leaf->pattern & leaf->mask), "%s", leaf->name);
		KUNIT_ASSERT_EQ_MSG(test, bcs_decode_class(leaf),
				    decoded.decode_class, "%s ordinal %u", leaf->name,
				    leaf->ordinal);
		if (leaf->kind == BCS_CBZ || leaf->kind == BCS_CBNZ)
			KUNIT_EXPECT_EQ(test, leaf->wide, decoded.is_64bit);
	}
}

static void bcs_production_resume(struct kunit *test)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(bcs_leaves); index++) {
		const struct bcs_leaf *leaf = &bcs_leaves[index];
		unsigned int taken;

		for (taken = 0; taken < 2; taken++) {
			struct pt_regs regs, before;
			struct tcti_result result;
			unsigned long address;
			u32 instruction;
			u32 expected_svc;

			if (!taken && !bcs_can_fall_through(leaf))
				continue;
			instruction = bcs_instruction(leaf, taken);
			address = bcs_map_program(test, instruction);
			bcs_seed_regs(&regs, address, leaf, taken);
			before = regs;
			result = tcti_resume_user(current, &regs, current->mm);
			expected_svc = taken ? BCS_SVC_TAKEN : BCS_SVC_NOT_TAKEN;

			KUNIT_ASSERT_EQ_MSG(test, TCTI_EXIT_SYSCALL, result.reason,
				    "%s ordinal %u taken=%u", leaf->name, leaf->ordinal,
				    taken);
			KUNIT_EXPECT_EQ(test, expected_svc, result.instruction);
			KUNIT_EXPECT_EQ(test, address +
					(taken ? 3 : 2) * sizeof(u32), regs.pc);
			KUNIT_EXPECT_EQ(test, before.pstate, regs.pstate);
			if (leaf->kind == BCS_BL || leaf->kind == BCS_BLR)
				KUNIT_EXPECT_EQ(test, address + sizeof(u32),
						regs.regs[30]);
			else
				KUNIT_EXPECT_EQ(test, before.regs[30], regs.regs[30]);
			bcs_expect_register_effects(test, leaf, &before, &regs, address);
			KUNIT_EXPECT_EQ(test, 0, vm_munmap(address, PAGE_SIZE));
		}
	}
}

static void bcs_x31_semantics_production(struct kunit *test)
{
	static const struct {
		u32 instruction;
		bool taken;
	} cases[] = {
		/* CBZ wzr always branches. CBNZ wzr always falls through. */
		{ 0x3400005fU, true },
		{ 0x3500005fU, false },
		/* TBZ xzr, #3 branches. TBNZ xzr, #3 falls through. */
		{ 0x3618005fU, true },
		{ 0x3718005fU, false },
	};
	size_t index;

	for (index = 0; index < ARRAY_SIZE(cases); index++) {
		struct pt_regs regs = {};
		struct pt_regs before;
		struct tcti_result result;
		unsigned long address;

		address = bcs_map_program(test, cases[index].instruction);
		bcs_seed_regs(&regs, address, &bcs_leaves[0], cases[index].taken);
		before = regs;
		result = tcti_resume_user(current, &regs, current->mm);
		KUNIT_EXPECT_EQ(test, TCTI_EXIT_SYSCALL, result.reason);
		KUNIT_EXPECT_EQ(test, cases[index].taken ? BCS_SVC_TAKEN :
				BCS_SVC_NOT_TAKEN, result.instruction);
		KUNIT_EXPECT_EQ(test, address +
				(cases[index].taken ? 3 : 2) * sizeof(u32), regs.pc);
		KUNIT_EXPECT_MEMEQ(test, before.regs, regs.regs, sizeof(regs.regs));
		KUNIT_EXPECT_EQ(test, before.pstate, regs.pstate);
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(address, PAGE_SIZE));
	}
}

static void bcs_al_nv_condition_production(struct kunit *test)
{
	static const u8 conditions[] = { 14, 15 };
	size_t index;

	for (index = 0; index < ARRAY_SIZE(conditions); index++) {
		u32 instruction = 0x54000040U | conditions[index];
		struct pt_regs regs = {};
		struct pt_regs before;
		struct tcti_result result;
		unsigned long address;

		KUNIT_EXPECT_EQ(test, TCTI_DECODE_CONDITIONAL_BRANCH_IMMEDIATE,
			tcti_decode_aarch64(instruction).decode_class);
		address = bcs_map_program(test, instruction);
		bcs_seed_regs(&regs, address, &bcs_leaves[0], false);
		before = regs;
		result = tcti_resume_user(current, &regs, current->mm);
		KUNIT_EXPECT_EQ(test, TCTI_EXIT_SYSCALL, result.reason);
		KUNIT_EXPECT_EQ(test, BCS_SVC_TAKEN, result.instruction);
		KUNIT_EXPECT_EQ(test, address + 3 * sizeof(u32), regs.pc);
		KUNIT_EXPECT_MEMEQ(test, before.regs, regs.regs, sizeof(regs.regs));
		KUNIT_EXPECT_EQ(test, before.pstate, regs.pstate);
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(address, PAGE_SIZE));
	}
}

static struct kunit_case bcs_cases[] = {
	KUNIT_CASE(bcs_source_decode),
	KUNIT_CASE(bcs_production_resume),
	KUNIT_CASE(bcs_x31_semantics_production),
	KUNIT_CASE(bcs_al_nv_condition_production),
	{}
};

static struct kunit_suite tcti_branch_control_source_bound_test_suite = {
	.name = "orlix-tcti-branch-control-source-bound",
	.test_cases = bcs_cases,
};

kunit_test_suite(tcti_branch_control_source_bound_test_suite);
