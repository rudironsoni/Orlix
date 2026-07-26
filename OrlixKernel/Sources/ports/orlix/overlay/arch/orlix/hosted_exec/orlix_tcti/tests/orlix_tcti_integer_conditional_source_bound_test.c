// SPDX-License-Identifier: GPL-2.0-only
#include <asm/ptrace.h>
#include <asm/orlix_tcti.h>
#include <kunit/test.h>
#include <linux/bitops.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/syscalls.h>

#include "../decode_aarch64.h"
#include "target_instruction_artifact.h"

#define INTEGER_CONDITIONAL_SVC 0xd4000001U
#define INTEGER_CONDITIONAL_NZCV (PSR_N_BIT | PSR_Z_BIT | PSR_C_BIT | PSR_V_BIT)

struct orlix_tcti_integer_conditional_leaf {
	u16 ordinal;
	const char *name;
	u32 mask;
	u32 pattern;
	enum orlix_tcti_decode_class decode_class;
};

/* Direct leaves from the pinned AARCHMRS 2026-06 source manifest. */
static const struct orlix_tcti_integer_conditional_leaf
    orlix_tcti_integer_conditional_leaves[] = {
	{3356, "UDIV_32_dp_2src", 0xffe0fc00U, 0x1ac00800U,
	 ORLIX_TCTI_DECODE_DATA_PROCESSING_2SOURCE},
	{3357, "SDIV_32_dp_2src", 0xffe0fc00U, 0x1ac00c00U,
	 ORLIX_TCTI_DECODE_DATA_PROCESSING_2SOURCE},
	{3373, "UDIV_64_dp_2src", 0xffe0fc00U, 0x9ac00800U,
	 ORLIX_TCTI_DECODE_DATA_PROCESSING_2SOURCE},
	{3374, "SDIV_64_dp_2src", 0xffe0fc00U, 0x9ac00c00U,
	 ORLIX_TCTI_DECODE_DATA_PROCESSING_2SOURCE},
	{3479, "CCMN_32_condcmp_reg", 0xffe00c10U, 0x3a400000U,
	 ORLIX_TCTI_DECODE_CONDITIONAL_COMPARE},
	{3480, "CCMP_32_condcmp_reg", 0xffe00c10U, 0x7a400000U,
	 ORLIX_TCTI_DECODE_CONDITIONAL_COMPARE},
	{3481, "CCMN_64_condcmp_reg", 0xffe00c10U, 0xba400000U,
	 ORLIX_TCTI_DECODE_CONDITIONAL_COMPARE},
	{3482, "CCMP_64_condcmp_reg", 0xffe00c10U, 0xfa400000U,
	 ORLIX_TCTI_DECODE_CONDITIONAL_COMPARE},
	{3483, "CCMN_32_condcmp_imm", 0xffe00c10U, 0x3a400800U,
	 ORLIX_TCTI_DECODE_CONDITIONAL_COMPARE},
	{3484, "CCMP_32_condcmp_imm", 0xffe00c10U, 0x7a400800U,
	 ORLIX_TCTI_DECODE_CONDITIONAL_COMPARE},
	{3485, "CCMN_64_condcmp_imm", 0xffe00c10U, 0xba400800U,
	 ORLIX_TCTI_DECODE_CONDITIONAL_COMPARE},
	{3486, "CCMP_64_condcmp_imm", 0xffe00c10U, 0xfa400800U,
	 ORLIX_TCTI_DECODE_CONDITIONAL_COMPARE},
	{3487, "CSEL_32_condsel", 0xffe00c00U, 0x1a800000U,
	 ORLIX_TCTI_DECODE_CONDITIONAL_SELECT},
	{3488, "CSINC_32_condsel", 0xffe00c00U, 0x1a800400U,
	 ORLIX_TCTI_DECODE_CONDITIONAL_SELECT},
	{3489, "CSINV_32_condsel", 0xffe00c00U, 0x5a800000U,
	 ORLIX_TCTI_DECODE_CONDITIONAL_SELECT},
	{3490, "CSNEG_32_condsel", 0xffe00c00U, 0x5a800400U,
	 ORLIX_TCTI_DECODE_CONDITIONAL_SELECT},
	{3491, "CSEL_64_condsel", 0xffe00c00U, 0x9a800000U,
	 ORLIX_TCTI_DECODE_CONDITIONAL_SELECT},
	{3492, "CSINC_64_condsel", 0xffe00c00U, 0x9a800400U,
	 ORLIX_TCTI_DECODE_CONDITIONAL_SELECT},
	{3493, "CSINV_64_condsel", 0xffe00c00U, 0xda800000U,
	 ORLIX_TCTI_DECODE_CONDITIONAL_SELECT},
	{3494, "CSNEG_64_condsel", 0xffe00c00U, 0xda800400U,
	 ORLIX_TCTI_DECODE_CONDITIONAL_SELECT},
	{3495, "MADD_32A_dp_3src", 0xffe08000U, 0x1b000000U,
	 ORLIX_TCTI_DECODE_MULTIPLY_ADD_SUB},
	{3496, "MSUB_32A_dp_3src", 0xffe08000U, 0x1b008000U,
	 ORLIX_TCTI_DECODE_MULTIPLY_ADD_SUB},
	{3497, "MADD_64A_dp_3src", 0xffe08000U, 0x9b000000U,
	 ORLIX_TCTI_DECODE_MULTIPLY_ADD_SUB},
	{3498, "MSUB_64A_dp_3src", 0xffe08000U, 0x9b008000U,
	 ORLIX_TCTI_DECODE_MULTIPLY_ADD_SUB},
	{3499, "SMADDL_64WA_dp_3src", 0xffe08000U, 0x9b200000U,
	 ORLIX_TCTI_DECODE_MULTIPLY_ADD_SUB},
	{3500, "SMSUBL_64WA_dp_3src", 0xffe08000U, 0x9b208000U,
	 ORLIX_TCTI_DECODE_MULTIPLY_ADD_SUB},
	{3501, "SMULH_64_dp_3src", 0xffe0fc00U, 0x9b407c00U,
	 ORLIX_TCTI_DECODE_MULTIPLY_ADD_SUB},
	{3504, "UMADDL_64WA_dp_3src", 0xffe08000U, 0x9ba00000U,
	 ORLIX_TCTI_DECODE_MULTIPLY_ADD_SUB},
	{3505, "UMSUBL_64WA_dp_3src", 0xffe08000U, 0x9ba08000U,
	 ORLIX_TCTI_DECODE_MULTIPLY_ADD_SUB},
	{3506, "UMULH_64_dp_3src", 0xffe0fc00U, 0x9bc07c00U,
	 ORLIX_TCTI_DECODE_MULTIPLY_ADD_SUB},
};

