// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include <linux/bitops.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/sched.h>
#include <linux/syscalls.h>
#include <asm/processor.h>
#include <asm/ptrace.h>
#include <asm/tcti.h>

#include "../block_cache.h"
#include "../decode_aarch64.h"
#include "tcti_test_suites.h"

#define ADD_SUB_IMMEDIATE_SOURCE_MASK 0xff800000U
#define ADD_SUB_IMMEDIATE_VARIABLE_MASK (~ADD_SUB_IMMEDIATE_SOURCE_MASK)
#define ADD_SUB_IMMEDIATE_NZCV \
	(PSR_N_BIT | PSR_Z_BIT | PSR_C_BIT | PSR_V_BIT)
#define ADD_SUB_IMMEDIATE_PSTATE_SEED \
	((0x155UL & ~PSR_MODE_MASK) | ADD_SUB_IMMEDIATE_NZCV)
#define ADD_SUB_IMMEDIATE_SVC 0xd4000001U

struct tcti_add_sub_immediate_leaf {
	u16 source_ordinal;
	const char *source_name;
	const char *operation_id;
	u32 source_pattern;
	bool is_64bit;
	bool subtract;
	bool set_flags;
};

/*
 * Exact direct leaves from the pinned Arm AARCHMRS 2026-06 source manifest.
 * CMN and CMP are operand aliases of the flag-setting leaves.
 */
static const struct tcti_add_sub_immediate_leaf
tcti_add_sub_immediate_leaves[] = {
	{ 2173, "ADD_32_addsub_imm", "ADD_addsub_imm", 0x11000000U,
	  false, false, false },
	{ 2174, "ADDS_32S_addsub_imm", "ADDS_addsub_imm", 0x31000000U,
	  false, false, true },
	{ 2175, "SUB_32_addsub_imm", "SUB_addsub_imm", 0x51000000U,
	  false, true, false },
	{ 2176, "SUBS_32S_addsub_imm", "SUBS_addsub_imm", 0x71000000U,
	  false, true, true },
	{ 2177, "ADD_64_addsub_imm", "ADD_addsub_imm", 0x91000000U,
	  true, false, false },
	{ 2178, "ADDS_64S_addsub_imm", "ADDS_addsub_imm", 0xb1000000U,
	  true, false, true },
	{ 2179, "SUB_64_addsub_imm", "SUB_addsub_imm", 0xd1000000U,
	  true, true, false },
	{ 2180, "SUBS_64S_addsub_imm", "SUBS_addsub_imm", 0xf1000000U,
	  true, true, true },
};

static u32 tcti_add_sub_immediate_instruction(
	const struct tcti_add_sub_immediate_leaf *leaf, bool shift,
	u16 immediate, u8 rn, u8 rd)
{
	return leaf->source_pattern | (shift ? BIT(22) : 0) |
		((u32)immediate << 10) | ((u32)rn << 5) | rd;
}

static const struct tcti_add_sub_immediate_leaf *
tcti_add_sub_immediate_source_leaf(u32 instruction)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(tcti_add_sub_immediate_leaves);
	     index++) {
		const struct tcti_add_sub_immediate_leaf *leaf =
			&tcti_add_sub_immediate_leaves[index];

		if ((instruction & ADD_SUB_IMMEDIATE_SOURCE_MASK) ==
		    leaf->source_pattern)
			return leaf;
	}
	return NULL;
}

static void tcti_add_sub_immediate_source_bindings(struct kunit *test)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(tcti_add_sub_immediate_leaves);
	     index++) {
		const struct tcti_add_sub_immediate_leaf *leaf =
			&tcti_add_sub_immediate_leaves[index];
		struct tcti_decoded_instruction decoded =
			tcti_decode_aarch64(leaf->source_pattern);

		KUNIT_EXPECT_EQ_MSG(test, leaf->source_pattern,
				    leaf->source_pattern &
					    ADD_SUB_IMMEDIATE_SOURCE_MASK,
				    "%s source fingerprint", leaf->source_name);
		KUNIT_ASSERT_EQ_MSG(test, TCTI_DECODE_ADD_SUB_IMMEDIATE,
				    decoded.decode_class, "%s", leaf->source_name);
		KUNIT_EXPECT_EQ(test, leaf->is_64bit, decoded.is_64bit);
		KUNIT_EXPECT_EQ(test, leaf->subtract, decoded.subtract);
		KUNIT_EXPECT_EQ(test, leaf->set_flags, decoded.set_flags);
		KUNIT_EXPECT_GT(test, leaf->source_ordinal, (u16)0);
	}
}

