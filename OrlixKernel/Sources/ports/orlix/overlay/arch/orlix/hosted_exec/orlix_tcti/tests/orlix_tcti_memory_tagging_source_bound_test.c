// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/sched.h>
#include <linux/syscalls.h>
#include <asm/orlix_tcti.h>
#include <asm/processor.h>
#include <asm/ptrace.h>

#include "../decode_aarch64.h"
#include "../semantics.h"
#include "../switch_debug.h"
#include "orlix_tcti_test_suites.h"

#define MTE_TAG_SHIFT 56U
#define MTE_ADDRESS_MASK GENMASK_ULL(55, 0)

struct mte_leaf {
	u32 ordinal;
	const char *name;
	u32 mask;
	u32 pattern;
	enum orlix_tcti_memory_tagging_op op;
	enum orlix_tcti_memory_index_mode mode;
	bool immediate;
	bool non_el0;
};

/* Exact issue #135 execution-slice rows and pinned AARCHMRS 2026-06 encodings. */
static const struct mte_leaf mte_leaves[] = {
	{ 2181, "ADDG_64_addsub_immtags", 0xffc0c000, 0x91800000, ORLIX_TCTI_MTE_ADDG },
	{ 2182, "SUBG_64_addsub_immtags", 0xffc0c000, 0xd1800000, ORLIX_TCTI_MTE_SUBG },
	{ 2567, "STG_64Spost_ldsttags", 0xffe00c00, 0xd9200400, ORLIX_TCTI_MTE_STG, ORLIX_TCTI_MEMORY_INDEX_POST, true },
	{ 2568, "STG_64Soffset_ldsttags", 0xffe00c00, 0xd9200800, ORLIX_TCTI_MTE_STG, ORLIX_TCTI_MEMORY_INDEX_SIGNED_OFFSET, true },
	{ 2569, "STG_64Spre_ldsttags", 0xffe00c00, 0xd9200c00, ORLIX_TCTI_MTE_STG, ORLIX_TCTI_MEMORY_INDEX_PRE, true },
	{ 2570, "STZGM_64bulk_ldsttags", 0xfffffc00, 0xd9200000, ORLIX_TCTI_MTE_STZGM, 0, false, true },
	{ 2571, "LDG_64Loffset_ldsttags", 0xffe00c00, 0xd9600000, ORLIX_TCTI_MTE_LDG, ORLIX_TCTI_MEMORY_INDEX_SIGNED_OFFSET, true },
	{ 2572, "STZG_64Spost_ldsttags", 0xffe00c00, 0xd9600400, ORLIX_TCTI_MTE_STZG, ORLIX_TCTI_MEMORY_INDEX_POST, true },
	{ 2573, "STZG_64Soffset_ldsttags", 0xffe00c00, 0xd9600800, ORLIX_TCTI_MTE_STZG, ORLIX_TCTI_MEMORY_INDEX_SIGNED_OFFSET, true },
	{ 2574, "STZG_64Spre_ldsttags", 0xffe00c00, 0xd9600c00, ORLIX_TCTI_MTE_STZG, ORLIX_TCTI_MEMORY_INDEX_PRE, true },
	{ 2575, "ST2G_64Spost_ldsttags", 0xffe00c00, 0xd9a00400, ORLIX_TCTI_MTE_ST2G, ORLIX_TCTI_MEMORY_INDEX_POST, true },
	{ 2576, "ST2G_64Soffset_ldsttags", 0xffe00c00, 0xd9a00800, ORLIX_TCTI_MTE_ST2G, ORLIX_TCTI_MEMORY_INDEX_SIGNED_OFFSET, true },
	{ 2577, "ST2G_64Spre_ldsttags", 0xffe00c00, 0xd9a00c00, ORLIX_TCTI_MTE_ST2G, ORLIX_TCTI_MEMORY_INDEX_PRE, true },
	{ 2578, "STGM_64bulk_ldsttags", 0xfffffc00, 0xd9a00000, ORLIX_TCTI_MTE_STGM, 0, false, true },
	{ 2579, "STZ2G_64Spost_ldsttags", 0xffe00c00, 0xd9e00400, ORLIX_TCTI_MTE_STZ2G, ORLIX_TCTI_MEMORY_INDEX_POST, true },
	{ 2580, "STZ2G_64Soffset_ldsttags", 0xffe00c00, 0xd9e00800, ORLIX_TCTI_MTE_STZ2G, ORLIX_TCTI_MEMORY_INDEX_SIGNED_OFFSET, true },
	{ 2581, "STZ2G_64Spre_ldsttags", 0xffe00c00, 0xd9e00c00, ORLIX_TCTI_MTE_STZ2G, ORLIX_TCTI_MEMORY_INDEX_PRE, true },
	{ 2582, "LDGM_64bulk_ldsttags", 0xfffffc00, 0xd9e00000, ORLIX_TCTI_MTE_LDGM, 0, false, true },
	{ 3375, "IRG_64I_dp_2src", 0xffe0fc00, 0x9ac01000, ORLIX_TCTI_MTE_IRG },
	{ 3376, "GMI_64G_dp_2src", 0xffe0fc00, 0x9ac01400, ORLIX_TCTI_MTE_GMI },
};