static const char *orlix_tcti_integer_conditional_artifact_string(
	const struct orlix_tcti_target_instruction_artifact *artifact, u32 offset)
{
	if (offset >= artifact->string_pool_size)
		return NULL;

	return (const char *)artifact->string_pool + offset;
}

static unsigned long orlix_tcti_integer_conditional_map(struct kunit *test,
						  u32 instruction) {
	const u32 program[] = {instruction, INTEGER_CONDITIONAL_SVC};
	unsigned long mapped;
	int ret;

	mapped = ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
				 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(mapped));
	ret =
	    orlix_tcti_write_user_data(current->mm, mapped, program, sizeof(program));
	KUNIT_ASSERT_EQ(test, 0, ret);
	ret = sys_mprotect(mapped, PAGE_SIZE, PROT_READ | PROT_EXEC);
	KUNIT_ASSERT_EQ(test, 0, ret);
	return mapped;
}

static struct orlix_tcti_result orlix_tcti_integer_conditional_run(struct kunit *test,
						       u32 instruction,
						       struct pt_regs *regs,
						       unsigned long *mapped) {
	struct orlix_tcti_result result;

	*mapped = orlix_tcti_integer_conditional_map(test, instruction);
	regs->pc = *mapped;
	regs->pstate |= PSR_MODE_EL0t;
	regs->syscallno = NO_SYSCALL;
	result = orlix_tcti_resume_user(current, regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_EXPECT_EQ(test, 0L, result.status);
	KUNIT_EXPECT_EQ(test, INTEGER_CONDITIONAL_SVC, result.instruction);
	KUNIT_EXPECT_EQ(test, *mapped + sizeof(u32), result.pc);
	KUNIT_EXPECT_EQ(test, *mapped + sizeof(u32), regs->pc);
	KUNIT_EXPECT_EQ(test, (unsigned long)PSR_MODE_EL0t,
			regs->pstate & PSR_MODE_MASK);
	return result;
}

static void orlix_tcti_integer_conditional_source_bindings(struct kunit *test) {
	const struct orlix_tcti_target_instruction_artifact *artifact =
		orlix_tcti_target_instruction_artifact_canonical();
	struct orlix_tcti_target_instruction_artifact_validation_result validation;
	size_t index;

	KUNIT_ASSERT_NOT_NULL(test, artifact);
	KUNIT_ASSERT_EQ(test, 0,
			orlix_tcti_target_instruction_artifact_validate(artifact,
								    &validation));
	KUNIT_ASSERT_EQ(test, 4350U, artifact->leaf_count);
	for (index = 0; index < ARRAY_SIZE(orlix_tcti_integer_conditional_leaves);
	     index++) {
		const struct orlix_tcti_integer_conditional_leaf *leaf =
		    &orlix_tcti_integer_conditional_leaves[index];
		const struct orlix_tcti_target_instruction_artifact_leaf *source;
		const char *source_name;
		struct orlix_tcti_decoded_instruction decoded =
		    orlix_tcti_decode_aarch64(leaf->pattern);

		KUNIT_EXPECT_GT(test, leaf->ordinal, (u16)0);
		KUNIT_ASSERT_LT(test, (u32)leaf->ordinal, artifact->leaf_count);
		source = &artifact->leaves[leaf->ordinal];
		source_name = orlix_tcti_integer_conditional_artifact_string(
			artifact, source->name_offset);
		KUNIT_ASSERT_NOT_NULL(test, source_name);
		KUNIT_EXPECT_STREQ(test, leaf->name, source_name);
		KUNIT_EXPECT_EQ(test, leaf->mask, source->encoding_mask);
		KUNIT_EXPECT_EQ(test, leaf->pattern, source->encoding_pattern);
		KUNIT_EXPECT_EQ(test, leaf->pattern,
				leaf->pattern & leaf->mask);
		KUNIT_EXPECT_EQ_MSG(test, leaf->decode_class,
				    decoded.decode_class, "%s", leaf->name);
	}
}

static u32 orlix_tcti_integer_conditional_dp2(u32 pattern, u8 rd, u8 rn, u8 rm)
{
	return pattern | ((u32)rm << 16) | ((u32)rn << 5) | rd;
}

static u32 orlix_tcti_integer_conditional_compare(u32 pattern, u8 rn, u8 rm_or_imm,
					     u8 condition, u8 nzcv)
{
	return pattern | ((u32)rm_or_imm << 16) | ((u32)condition << 12) |
	       ((u32)rn << 5) | nzcv;
}

static u32 orlix_tcti_integer_conditional_select(u32 pattern, u8 rd, u8 rn, u8 rm,
					    u8 condition)
{
	return pattern | ((u32)rm << 16) | ((u32)condition << 12) |
	       ((u32)rn << 5) | rd;
}

static u32 orlix_tcti_integer_conditional_multiply(u32 pattern, u8 rd, u8 rn,
					      u8 rm, u8 ra)
{
	return pattern | ((u32)rm << 16) | ((u32)ra << 10) |
	       ((u32)rn << 5) | rd;
}

static void orlix_tcti_integer_conditional_expect_preserved(
	struct kunit *test, const struct pt_regs *before, const struct pt_regs *after,
	bool check_ra)
{
	KUNIT_EXPECT_EQ(test, before->regs[1], after->regs[1]);
	KUNIT_EXPECT_EQ(test, before->regs[2], after->regs[2]);
	if (check_ra)
		KUNIT_EXPECT_EQ(test, before->regs[3], after->regs[3]);
}

static void
orlix_tcti_integer_conditional_divide_corners_production_path(struct kunit *test) {
	static const struct {
		u32 instruction;
		u64 dividend;
		u64 divisor;
		u64 expected;
	} cases[] = {
	    {0x1ac00800U, U32_MAX, 0, 0},
	    {0x1ac00c00U, BIT_ULL(31), U32_MAX, BIT_ULL(31)},
	    {0x9ac00800U, U64_MAX, 0, 0},
	    {0x9ac00c00U, BIT_ULL(63), U64_MAX, BIT_ULL(63)},
	};
	size_t index;

	for (index = 0; index < ARRAY_SIZE(cases); index++) {
		struct pt_regs regs = {};
		struct pt_regs before;
		unsigned long mapped;

		regs.regs[1] = cases[index].dividend;
		regs.regs[2] = cases[index].divisor;
		regs.pstate =
		    PSR_MODE_EL0t |
		    (0x155UL & ~(INTEGER_CONDITIONAL_NZCV | PSR_MODE_MASK)) |
		    PSR_N_BIT | PSR_C_BIT;
		before = regs;
		orlix_tcti_integer_conditional_run(test,
			orlix_tcti_integer_conditional_dp2(cases[index].instruction, 0, 1, 2),
					     &regs, &mapped);
		KUNIT_EXPECT_EQ(test, cases[index].expected, regs.regs[0]);
		KUNIT_EXPECT_EQ(test, before.regs[1], regs.regs[1]);
		KUNIT_EXPECT_EQ(test, before.regs[2], regs.regs[2]);
		KUNIT_EXPECT_EQ(test, before.pstate, regs.pstate);
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
	}
}

static void orlix_tcti_integer_conditional_all_divide_leaves_production_path(
	struct kunit *test)
{
	static const struct {
		u32 pattern;
		u64 left;
		u64 right;
		u64 expected;
	} cases[] = {
		{0x1ac00800U, 0xf0000000ULL, 3, 0x50000000ULL},
		{0x1ac00c00U, 0xffffffebULL, 4, 0xfffffffbULL},
		{0x9ac00800U, 0xfedcba9876543210ULL, 0x10,
		 0x0fedcba987654321ULL},
		{0x9ac00c00U, (u64)-21, 4, (u64)-5},
	};
	unsigned long base_pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_C_BIT;
	size_t index;

	for (index = 0; index < ARRAY_SIZE(cases); index++) {
		struct pt_regs regs = {};
		struct pt_regs before;
		unsigned long mapped;

		regs.regs[1] = cases[index].left;
		regs.regs[2] = cases[index].right;
		regs.regs[3] = 0x5555555555555555ULL;
		regs.pstate = base_pstate;
		before = regs;
		orlix_tcti_integer_conditional_run(test,
			orlix_tcti_integer_conditional_dp2(cases[index].pattern, 0, 1, 2),
			&regs, &mapped);
		KUNIT_EXPECT_EQ(test, cases[index].expected, regs.regs[0]);
		orlix_tcti_integer_conditional_expect_preserved(test, &before, &regs,
			true);
		KUNIT_EXPECT_EQ(test, base_pstate, regs.pstate);
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
	}
}

static void orlix_tcti_integer_conditional_compare_leaves_production_path(
	struct kunit *test)
{
	static const struct {
		u32 pattern;
		bool subtract;
		bool immediate;
	} cases[] = {
		{0x3a400000U, false, false},
		{0x7a400000U, true, false},
		{0xba400000U, false, false},
		{0xfa400000U, true, false},
		{0x3a400800U, false, true},
		{0x7a400800U, true, true},
		{0xba400800U, false, true},
		{0xfa400800U, true, true},
	};
	unsigned long base_pstate = PSR_MODE_EL0t |
		(0x155UL & ~(INTEGER_CONDITIONAL_NZCV | PSR_MODE_MASK));
	size_t index;

	for (index = 0; index < ARRAY_SIZE(cases); index++) {
		struct pt_regs regs = {};
		struct pt_regs before;
		unsigned long mapped;
		u64 left = cases[index].subtract ? 5 : 1;
		u64 right = cases[index].immediate ? 2 :
			(cases[index].subtract ? 2 : 2);

		regs.regs[1] = left;
		regs.regs[2] = 2;
		regs.regs[3] = 0xaaaaaaaaaaaaaaaaULL;
		regs.pstate = base_pstate | PSR_Z_BIT;
		before = regs;
		orlix_tcti_integer_conditional_run(test,
			orlix_tcti_integer_conditional_compare(cases[index].pattern, 1,
				right, 0, 0), &regs, &mapped);
		orlix_tcti_integer_conditional_expect_preserved(test, &before, &regs,
			true);
		KUNIT_EXPECT_EQ(test, base_pstate |
			(cases[index].subtract ? PSR_C_BIT : 0), regs.pstate);
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));

		/* A false condition must take the encoded immediate NZCV path. */
		memset(&regs, 0, sizeof(regs));
		regs.regs[1] = left;
		regs.regs[2] = 2;
		regs.regs[3] = 0xaaaaaaaaaaaaaaaaULL;
		regs.pstate = base_pstate | PSR_C_BIT;
		before = regs;
		orlix_tcti_integer_conditional_run(test,
			orlix_tcti_integer_conditional_compare(cases[index].pattern, 1,
				right, 0, 0xb), &regs, &mapped);
		orlix_tcti_integer_conditional_expect_preserved(test, &before, &regs,
			true);
		KUNIT_EXPECT_EQ(test, base_pstate | PSR_N_BIT | PSR_C_BIT |
			PSR_V_BIT, regs.pstate);
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
	}
}