static void tcti_add_sub_immediate_all_legal_encodings(struct kunit *test)
{
	size_t leaf_index;

	for (leaf_index = 0;
	     leaf_index < ARRAY_SIZE(tcti_add_sub_immediate_leaves);
	     leaf_index++) {
		const struct tcti_add_sub_immediate_leaf *leaf =
			&tcti_add_sub_immediate_leaves[leaf_index];
		u32 variable;

		for (variable = 0;
		     variable <= ADD_SUB_IMMEDIATE_VARIABLE_MASK; variable++) {
			u32 instruction = leaf->source_pattern | variable;
			struct tcti_decoded_instruction decoded =
				tcti_decode_aarch64(instruction);

			if (decoded.decode_class != TCTI_DECODE_ADD_SUB_IMMEDIATE ||
			    decoded.is_64bit != leaf->is_64bit ||
			    decoded.subtract != leaf->subtract ||
			    decoded.set_flags != leaf->set_flags ||
			    decoded.shift != !!(variable & BIT(22)) ||
			    decoded.imm12 != ((variable >> 10) & 0xfffU) ||
			    decoded.rn != ((variable >> 5) & 0x1fU) ||
			    decoded.rd != (variable & 0x1fU)) {
				KUNIT_FAIL(test,
					   "%s legal encoding %#x decoded incorrectly",
					   leaf->source_name, instruction);
				return;
			}
			if ((variable & 0xfffU) == 0xfffU)
				cond_resched();
		}
	}
}

static unsigned long tcti_add_sub_immediate_map_program(
	struct kunit *test, u32 instruction)
{
	const u32 program[] = { instruction, ADD_SUB_IMMEDIATE_SVC };
	unsigned long mapped;
	int ret;

	KUNIT_ASSERT_NOT_NULL(test, current->mm);
	mapped = ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
				 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(mapped));
	ret = tcti_write_user_data(current->mm, mapped, program,
				   sizeof(program));
	if (ret) {
		vm_munmap(mapped, PAGE_SIZE);
		KUNIT_FAIL(test, "could not write ADD/SUB program: %d", ret);
		return 0;
	}
	ret = sys_mprotect(mapped, PAGE_SIZE, PROT_READ | PROT_EXEC);
	if (ret) {
		vm_munmap(mapped, PAGE_SIZE);
		KUNIT_FAIL(test, "could not protect ADD/SUB program: %d", ret);
		return 0;
	}
	return mapped;
}

static unsigned long tcti_add_sub_immediate_expected_pstate(
	const struct tcti_add_sub_immediate_leaf *leaf, u64 left, u64 right,
	u64 result, unsigned long initial)
{
	u64 mask = leaf->is_64bit ? U64_MAX : U32_MAX;
	u64 sign = leaf->is_64bit ? BIT_ULL(63) : BIT_ULL(31);
	unsigned long flags = 0;

	if (!leaf->set_flags)
		return initial;
	left &= mask;
	right &= mask;
	result &= mask;
	if (result & sign)
		flags |= PSR_N_BIT;
	if (!result)
		flags |= PSR_Z_BIT;
	if (leaf->subtract ? left >= right :
	    ((__uint128_t)left + right) > mask)
		flags |= PSR_C_BIT;
	if (leaf->subtract ?
	    (((left ^ right) & (left ^ result) & sign) != 0) :
	    ((~(left ^ right) & (left ^ result) & sign) != 0))
		flags |= PSR_V_BIT;
	return (initial & ~ADD_SUB_IMMEDIATE_NZCV) | flags;
}