static u32 mte_instruction(const struct mte_leaf *leaf, u16 immediate,
			   u8 rm_or_rt, u8 rn, u8 rd)
{
	if (leaf->op == ORLIX_TCTI_MTE_ADDG || leaf->op == ORLIX_TCTI_MTE_SUBG)
		return leaf->pattern | ((u32)(immediate & 0x3f) << 16) |
			((u32)((immediate >> 6) & 0xf) << 10) | ((u32)rn << 5) | rd;
	if (leaf->op == ORLIX_TCTI_MTE_IRG || leaf->op == ORLIX_TCTI_MTE_GMI)
		return leaf->pattern | ((u32)rm_or_rt << 16) | ((u32)rn << 5) | rd;
	return leaf->pattern | (leaf->immediate ? (u32)(immediate & 0x1ff) << 12 : 0) |
		((u32)rn << 5) | rm_or_rt;
}

static bool mte_decoded_matches(const struct mte_leaf *leaf, u32 instruction)
{
	struct orlix_tcti_decoded_instruction d = orlix_tcti_decode_aarch64(instruction);

	if (d.decode_class != ORLIX_TCTI_DECODE_MEMORY_TAGGING ||
	    d.memory_tagging_op != leaf->op)
		return false;
	if (leaf->op >= ORLIX_TCTI_MTE_STG && leaf->op <= ORLIX_TCTI_MTE_LDGM &&
	    !leaf->non_el0)
		return d.memory_index_mode == leaf->mode;
	return true;
}

static void mte_source_cohort_is_exact(struct kunit *test)
{
	size_t i;

	KUNIT_EXPECT_EQ(test, (size_t)20, ARRAY_SIZE(mte_leaves));
	for (i = 0; i < ARRAY_SIZE(mte_leaves); i++) {
		KUNIT_EXPECT_TRUE_MSG(test,
			mte_decoded_matches(&mte_leaves[i], mte_leaves[i].pattern),
			"ordinal %u %s", mte_leaves[i].ordinal, mte_leaves[i].name);
		if (i)
			KUNIT_EXPECT_LT(test, mte_leaves[i - 1].ordinal, mte_leaves[i].ordinal);
	}
}