static void orlix_tcti_integer_conditional_select_leaves_production_path(
	struct kunit *test)
{
	static const struct {
		u32 pattern;
		bool is_64bit;
		u64 false_result;
	} cases[] = {
		{0x1a800000U, false, 0x00000007ULL},
		{0x1a800400U, false, 0x00000008ULL},
		{0x5a800000U, false, 0xfffffff8ULL},
		{0x5a800400U, false, 0xfffffff9ULL},
		{0x9a800000U, true, 7},
		{0x9a800400U, true, 8},
		{0xda800000U, true, ~7ULL},
		{0xda800400U, true, (u64)-7},
	};
	unsigned long base_pstate = PSR_MODE_EL0t |
		(0x155UL & ~(INTEGER_CONDITIONAL_NZCV | PSR_MODE_MASK));
	size_t index;

	for (index = 0; index < ARRAY_SIZE(cases); index++) {
		struct pt_regs regs = {};
		struct pt_regs before;
		unsigned long mapped;
		u64 true_result = cases[index].is_64bit ?
			0x1111111111111111ULL : 0x00000000deadbeefULL;

		regs.regs[1] = true_result;
		regs.regs[2] = 7;
		regs.regs[3] = 0x3333333333333333ULL;
		regs.pstate = base_pstate | PSR_Z_BIT;
		before = regs;
		orlix_tcti_integer_conditional_run(test,
			orlix_tcti_integer_conditional_select(cases[index].pattern, 0, 1,
				2, 0), &regs, &mapped);
		KUNIT_EXPECT_EQ(test, true_result, regs.regs[0]);
		orlix_tcti_integer_conditional_expect_preserved(test, &before, &regs,
			true);
		KUNIT_EXPECT_EQ(test, before.pstate, regs.pstate);
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));

		memset(&regs, 0, sizeof(regs));
		regs.regs[1] = true_result;
		regs.regs[2] = 7;
		regs.regs[3] = 0x3333333333333333ULL;
		regs.pstate = base_pstate | PSR_C_BIT;
		before = regs;
		orlix_tcti_integer_conditional_run(test,
			orlix_tcti_integer_conditional_select(cases[index].pattern, 0, 1,
				2, 0), &regs, &mapped);
		KUNIT_EXPECT_EQ(test, cases[index].false_result, regs.regs[0]);
		orlix_tcti_integer_conditional_expect_preserved(test, &before, &regs,
			true);
		KUNIT_EXPECT_EQ(test, before.pstate, regs.pstate);
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
	}
}