static void tcti_add_sub_immediate_run_with_expected_nzcv(
	struct kunit *test, const struct tcti_add_sub_immediate_leaf *leaf,
	bool shift, u16 immediate, u8 rn, u8 rd, u64 source, u64 sp,
	unsigned long pstate, bool has_expected_nzcv,
	unsigned long expected_nzcv)
{
	u32 instruction = tcti_add_sub_immediate_instruction(
		leaf, shift, immediate, rn, rd);
	unsigned long mapped = tcti_add_sub_immediate_map_program(
		test, instruction);
	u64 mask = leaf->is_64bit ? U64_MAX : U32_MAX;
	u64 right = (u64)immediate << (shift ? 12 : 0);
	unsigned int pass;

	KUNIT_ASSERT_NE(test, 0UL, mapped);
	for (pass = 0; pass < 2; pass++) {
		struct tcti_block *block;
		struct pt_regs regs = {};
		struct pt_regs before;
		struct tcti_result result;
		u64 left;
		u64 expected;
		unsigned long expected_pstate;
		unsigned int reg;

		for (reg = 0; reg < 31; reg++)
			regs.regs[reg] = 0x8100000000000000ULL + reg;
		if (rn < 31)
		regs.regs[rn] = source;
		regs.sp = sp;
		regs.pc = mapped;
		regs.pstate = PSR_MODE_EL0t | (pstate & ~PSR_MODE_MASK);
		regs.syscallno = NO_SYSCALL;
		KUNIT_ASSERT_EQ(test, (unsigned long)PSR_MODE_EL0t,
				regs.pstate & PSR_MODE_MASK);
		before = regs;
		left = (rn == 31 ? before.sp : before.regs[rn]) & mask;
		expected = (leaf->subtract ? left - right : left + right) & mask;
		expected_pstate = tcti_add_sub_immediate_expected_pstate(
			leaf, left, right, expected, before.pstate);
		if (has_expected_nzcv) {
			KUNIT_EXPECT_EQ(test, expected_nzcv,
					expected_pstate &
						ADD_SUB_IMMEDIATE_NZCV);
			expected_pstate =
				(before.pstate & ~ADD_SUB_IMMEDIATE_NZCV) |
				expected_nzcv;
		}

		result = tcti_resume_user(current, &regs, current->mm);
		KUNIT_ASSERT_EQ(test, TCTI_EXIT_SYSCALL, result.reason);
		KUNIT_EXPECT_EQ(test, 0L, result.status);
		KUNIT_EXPECT_EQ(test, mapped + sizeof(u32), result.pc);
		KUNIT_EXPECT_EQ(test, ADD_SUB_IMMEDIATE_SVC,
				result.instruction);
		if (!pass) {
			block = tcti_block_cache_lookup(
				current->mm, mapped,
				tcti_code_generation(current->mm));
			KUNIT_EXPECT_NOT_NULL(test, block);
			if (block)
				tcti_block_put(block);
		}
		for (reg = 0; reg < 31; reg++) {
			u64 expected_reg = before.regs[reg];

			if (rd == reg)
				expected_reg = expected;
			KUNIT_EXPECT_EQ_MSG(test, expected_reg, regs.regs[reg],
					    "%s x%u pass %u", leaf->source_name,
					    reg, pass);
		}
		KUNIT_EXPECT_EQ(test,
			(!leaf->set_flags && rd == 31) ? expected : before.sp,
			regs.sp);
		KUNIT_EXPECT_EQ(test, expected_pstate, regs.pstate);
		KUNIT_EXPECT_EQ(test, (unsigned long)PSR_MODE_EL0t,
				regs.pstate & PSR_MODE_MASK);
		KUNIT_EXPECT_EQ(test, mapped + sizeof(u32), regs.pc);
	}
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
}

static void tcti_add_sub_immediate_run(
	struct kunit *test, const struct tcti_add_sub_immediate_leaf *leaf,
	bool shift, u16 immediate, u8 rn, u8 rd, u64 source, u64 sp,
	unsigned long pstate)
{
	tcti_add_sub_immediate_run_with_expected_nzcv(
		test, leaf, shift, immediate, rn, rd, source, sp, pstate, false,
		0);
}