static void mte_all_legal_free_fields_decode(struct kunit *test)
{
	size_t i;
	u16 value;
	u8 reg;

	for (i = 0; i < ARRAY_SIZE(mte_leaves); i++) {
		const struct mte_leaf *leaf = &mte_leaves[i];
		u16 limit = leaf->immediate ? 512 :
			(leaf->op == ORLIX_TCTI_MTE_ADDG || leaf->op == ORLIX_TCTI_MTE_SUBG) ?
			1024 : 1;

		for (value = 0; value < limit; value++)
			KUNIT_ASSERT_TRUE_MSG(test,
				mte_decoded_matches(leaf, mte_instruction(leaf, value, 3, 2, 1)),
				"%s immediate %#x", leaf->name, value);
		for (reg = 0; reg < 32; reg++) {
			KUNIT_ASSERT_TRUE_MSG(test,
				mte_decoded_matches(leaf, mte_instruction(leaf, 0x155, reg, 2, 1)),
				"%s Rm/Rt %u", leaf->name, reg);
			KUNIT_ASSERT_TRUE_MSG(test,
				mte_decoded_matches(leaf, mte_instruction(leaf, 0x155, 3, reg, 1)),
				"%s Rn %u", leaf->name, reg);
			if (leaf->op == ORLIX_TCTI_MTE_ADDG || leaf->op == ORLIX_TCTI_MTE_SUBG ||
			    leaf->op == ORLIX_TCTI_MTE_IRG || leaf->op == ORLIX_TCTI_MTE_GMI)
				KUNIT_ASSERT_TRUE_MSG(test,
					mte_decoded_matches(leaf, mte_instruction(leaf, 0x155, 3, 2, reg)),
					"%s Rd %u", leaf->name, reg);
		}
	}
}

static void mte_fixed_bit_neighbours_are_not_the_leaf(struct kunit *test)
{
	size_t i;
	u8 bit;

	for (i = 0; i < ARRAY_SIZE(mte_leaves); i++)
		for (bit = 0; bit < 32; bit++)
			if (mte_leaves[i].mask & BIT(bit))
				KUNIT_EXPECT_FALSE_MSG(test,
					mte_decoded_matches(&mte_leaves[i],
						mte_leaves[i].pattern ^ BIT(bit)),
					"%s fixed bit %u", mte_leaves[i].name, bit);
}

static void mte_reserved_bulk_immediates_are_undefined(struct kunit *test)
{
	static const u32 reserved_bases[] = {
		0xd9200000U, /* STZGM occupies only the imm9 == 0 bulk encoding. */
		0xd9a00000U, /* STGM occupies only the imm9 == 0 bulk encoding. */
		0xd9e00000U, /* LDGM occupies only the imm9 == 0 bulk encoding. */
	};
	size_t base;
	u16 imm9;

	for (base = 0; base < ARRAY_SIZE(reserved_bases); base++)
		for (imm9 = 1; imm9 < 512; imm9++)
			KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_DECODE_UNSUPPORTED,
				orlix_tcti_decode_aarch64(reserved_bases[base] |
							  ((u32)imm9 << 12)).decode_class,
				"base %#x imm9 %#x", reserved_bases[base], imm9);
}

static void mte_register_semantics_and_non_el0(struct kunit *test)
{
	struct pt_regs regs = {};
	struct orlix_tcti_decoded_instruction d;
	unsigned long fault = 0;
	u32 instruction;
	int ret;

	current->thread.user_mte_exclude_mask = 0xfffb;
	regs.pc = 0x1000;
	regs.pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_V_BIT;
	regs.regs[2] = 0xa012345678900000ULL;
	instruction = mte_instruction(&mte_leaves[0], (2U << 6) | 3U, 0, 2, 1);
	d = orlix_tcti_decode_aarch64(instruction);
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_execute_decoded_semantics(current->mm,
								  &regs, &d, &fault));
	KUNIT_EXPECT_EQ(test, 0x2012345678900030ULL, regs.regs[1]);
	KUNIT_EXPECT_EQ(test, 0x1004UL, regs.pc);
	KUNIT_EXPECT_EQ(test, (u64)(PSR_MODE_EL0t | PSR_N_BIT | PSR_V_BIT),
			regs.pstate);

	regs.pc = 0x2000;
	current->thread.user_mte_exclude_mask = 0;
	regs.regs[2] = 0xb000000000001234ULL;
	regs.regs[3] = 0xffffU & ~BIT(7);
	d = orlix_tcti_decode_aarch64(mte_instruction(&mte_leaves[18], 0, 3, 2, 1));
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_execute_decoded_semantics(current->mm,
								  &regs, &d, &fault));
	KUNIT_EXPECT_EQ(test, 0x7000000000001234ULL, regs.regs[1]);

	regs.pc = 0x3000;
	regs.regs[2] = 0xc000000000001234ULL;
	regs.regs[3] = BIT_ULL(2);
	d = orlix_tcti_decode_aarch64(mte_instruction(&mte_leaves[19], 0, 3, 2, 1));
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_execute_decoded_semantics(current->mm,
								  &regs, &d, &fault));
	KUNIT_EXPECT_EQ(test, BIT_ULL(12) | BIT_ULL(2), regs.regs[1]);

	regs.pc = 0x4000;
	regs.regs[1] = 0x55;
	d = orlix_tcti_decode_aarch64(mte_instruction(&mte_leaves[5], 0, 1, 2, 0));
	ret = orlix_tcti_execute_decoded_semantics(current->mm, &regs, &d, &fault);
	KUNIT_EXPECT_EQ(test, -EOPNOTSUPP, ret);
	KUNIT_EXPECT_EQ(test, 0x4000UL, regs.pc);
	KUNIT_EXPECT_EQ(test, 0x55ULL, regs.regs[1]);
	current->thread.user_mte_exclude_mask = 0;
}