static void orlix_tcti_integer_conditional_multiply_leaves_production_path(
	struct kunit *test)
{
	static const struct {
		u32 pattern;
		u64 left;
		u64 right;
		u64 accumulator;
		u64 expected;
		bool has_accumulator;
	} cases[] = {
		{0x1b000000U, 0xfffffffeULL, 3, 5, 0xffffffffULL, true},
		{0x1b008000U, 0xfffffffeULL, 3, 5, 11, true},
		{0x9b000000U, 6, 7, 5, 47, true},
		{0x9b008000U, 6, 7, 50, 8, true},
		{0x9b200000U, 0xfffffffeULL, 3, 5, (u64)-1, true},
		{0x9b208000U, 0xfffffffeULL, 3, 5, 11, true},
		{0x9b407c00U, (u64)-2, 3, 0, (u64)-1, false},
		{0x9ba00000U, 0xfffffffeULL, 3, 5, 0x2ffffffffULL, true},
		{0x9ba08000U, 0xfffffffeULL, 3, 5,
		  0xfffffffd00000007ULL, true},
		{0x9bc07c00U, U64_MAX, 3, 0, 2, false},
	};
	unsigned long base_pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_C_BIT;
	size_t index;

	for (index = 0; index < ARRAY_SIZE(cases); index++) {
		struct pt_regs regs = {};
		struct pt_regs before;
		unsigned long mapped;
		u8 ra = cases[index].has_accumulator ? 3 : 31;

		regs.regs[1] = cases[index].left;
		regs.regs[2] = cases[index].right;
		regs.regs[3] = cases[index].accumulator;
		regs.pstate = base_pstate;
		before = regs;
		orlix_tcti_integer_conditional_run(test,
			orlix_tcti_integer_conditional_multiply(cases[index].pattern, 0, 1,
				2, ra), &regs, &mapped);
		KUNIT_EXPECT_EQ(test, cases[index].expected, regs.regs[0]);
		orlix_tcti_integer_conditional_expect_preserved(test, &before, &regs,
			cases[index].has_accumulator);
		KUNIT_EXPECT_EQ(test, base_pstate, regs.pstate);
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
	}
}