static void tcti_add_sub_immediate_production_path_arithmetic(
	struct kunit *test)
{
	static const struct {
		u64 source;
		u16 immediate;
		bool shift;
	} vectors[] = {
		{ 0, 0, false },
		{ 0, 1, false },
		{ U32_MAX, 1, false },
		{ BIT_ULL(31) - 1, 1, false },
		{ BIT_ULL(31), 1, false },
		{ U64_MAX, 1, false },
		{ BIT_ULL(63) - 1, 1, false },
		{ BIT_ULL(63), 1, false },
		{ 0x123456789abcdef0ULL, 0xfff, true },
	};
	size_t leaf_index;
	size_t vector_index;

	for (leaf_index = 0;
	     leaf_index < ARRAY_SIZE(tcti_add_sub_immediate_leaves);
	     leaf_index++)
		for (vector_index = 0; vector_index < ARRAY_SIZE(vectors);
		     vector_index++)
			tcti_add_sub_immediate_run(
				test, &tcti_add_sub_immediate_leaves[leaf_index],
				vectors[vector_index].shift,
				vectors[vector_index].immediate, 4, 7,
				vectors[vector_index].source,
				0x000000fffffff000ULL,
				ADD_SUB_IMMEDIATE_PSTATE_SEED);
}

static void tcti_add_sub_immediate_special_register_aliases(
	struct kunit *test)
{
	size_t leaf_index;

	for (leaf_index = 0;
	     leaf_index < ARRAY_SIZE(tcti_add_sub_immediate_leaves);
	     leaf_index++) {
		const struct tcti_add_sub_immediate_leaf *leaf =
			&tcti_add_sub_immediate_leaves[leaf_index];

		tcti_add_sub_immediate_run(test, leaf, false, 7, 31, 5,
					   0x1111111111111111ULL,
					   0x00000001fffffff0ULL,
					   ADD_SUB_IMMEDIATE_PSTATE_SEED);
		tcti_add_sub_immediate_run(test, leaf, false, 9, 31, 31,
					   0x2222222222222222ULL,
					   0x00000001fffffff0ULL,
					   ADD_SUB_IMMEDIATE_PSTATE_SEED);
		tcti_add_sub_immediate_run(test, leaf, true, 1, 6, 6,
					   0x8000000080001000ULL,
					   0x00000001fffffff0ULL,
					   ADD_SUB_IMMEDIATE_PSTATE_SEED);
	}
}

static void tcti_add_sub_immediate_cmn_cmp_nzcv_boundaries(
	struct kunit *test)
{
	static const struct {
		bool is_64bit;
		u64 source;
		u16 immediate;
		bool subtract;
		unsigned long expected_nzcv;
	} vectors[] = {
		/* CMN carry and signed-overflow boundaries. */
		{ false, U32_MAX, 1, false, PSR_Z_BIT | PSR_C_BIT },
		{ false, BIT_ULL(31) - 1, 1, false,
		  PSR_N_BIT | PSR_V_BIT },
		{ true, U64_MAX, 1, false, PSR_Z_BIT | PSR_C_BIT },
		{ true, BIT_ULL(63) - 1, 1, false,
		  PSR_N_BIT | PSR_V_BIT },
		/* CMP borrow and signed-overflow boundaries. */
		{ false, 0, 1, true, PSR_N_BIT },
		{ false, BIT_ULL(31), 1, true,
		  PSR_C_BIT | PSR_V_BIT },
		{ true, 0, 1, true, PSR_N_BIT },
		{ true, BIT_ULL(63), 1, true,
		  PSR_C_BIT | PSR_V_BIT },
	};
	static const size_t leaf_indices[] = { 1, 3, 5, 7 };
	size_t leaf_index;
	size_t vector_index;

	for (leaf_index = 0; leaf_index < ARRAY_SIZE(leaf_indices);
	     leaf_index++) {
		const struct tcti_add_sub_immediate_leaf *leaf =
			&tcti_add_sub_immediate_leaves[leaf_indices[leaf_index]];

		for (vector_index = 0; vector_index < ARRAY_SIZE(vectors);
		     vector_index++) {
			if (leaf->is_64bit != vectors[vector_index].is_64bit ||
			    leaf->subtract != vectors[vector_index].subtract)
				continue;
			/* CMN/CMP discard the result through Rd == 31. */
			tcti_add_sub_immediate_run_with_expected_nzcv(
				test, leaf, false,
				vectors[vector_index].immediate, 31, 31, 0,
				vectors[vector_index].source,
				ADD_SUB_IMMEDIATE_PSTATE_SEED, true,
				vectors[vector_index].expected_nzcv);
		}
	}
}