static unsigned long mte_map_program(struct kunit *test, const u32 *instructions,
				    size_t instruction_count)
{
	unsigned long mapped;
	int ret;

	mapped = ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
				 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(mapped));
	ret = orlix_tcti_write_user_data(current->mm, mapped, instructions,
					 instruction_count * sizeof(*instructions));
	if (ret) {
		vm_munmap(mapped, PAGE_SIZE);
		KUNIT_FAIL(test, "could not write memory-tagging program: %d", ret);
		return 0;
	}
	ret = sys_mprotect(mapped, PAGE_SIZE, PROT_READ | PROT_EXEC);
	if (ret) {
		vm_munmap(mapped, PAGE_SIZE);
		KUNIT_FAIL(test, "could not protect memory-tagging program: %d", ret);
		return 0;
	}
	return mapped;
}

static void mte_production_resume_all_el0_leaves(struct kunit *test)
{
	static const u32 svc = 0xd4000001U;
	unsigned long data;
	size_t index;

	data = ksys_mmap_pgoff(0, 2 * PAGE_SIZE, PROT_READ | PROT_WRITE,
				MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(data));
	for (index = 0; index < ARRAY_SIZE(mte_leaves); index++) {
		const struct mte_leaf *leaf = &mte_leaves[index];
		struct orlix_tcti_result result;
		struct pt_regs regs = {};
		unsigned long base = data + 0x100;
		unsigned long effective = base;
		unsigned long text;
		u64 expected_base = base;
		u64 pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_C_BIT | PSR_V_BIT;
		u16 immediate = 0;
		u8 bytes[32];
		u8 tag;
		u32 program[2];

		if (leaf->non_el0)
			continue;
		if (leaf->immediate) {
			if (leaf->mode == ORLIX_TCTI_MEMORY_INDEX_SIGNED_OFFSET) {
				immediate = 0x1ff;
				effective -= 16;
			} else if (leaf->mode == ORLIX_TCTI_MEMORY_INDEX_PRE) {
				immediate = 2;
				effective += 32;
				expected_base += 32;
			} else {
				immediate = 2;
				expected_base += 32;
			}
		}
		if (leaf->op == ORLIX_TCTI_MTE_ADDG ||
		    leaf->op == ORLIX_TCTI_MTE_SUBG)
			immediate = (2U << 6) | 3U;

		program[0] = mte_instruction(leaf, immediate, 3, 2, 1);
		program[1] = svc;
		text = mte_map_program(test, program, ARRAY_SIZE(program));
		KUNIT_ASSERT_NE(test, 0UL, text);
		regs.pc = text;
		regs.sp = STACK_TOP - 16;
		regs.pstate = pstate;
		regs.regs[1] = 0x123456789abcdef0ULL;
		regs.regs[2] = base;
		regs.regs[3] = (u64)6 << MTE_TAG_SHIFT;

		if (leaf->op == ORLIX_TCTI_MTE_ADDG ||
		    leaf->op == ORLIX_TCTI_MTE_SUBG) {
			regs.regs[2] = 0x0a12345678900000ULL;
			current->thread.user_mte_exclude_mask = 0xfffb;
		} else if (leaf->op == ORLIX_TCTI_MTE_IRG) {
			regs.regs[2] = 0x0a00000000001234ULL;
			regs.regs[3] = 0xffffU & ~BIT(7);
		} else if (leaf->op == ORLIX_TCTI_MTE_GMI) {
			regs.regs[2] = 0x0c00000000001234ULL;
			regs.regs[3] = BIT_ULL(2);
		} else {
			memset(bytes, 0xa5, sizeof(bytes));
			KUNIT_ASSERT_EQ(test, 0, orlix_tcti_write_user_data(current->mm,
							 effective, bytes, sizeof(bytes)));
			KUNIT_ASSERT_EQ(test, 0, orlix_tcti_mte_store_allocation_tag(
				current->mm, effective, leaf->op == ORLIX_TCTI_MTE_LDG ? 9 : 0));
			KUNIT_ASSERT_EQ(test, 0, orlix_tcti_mte_store_allocation_tag(
				current->mm, effective + 16, 0));
		}

		result = orlix_tcti_resume_user(current, &regs, current->mm);
		KUNIT_ASSERT_EQ_MSG(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason,
				    "%s ordinal %u", leaf->name, leaf->ordinal);
		KUNIT_EXPECT_EQ_MSG(test, 0L, result.status, "%s", leaf->name);
		KUNIT_EXPECT_EQ_MSG(test, svc, result.instruction, "%s", leaf->name);
		KUNIT_EXPECT_EQ_MSG(test, text + sizeof(u32), regs.pc, "%s", leaf->name);
		KUNIT_EXPECT_EQ_MSG(test, pstate, regs.pstate, "%s", leaf->name);

		switch (leaf->op) {
		case ORLIX_TCTI_MTE_ADDG:
			KUNIT_EXPECT_EQ(test, 0x0c12345678900030ULL, regs.regs[1]);
			break;
		case ORLIX_TCTI_MTE_SUBG:
			KUNIT_EXPECT_EQ(test, 0x08123456788fffd0ULL, regs.regs[1]);
			break;
		case ORLIX_TCTI_MTE_IRG:
			KUNIT_EXPECT_EQ(test, 0x0700000000001234ULL, regs.regs[1]);
			break;
		case ORLIX_TCTI_MTE_GMI:
			KUNIT_EXPECT_EQ(test, BIT_ULL(12) | BIT_ULL(2), regs.regs[1]);
			break;
		case ORLIX_TCTI_MTE_LDG:
			KUNIT_EXPECT_EQ(test, 0x193456789abcdef0ULL, regs.regs[1]);
			KUNIT_EXPECT_EQ(test, (u64)base, regs.regs[2]);
			break;
		default:
			KUNIT_EXPECT_EQ(test, expected_base, regs.regs[2]);
			KUNIT_ASSERT_EQ(test, 0, orlix_tcti_mte_load_allocation_tag(
				current->mm, effective, &tag));
			KUNIT_EXPECT_EQ(test, (u8)6, tag);
			if (leaf->op == ORLIX_TCTI_MTE_ST2G ||
			    leaf->op == ORLIX_TCTI_MTE_STZ2G) {
				KUNIT_ASSERT_EQ(test, 0, orlix_tcti_mte_load_allocation_tag(
					current->mm, effective + 16, &tag));
				KUNIT_EXPECT_EQ(test, (u8)6, tag);
			}
			KUNIT_ASSERT_EQ(test, 0, orlix_tcti_read_user_data(current->mm,
								effective, bytes, sizeof(bytes)));
			if (leaf->op == ORLIX_TCTI_MTE_STZG)
				KUNIT_EXPECT_MEMEQ(test, bytes, (u8[16]){}, 16);
			else if (leaf->op == ORLIX_TCTI_MTE_STZ2G)
				KUNIT_EXPECT_MEMEQ(test, bytes, (u8[32]){}, 32);
			else
				KUNIT_EXPECT_MEMEQ(test, bytes,
					((u8[32]){ [0 ... 31] = 0xa5 }), 32);
			break;
		}
		current->thread.user_mte_exclude_mask = 0;
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(text, PAGE_SIZE));
	}
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(data, 2 * PAGE_SIZE));
}