static void orlix_tcti_integer_conditional_x31_is_zero_and_not_writable(
	struct kunit *test)
{
	struct pt_regs regs = {};
	struct pt_regs before;
	unsigned long mapped;

	regs.regs[1] = 0x1234;
	regs.regs[2] = 7;
	regs.regs[30] = 0x5555555555555555ULL;
	regs.pstate = PSR_MODE_EL0t | PSR_Z_BIT;
	before = regs;
	orlix_tcti_integer_conditional_run(test,
		orlix_tcti_integer_conditional_select(0x9a800000U, 0, 31, 2, 0),
		&regs, &mapped);
	KUNIT_EXPECT_EQ(test, 0ULL, regs.regs[0]);
	KUNIT_EXPECT_EQ(test, before.regs[1], regs.regs[1]);
	KUNIT_EXPECT_EQ(test, before.regs[2], regs.regs[2]);
	KUNIT_EXPECT_EQ(test, before.regs[30], regs.regs[30]);
	KUNIT_EXPECT_EQ(test, before.pstate, regs.pstate);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));

	memset(&regs, 0, sizeof(regs));
	regs.regs[1] = 0x1234;
	regs.regs[2] = 7;
	regs.regs[30] = 0x5555555555555555ULL;
	regs.pstate = PSR_MODE_EL0t | PSR_Z_BIT;
	before = regs;
	orlix_tcti_integer_conditional_run(test,
		orlix_tcti_integer_conditional_select(0x1a800000U, 31, 1, 2, 0),
		&regs, &mapped);
	KUNIT_EXPECT_MEMEQ(test, before.regs, regs.regs, sizeof(regs.regs));
	KUNIT_EXPECT_EQ(test, before.pstate, regs.pstate);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
}