static void tcti_add_sub_immediate_source_mask_boundaries(
	struct kunit *test)
{
	static const u32 cssc_patterns[] = {
		0x11c00000U, 0x11c40000U, 0x11c80000U, 0x11cc0000U,
		0x91c00000U, 0x91c40000U, 0x91c80000U, 0x91cc0000U,
	};
	static const u32 mte_tagged_patterns[] = {
		0x91800000U, 0xd1800000U,
	};
	size_t leaf_index;
	size_t index;

	for (leaf_index = 0;
	     leaf_index < ARRAY_SIZE(tcti_add_sub_immediate_leaves);
	     leaf_index++) {
		const struct tcti_add_sub_immediate_leaf *leaf =
			&tcti_add_sub_immediate_leaves[leaf_index];
		u32 instruction = tcti_add_sub_immediate_instruction(
			leaf, false, 0x555, 7, 3);
		u8 bit;

		for (bit = 0; bit < 32; bit++) {
			const struct tcti_add_sub_immediate_leaf *neighbour;
			struct tcti_decoded_instruction decoded;
			u32 mutated;

			if (!(ADD_SUB_IMMEDIATE_SOURCE_MASK & BIT(bit)))
				continue;
			mutated = instruction ^ BIT(bit);
			neighbour = tcti_add_sub_immediate_source_leaf(mutated);
			decoded = tcti_decode_aarch64(mutated);
			if (!neighbour) {
				KUNIT_EXPECT_NE_MSG(test,
					TCTI_DECODE_ADD_SUB_IMMEDIATE,
					decoded.decode_class,
					"%s fixed-bit neighbour %#x rebound",
					leaf->source_name, mutated);
				continue;
			}
			KUNIT_EXPECT_PTR_NE(test, leaf, neighbour);
			KUNIT_EXPECT_EQ_MSG(test, TCTI_DECODE_ADD_SUB_IMMEDIATE,
				decoded.decode_class, "%s neighbour %#x",
				neighbour->source_name, mutated);
			KUNIT_EXPECT_EQ(test, neighbour->is_64bit,
				decoded.is_64bit);
			KUNIT_EXPECT_EQ(test, neighbour->subtract,
				decoded.subtract);
			KUNIT_EXPECT_EQ(test, neighbour->set_flags,
				decoded.set_flags);
		}
	}

	for (index = 0; index < ARRAY_SIZE(cssc_patterns); index++)
		KUNIT_EXPECT_EQ(test, TCTI_DECODE_MIN_MAX_IMMEDIATE,
			tcti_decode_aarch64(cssc_patterns[index]).decode_class);
	for (index = 0; index < ARRAY_SIZE(mte_tagged_patterns); index++)
		KUNIT_EXPECT_NE(test, TCTI_DECODE_ADD_SUB_IMMEDIATE,
			tcti_decode_aarch64(mte_tagged_patterns[index]).decode_class);
}

static struct kunit_case tcti_add_sub_immediate_test_cases[] = {
	KUNIT_CASE(tcti_add_sub_immediate_source_bindings),
	KUNIT_CASE(tcti_add_sub_immediate_all_legal_encodings),
	KUNIT_CASE(tcti_add_sub_immediate_production_path_arithmetic),
	KUNIT_CASE(tcti_add_sub_immediate_special_register_aliases),
	KUNIT_CASE(tcti_add_sub_immediate_cmn_cmp_nzcv_boundaries),
	KUNIT_CASE(tcti_add_sub_immediate_source_mask_boundaries),
	{}
};

struct kunit_suite tcti_add_sub_immediate_test_suite = {
	.name = "orlix-tcti-add-sub-immediate",
	.test_cases = tcti_add_sub_immediate_test_cases,
};

kunit_test_suite(tcti_add_sub_immediate_test_suite);