static void mte_production_resume_register_memory_and_faults(struct kunit *test)
{
	static const u32 svc = 0xd4000001U;
	struct orlix_tcti_result result;
	struct pt_regs regs = {};
	unsigned long text;
	unsigned long data;
	u64 pstate;
	u8 bytes[32];
	u8 tag;
	u32 program[3];

	program[0] = mte_instruction(&mte_leaves[0], (2U << 6) | 3U, 0, 2, 1);
	program[1] = svc;
	text = mte_map_program(test, program, 2);
	KUNIT_ASSERT_NE(test, 0UL, text);
	regs.pc = text;
	regs.sp = STACK_TOP - 16;
	regs.pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_C_BIT;
	pstate = regs.pstate;
	regs.regs[2] = 0x0a12345678900000ULL;
	current->thread.user_mte_exclude_mask = 0xfffb;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_EXPECT_EQ(test, 0L, result.status);
	KUNIT_EXPECT_EQ(test, svc, result.instruction);
	KUNIT_EXPECT_EQ(test, 0x0c12345678900030ULL, regs.regs[1]);
	KUNIT_EXPECT_EQ(test, text + sizeof(u32), regs.pc);
	KUNIT_EXPECT_EQ(test, pstate, regs.pstate);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(text, PAGE_SIZE));
	current->thread.user_mte_exclude_mask = 0;

	program[0] = mte_instruction(&mte_leaves[0], 0, 0, 31, 1);
	text = mte_map_program(test, program, 1);
	KUNIT_ASSERT_NE(test, 0UL, text);
	memset(&regs, 0, sizeof(regs));
	regs.pc = text;
	regs.sp = (STACK_TOP - 16) | 1;
	regs.pstate = PSR_MODE_EL0t;
	regs.regs[1] = 0x123456789abcdef0ULL;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_ALIGNMENT_FAULT, result.reason);
	KUNIT_EXPECT_EQ(test, -EFAULT, result.status);
	KUNIT_EXPECT_EQ(test, regs.sp & MTE_ADDRESS_MASK, result.fault_address);
	KUNIT_EXPECT_EQ(test, text, regs.pc);
	KUNIT_EXPECT_EQ(test, 0x123456789abcdef0ULL, regs.regs[1]);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(text, PAGE_SIZE));

	data = ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
				MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(data));
	memset(bytes, 0xa5, sizeof(bytes));
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_write_user_data(current->mm, data,
							     bytes, sizeof(bytes)));
	program[0] = mte_instruction(&mte_leaves[8], 0, 3, 2, 0);
	program[1] = mte_instruction(&mte_leaves[6], 0, 1, 2, 1);
	program[2] = svc;
	text = mte_map_program(test, program, ARRAY_SIZE(program));
	KUNIT_ASSERT_NE(test, 0UL, text);
	memset(&regs, 0, sizeof(regs));
	regs.pc = text;
	regs.sp = STACK_TOP - 16;
	regs.pstate = PSR_MODE_EL0t | PSR_Z_BIT | PSR_V_BIT;
	pstate = regs.pstate;
	regs.regs[1] = 0x123456789abcdef0ULL;
	regs.regs[2] = data;
	regs.regs[3] = (u64)6 << MTE_TAG_SHIFT;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_EXPECT_EQ(test, 0L, result.status);
	KUNIT_EXPECT_EQ(test, text + 2 * sizeof(u32), regs.pc);
	KUNIT_EXPECT_EQ(test, pstate, regs.pstate);
	KUNIT_EXPECT_EQ(test, 0x163456789abcdef0ULL, regs.regs[1]);
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_read_user_data(current->mm, data,
							    bytes, sizeof(bytes)));
	KUNIT_EXPECT_MEMEQ(test, bytes, (u8[16]){}, 16);
	KUNIT_EXPECT_MEMEQ(test, bytes + 16, ((u8[16]){ [0 ... 15] = 0xa5 }), 16);
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_mte_load_allocation_tag(current->mm,
							     data, &tag));
	KUNIT_EXPECT_EQ(test, (u8)6, tag);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(text, PAGE_SIZE));

	program[0] = mte_instruction(&mte_leaves[8], 0, 3, 31, 0);
	text = mte_map_program(test, program, 1);
	KUNIT_ASSERT_NE(test, 0UL, text);
	memset(&regs, 0, sizeof(regs));
	regs.pc = text;
	regs.sp = data + 1;
	regs.pstate = PSR_MODE_EL0t;
	regs.regs[3] = (u64)4 << MTE_TAG_SHIFT;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_ALIGNMENT_FAULT, result.reason);
	KUNIT_EXPECT_EQ(test, -EFAULT, result.status);
	KUNIT_EXPECT_EQ(test, data + 1, result.fault_address);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_ACCESS_WRITE, result.fault_access);
	KUNIT_EXPECT_EQ(test, text, regs.pc);
	KUNIT_EXPECT_EQ(test, data + 1, regs.sp);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(text, PAGE_SIZE));

	program[0] = mte_instruction(&mte_leaves[6], 0, 1, 2, 0);
	text = mte_map_program(test, program, 1);
	KUNIT_ASSERT_NE(test, 0UL, text);
	memset(&regs, 0, sizeof(regs));
	regs.pc = text;
	regs.sp = STACK_TOP - 16;
	regs.pstate = PSR_MODE_EL0t;
	regs.regs[1] = 0x123456789abcdef0ULL;
	regs.regs[2] = TASK_SIZE + PAGE_SIZE;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_USER_FAULT, result.reason);
	KUNIT_EXPECT_TRUE(test, result.status == -EFAULT || result.status == -EACCES);
	KUNIT_EXPECT_EQ(test, TASK_SIZE + PAGE_SIZE, result.fault_address);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_ACCESS_READ, result.fault_access);
	KUNIT_EXPECT_EQ(test, text, regs.pc);
	KUNIT_EXPECT_EQ(test, 0x123456789abcdef0ULL, regs.regs[1]);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(text, PAGE_SIZE));
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(data, PAGE_SIZE));
}