static void orlix_tcti_integer_conditional_reserved_encodings_fail_before_state(
    struct kunit *test) {
	/* SMADDL is a 64-bit result instruction. sf == 0 is reserved. */
	const u32 instruction = 0x1b200000U;
	struct pt_regs regs = {};
	struct pt_regs before;
	struct orlix_tcti_result result;
	unsigned long mapped;

	mapped = orlix_tcti_integer_conditional_map(test, instruction);
	regs.regs[1] = 0x1234;
	regs.regs[2] = 0x5678;
	regs.pc = mapped;
	regs.pstate = PSR_MODE_EL0t | PSR_Z_BIT;
	regs.syscallno = NO_SYSCALL;
	before = regs;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_UNSUPPORTED_INSTRUCTION, result.reason);
	KUNIT_EXPECT_EQ(test, -EOPNOTSUPP, result.status);
	KUNIT_EXPECT_EQ(test, instruction, result.instruction);
	KUNIT_EXPECT_MEMEQ(test, before.regs, regs.regs, sizeof(regs.regs));
	KUNIT_EXPECT_EQ(test, before.pc, regs.pc);
	KUNIT_EXPECT_EQ(test, before.pstate, regs.pstate);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
}

static struct kunit_case orlix_tcti_integer_conditional_source_bound_test_cases[] = {
    KUNIT_CASE(orlix_tcti_integer_conditional_source_bindings),
    KUNIT_CASE(orlix_tcti_integer_conditional_divide_corners_production_path),
    KUNIT_CASE(orlix_tcti_integer_conditional_all_divide_leaves_production_path),
    KUNIT_CASE(orlix_tcti_integer_conditional_compare_leaves_production_path),
    KUNIT_CASE(orlix_tcti_integer_conditional_select_leaves_production_path),
    KUNIT_CASE(orlix_tcti_integer_conditional_multiply_leaves_production_path),
    KUNIT_CASE(orlix_tcti_integer_conditional_x31_is_zero_and_not_writable),
    KUNIT_CASE(orlix_tcti_integer_conditional_reserved_encodings_fail_before_state),
    {}};

struct kunit_suite orlix_tcti_integer_conditional_source_bound_test_suite = {
    .name = "orlix-tcti-integer-conditional-source-bound",
    .test_cases = orlix_tcti_integer_conditional_source_bound_test_cases,
};

kunit_test_suite(orlix_tcti_integer_conditional_source_bound_test_suite);