static void mte_production_resume_non_el0_is_undefined(struct kunit *test)
{
	static const size_t bulk_indices[] = { 5, 13, 17 };
	size_t index;

	for (index = 0; index < ARRAY_SIZE(bulk_indices); index++) {
		const struct mte_leaf *leaf = &mte_leaves[bulk_indices[index]];
		struct orlix_tcti_result result;
		struct pt_regs regs = {}, before;
		unsigned long text;
		u32 instruction = mte_instruction(leaf, 0, 3, 2, 0);

		text = mte_map_program(test, &instruction, 1);
		KUNIT_ASSERT_NE(test, 0UL, text);
		regs.pc = text;
		regs.sp = STACK_TOP - 16;
		regs.pstate = PSR_MODE_EL0t | PSR_N_BIT;
		regs.regs[2] = TASK_SIZE + PAGE_SIZE;
		regs.regs[3] = 0x7000000000000000ULL;
		before = regs;
		result = orlix_tcti_resume_user(current, &regs, current->mm);
		KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_EXIT_UNSUPPORTED_INSTRUCTION,
				    result.reason, "%s", leaf->name);
		KUNIT_EXPECT_EQ_MSG(test, -EOPNOTSUPP, result.status, "%s", leaf->name);
		KUNIT_EXPECT_EQ_MSG(test, instruction, result.instruction, "%s", leaf->name);
		KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(text, PAGE_SIZE));
	}
}

static void mte_memory_tags_data_writeback_and_faults(struct kunit *test)
{
	unsigned long mapped;
	unsigned long fault = 0;
	struct pt_regs regs = {};
	struct orlix_tcti_decoded_instruction d;
	u8 data[32];
	u8 tag;
	int ret;

	mapped = ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
				 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(mapped));
	memset(data, 0xa5, sizeof(data));
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_write_user_data(current->mm, mapped, data,
							      sizeof(data)));

	regs.pc = 0x1000;
	regs.pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_Z_BIT | PSR_C_BIT | PSR_V_BIT;
	regs.regs[2] = mapped;
	regs.regs[3] = (u64)9 << MTE_TAG_SHIFT;
	d = orlix_tcti_decode_aarch64(mte_instruction(&mte_leaves[2], 1, 3, 2, 0));
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_execute_decoded_semantics(current->mm,
								  &regs, &d, &fault));
	KUNIT_EXPECT_EQ(test, mapped + 16, regs.regs[2]);
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_mte_load_allocation_tag(current->mm,
							      mapped, &tag));
	KUNIT_EXPECT_EQ(test, (u8)9, tag);

	regs.pc = 0x2000;
	regs.regs[2] = mapped;
	regs.regs[3] = (u64)6 << MTE_TAG_SHIFT;
	d = orlix_tcti_decode_aarch64(mte_instruction(&mte_leaves[14], 0, 3, 2, 0));
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_execute_decoded_semantics(current->mm,
								  &regs, &d, &fault));
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_read_user_data(current->mm, mapped, data,
							     sizeof(data)));
	KUNIT_EXPECT_MEMEQ(test, data, (u8[32]){}, sizeof(data));
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_mte_load_allocation_tag(current->mm,
							      mapped + 16, &tag));
	KUNIT_EXPECT_EQ(test, (u8)6, tag);

	regs.pc = 0x3000;
	regs.regs[2] = mapped;
	regs.regs[1] = 0x123456789abcdef0ULL;
	d = orlix_tcti_decode_aarch64(mte_instruction(&mte_leaves[6], 0, 1, 2, 0));
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_execute_decoded_semantics(current->mm,
								  &regs, &d, &fault));
	KUNIT_EXPECT_EQ(test, 0x163456789abcdef0ULL,
			regs.regs[1]);

	regs.pc = 0x4000;
	regs.regs[2] = TASK_SIZE + PAGE_SIZE;
	d = orlix_tcti_decode_aarch64(mte_instruction(&mte_leaves[7], 1, 3, 2, 0));
	ret = orlix_tcti_execute_decoded_semantics(current->mm, &regs, &d, &fault);
	KUNIT_EXPECT_TRUE(test, ret == -EFAULT || ret == -EACCES);
	KUNIT_EXPECT_EQ(test, 0x4000UL, regs.pc);
	KUNIT_EXPECT_EQ(test, TASK_SIZE + PAGE_SIZE, regs.regs[2]);
	KUNIT_EXPECT_EQ(test,
		(u64)(PSR_MODE_EL0t | PSR_N_BIT | PSR_Z_BIT | PSR_C_BIT | PSR_V_BIT),
		regs.pstate);

	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
}

static struct kunit_case orlix_tcti_memory_tagging_source_bound_test_cases[] = {
	KUNIT_CASE(mte_source_cohort_is_exact),
	KUNIT_CASE(mte_all_legal_free_fields_decode),
	KUNIT_CASE(mte_fixed_bit_neighbours_are_not_the_leaf),
	KUNIT_CASE(mte_reserved_bulk_immediates_are_undefined),
	KUNIT_CASE(mte_register_semantics_and_non_el0),
	KUNIT_CASE(mte_memory_tags_data_writeback_and_faults),
	KUNIT_CASE(mte_production_resume_all_el0_leaves),
	KUNIT_CASE(mte_production_resume_register_memory_and_faults),
	KUNIT_CASE(mte_production_resume_non_el0_is_undefined),
	{}
};

struct kunit_suite orlix_tcti_memory_tagging_source_bound_test_suite = {
	.name = "orlix-tcti-memory-tagging-source-bound",
	.test_cases = orlix_tcti_memory_tagging_source_bound_test_cases,
};

kunit_test_suite(orlix_tcti_memory_tagging_source_bound_test_suite);
