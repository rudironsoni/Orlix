// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include <linux/err.h>
#include <linux/completion.h>
#include <linux/kthread.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/sched.h>
#include <linux/sched/mm.h>
#include <linux/sched/signal.h>
#include <linux/signal.h>
#include <linux/syscalls.h>
#include <asm/orlix_tcti.h>
#include <asm/mte.h>
#include <asm/page.h>
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
	regs.regs[2] = 0x0a12345678900000ULL;
	instruction = mte_instruction(&mte_leaves[0], (2U << 6) | 3U, 0, 2, 1);
	d = orlix_tcti_decode_aarch64(instruction);
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_execute_decoded_semantics(current->mm,
								  &regs, &d, &fault));
	KUNIT_EXPECT_EQ(test, 0x0c12345678900030ULL, regs.regs[1]);
	KUNIT_EXPECT_EQ(test, 0x1004UL, regs.pc);
	KUNIT_EXPECT_EQ(test, (u64)(PSR_MODE_EL0t | PSR_N_BIT | PSR_V_BIT),
			regs.pstate);

	regs.pc = 0x2000;
	current->thread.user_mte_exclude_mask = 0;
	regs.regs[2] = 0x0b000000000001234ULL;
	regs.regs[3] = 0xffffU & ~BIT(7);
	d = orlix_tcti_decode_aarch64(mte_instruction(&mte_leaves[18], 0, 3, 2, 1));
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_execute_decoded_semantics(current->mm,
								  &regs, &d, &fault));
	KUNIT_EXPECT_EQ(test, 0x0700000000001234ULL, regs.regs[1]);

	regs.pc = 0x3000;
	regs.regs[2] = 0x0c000000000001234ULL;
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

	data = ksys_mmap_pgoff(0, 2 * PAGE_SIZE, PROT_READ | PROT_WRITE | PROT_MTE,
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
			KUNIT_ASSERT_EQ(test, 0, orlix_mte_store_allocation_tag(
				current->mm, effective, leaf->op == ORLIX_TCTI_MTE_LDG ? 9 : 0));
			KUNIT_ASSERT_EQ(test, 0, orlix_mte_store_allocation_tag(
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
			KUNIT_ASSERT_EQ(test, 0, orlix_mte_load_allocation_tag(
				current->mm, effective, &tag));
			KUNIT_EXPECT_EQ(test, (u8)6, tag);
			if (leaf->op == ORLIX_TCTI_MTE_ST2G ||
			    leaf->op == ORLIX_TCTI_MTE_STZ2G) {
				KUNIT_ASSERT_EQ(test, 0, orlix_mte_load_allocation_tag(
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

	data = ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE | PROT_MTE,
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
	KUNIT_ASSERT_EQ(test, 0, orlix_mte_load_allocation_tag(current->mm,
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

	mapped = ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE | PROT_MTE,
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
	KUNIT_ASSERT_EQ(test, 0, orlix_mte_load_allocation_tag(current->mm,
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
	KUNIT_ASSERT_EQ(test, 0, orlix_mte_load_allocation_tag(current->mm,
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

static void mte_tagged_user_addresses_use_linux_untagged_lookup(struct kunit *test)
{
	unsigned long mapped, tagged;
	u8 byte = 0;
	int ret;

	mapped = ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE | PROT_MTE,
				 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(mapped));
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_write_user_data(current->mm, mapped,
							     &byte, sizeof(byte)));
	KUNIT_ASSERT_EQ(test, 0, orlix_mte_store_allocation_tag(current->mm,
								mapped, 5));
	tagged = mapped | (5UL << ORLIX_MTE_TAG_SHIFT);
	KUNIT_ASSERT_EQ(test, 0L, set_tagged_addr_ctrl(current,
		PR_TAGGED_ADDR_ENABLE | PR_MTE_TCF_SYNC));
	KUNIT_EXPECT_EQ(test, 0, orlix_tcti_read_user_data(current->mm, tagged,
							       &byte, sizeof(byte)));

	/* The same VMA/page is resolved from bits 55:0, but the tag is checked. */
	tagged = mapped | (4UL << ORLIX_MTE_TAG_SHIFT);
	ret = orlix_tcti_read_user_data(current->mm, tagged, &byte, sizeof(byte));
	KUNIT_EXPECT_EQ(test, -EHWPOISON, ret);
	KUNIT_EXPECT_EQ(test, 0L, set_tagged_addr_ctrl(current, 0));
	KUNIT_EXPECT_EQ(test, 0, orlix_tcti_read_user_data(current->mm, tagged,
							       &byte, sizeof(byte)));
	KUNIT_EXPECT_EQ(test, -EINVAL, set_tagged_addr_ctrl(current,
		PR_TAGGED_ADDR_ENABLE | PR_MTE_TCF_ASYNC));
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
}

static void mte_ordinary_accesses_produce_sync_fault_signal_state(struct kunit *test)
{
	static const u32 svc = 0xd4000001U;
	static const u32 strb = 0x39000020U; /* STRB w0, [x1] */
	static const u32 ldrb = 0x39400020U; /* LDRB w0, [x1] */
	struct orlix_tcti_result result;
	struct orlix_tcti_result debug_result;
	struct pt_regs regs = {};
	struct pt_regs debug_regs;
	kernel_siginfo_t info;
	enum pid_type type;
	sigset_t mask;
	unsigned long data, text, tagged;
	u8 byte;
	u64 pstate;
	u32 program[2];

	data = ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE | PROT_MTE,
			       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(data));
	KUNIT_ASSERT_EQ(test, 0, orlix_mte_store_allocation_tag(current->mm, data, 5));
	KUNIT_ASSERT_EQ(test, 0L, set_tagged_addr_ctrl(current,
		PR_TAGGED_ADDR_ENABLE | PR_MTE_TCF_SYNC));
	tagged = data | (5UL << MTE_TAG_SHIFT);

	/* Ordinary matching STRB and LDRB run through the production gadget path. */
	program[0] = strb;
	program[1] = svc;
	text = mte_map_program(test, program, ARRAY_SIZE(program));
	KUNIT_ASSERT_NE(test, 0UL, text);
	regs.pc = text;
	regs.sp = STACK_TOP - 16;
	regs.pstate = PSR_MODE_EL0t | PSR_Z_BIT;
	regs.regs[0] = 0xa5;
	regs.regs[1] = tagged;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_EXPECT_EQ(test, text + 2 * sizeof(u32), regs.pc);
	KUNIT_EXPECT_FALSE(test, sigismember(&current->pending.signal, SIGSEGV));
	KUNIT_ASSERT_EQ(test, 0, vm_munmap(text, PAGE_SIZE));

	program[0] = ldrb;
	program[1] = svc;
	text = mte_map_program(test, program, ARRAY_SIZE(program));
	KUNIT_ASSERT_NE(test, 0UL, text);
	memset(&regs, 0, sizeof(regs));
	regs.pc = text;
	regs.sp = STACK_TOP - 16;
	regs.pstate = PSR_MODE_EL0t | PSR_N_BIT;
	regs.regs[1] = tagged;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_EXPECT_EQ(test, 0xa5ULL, regs.regs[0]);
	KUNIT_EXPECT_EQ(test, text + 2 * sizeof(u32), regs.pc);
	KUNIT_EXPECT_FALSE(test, sigismember(&current->pending.signal, SIGSEGV));
	KUNIT_ASSERT_EQ(test, 0, vm_munmap(text, PAGE_SIZE));

	/* A mismatching ordinary store leaves registers, PC, and data intact. */
	program[0] = strb;
	text = mte_map_program(test, program, 1);
	KUNIT_ASSERT_NE(test, 0UL, text);
	memset(&regs, 0, sizeof(regs));
	regs.pc = text;
	regs.sp = STACK_TOP - 16;
	regs.pstate = PSR_MODE_EL0t | PSR_C_BIT;
	pstate = regs.pstate;
	regs.regs[0] = 0x3c;
	regs.regs[1] = data | (4UL << MTE_TAG_SHIFT);
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_MTE_TAG_FAULT, result.reason);
	KUNIT_EXPECT_EQ(test, -EHWPOISON, result.status);
	KUNIT_EXPECT_EQ(test, regs.regs[1], result.fault_address);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_ACCESS_WRITE, result.fault_access);
	KUNIT_EXPECT_EQ(test, text, regs.pc);
	KUNIT_EXPECT_EQ(test, pstate, regs.pstate);
	KUNIT_EXPECT_EQ(test, 0x3cULL, regs.regs[0]);

	/* Keep the production-linked switch-debug MTE fault exit typed as well. */
	debug_regs = (struct pt_regs) {};
	debug_regs.pc = text;
	debug_regs.sp = STACK_TOP - 16;
	debug_regs.pstate = PSR_MODE_EL0t | PSR_C_BIT;
	debug_regs.regs[0] = 0x3c;
	debug_regs.regs[1] = data | (4UL << MTE_TAG_SHIFT);
	debug_result = orlix_tcti_switch_debug_resume_user(current, &debug_regs,
						      current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_MTE_TAG_FAULT,
				debug_result.reason);
	KUNIT_EXPECT_EQ(test, -EHWPOISON, debug_result.status);
	KUNIT_EXPECT_EQ(test, debug_regs.regs[1], debug_result.fault_address);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_ACCESS_WRITE,
				debug_result.fault_access);
	KUNIT_EXPECT_EQ(test, text, debug_regs.pc);
	KUNIT_ASSERT_EQ(test, 0L, set_tagged_addr_ctrl(current, 0));
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_read_user_data(current->mm, data,
							       &byte, sizeof(byte)));
	KUNIT_EXPECT_EQ(test, (u8)0xa5, byte);
	KUNIT_ASSERT_EQ(test, 0L, set_tagged_addr_ctrl(current,
		PR_TAGGED_ADDR_ENABLE | PR_MTE_TCF_SYNC));

	/* This is the Linux-owned classification used by the entry loop. */
	orlix_mte_signal_sync_fault(&regs, result.fault_address);
	sigemptyset(&mask);
	memset(&info, 0, sizeof(info));
	KUNIT_ASSERT_EQ(test, SIGSEGV, dequeue_signal(&mask, &info, &type));
	KUNIT_EXPECT_EQ(test, SIGSEGV, info.si_signo);
	KUNIT_EXPECT_EQ(test, SEGV_MTESERR, info.si_code);
	KUNIT_EXPECT_EQ(test, data, (unsigned long)info.si_addr);
	KUNIT_EXPECT_EQ(test, text, regs.pc);
	KUNIT_EXPECT_EQ(test, pstate, regs.pstate);
	KUNIT_ASSERT_EQ(test, 0, vm_munmap(text, PAGE_SIZE));

	/* A mismatching ordinary load also returns the precise typed TCTI exit. */
	program[0] = ldrb;
	text = mte_map_program(test, program, 1);
	KUNIT_ASSERT_NE(test, 0UL, text);
	memset(&regs, 0, sizeof(regs));
	regs.pc = text;
	regs.sp = STACK_TOP - 16;
	regs.pstate = PSR_MODE_EL0t | PSR_V_BIT;
	pstate = regs.pstate;
	regs.regs[0] = 0xfeed;
	regs.regs[1] = data | (6UL << MTE_TAG_SHIFT);
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_MTE_TAG_FAULT, result.reason);
	KUNIT_EXPECT_EQ(test, -EHWPOISON, result.status);
	KUNIT_EXPECT_EQ(test, regs.regs[1], result.fault_address);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_ACCESS_READ, result.fault_access);
	KUNIT_EXPECT_EQ(test, text, regs.pc);
	KUNIT_EXPECT_EQ(test, pstate, regs.pstate);
	KUNIT_EXPECT_EQ(test, 0xfeedULL, regs.regs[0]);
	KUNIT_EXPECT_FALSE(test, sigismember(&current->pending.signal, SIGSEGV));
	KUNIT_EXPECT_EQ(test, 0L, set_tagged_addr_ctrl(current, 0));
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(text, PAGE_SIZE));
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(data, PAGE_SIZE));
}

static void mte_prctl_tcf_dispositions_are_source_classified(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, ORLIX_MTE_PRCTL_TCF_SYNC_SUPPORTED,
			orlix_mte_prctl_tcf_disposition(PR_MTE_TCF_SYNC));
	KUNIT_EXPECT_EQ(test, ORLIX_MTE_PRCTL_TCF_ASYNC_REJECTED_NO_RETURN_TO_USER,
			orlix_mte_prctl_tcf_disposition(PR_MTE_TCF_ASYNC));
	/* v6.12's PR_MTE_TCF_MASK has no ASYMM encoding. */
	KUNIT_EXPECT_FALSE(test, PR_MTE_TCF_MASK & BIT(3));
	KUNIT_EXPECT_EQ(test, ORLIX_MTE_PRCTL_TCF_ASYMM_UNAVAILABLE_V612_UAPI,
			ORLIX_MTE_PRCTL_TCF_ASYMM_UNAVAILABLE_V612_UAPI);
}

/* Production RX path: a logical tag must match every granule before the
 * decoder commits a register or stores any byte.  X/Q forms cross at +15. */
static void mte_resume_cross_granule_accesses_do_not_commit(struct kunit *test)
{
	static const u32 ldr_x0 = 0xf9400020U, str_x0 = 0xf9000020U;
	static const u32 ldr_q0 = 0x3dc00020U, str_q0 = 0x3d800020U, svc = 0xd4000001U;
	const u32 instructions[] = { ldr_x0, svc };
	const u32 simd_instructions[] = { ldr_q0, svc };
	struct orlix_tcti_result result;
	struct pt_regs regs = {};
	unsigned long data, text, tagged;
	u8 before[32], after[32];

	data = ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE | PROT_MTE,
				 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(data));
	memset(before, 0xa5, sizeof(before));
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_write_user_data(current->mm, data,
						     before, sizeof(before)));
	KUNIT_ASSERT_EQ(test, 0, orlix_mte_store_allocation_tag(current->mm, data, 5));
	KUNIT_ASSERT_EQ(test, 0, orlix_mte_store_allocation_tag(current->mm, data + 16, 6));
	KUNIT_ASSERT_EQ(test, 0L, set_tagged_addr_ctrl(current,
		PR_TAGGED_ADDR_ENABLE | PR_MTE_TCF_SYNC));
	tagged = data + 15 | (5UL << MTE_TAG_SHIFT);
	text = mte_map_program(test, instructions, ARRAY_SIZE(instructions));
	regs.pc = text; regs.sp = STACK_TOP - 16; regs.pstate = PSR_MODE_EL0t | PSR_C_BIT;
	regs.regs[0] = 0xfeed; regs.regs[1] = tagged;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_MTE_TAG_FAULT, result.reason);
	KUNIT_EXPECT_EQ(test, text, regs.pc);
	KUNIT_EXPECT_EQ(test, 0xfeedULL, regs.regs[0]);
	KUNIT_ASSERT_EQ(test, 0, vm_munmap(text, PAGE_SIZE));
	text = mte_map_program(test, (u32[]){ str_x0 }, 1);
	regs.pc = text; regs.regs[0] = 0x1122334455667788ULL; regs.regs[1] = tagged;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_MTE_TAG_FAULT, result.reason);
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_read_user_data(current->mm, data, after, sizeof(after)));
	KUNIT_EXPECT_MEMEQ(test, before, after, sizeof(before));
	KUNIT_ASSERT_EQ(test, 0, vm_munmap(text, PAGE_SIZE));
	text = mte_map_program(test, simd_instructions, ARRAY_SIZE(simd_instructions));
	regs.pc = text; regs.regs[1] = tagged;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_MTE_TAG_FAULT, result.reason);
	KUNIT_ASSERT_EQ(test, 0, vm_munmap(text, PAGE_SIZE));
	text = mte_map_program(test, (u32[]){ str_q0, svc }, 2);
	regs.pc = text; regs.regs[1] = tagged;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_MTE_TAG_FAULT, result.reason);
	KUNIT_ASSERT_EQ(test, 0, vm_munmap(text, PAGE_SIZE));
	/* Matching both granules is the control for each RX family. */
	KUNIT_ASSERT_EQ(test, 0, orlix_mte_store_allocation_tag(current->mm, data + 16, 5));
	text = mte_map_program(test, instructions, ARRAY_SIZE(instructions));
	regs.pc = text; regs.regs[1] = tagged; regs.regs[0] = 0;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_ASSERT_EQ(test, 0, vm_munmap(text, PAGE_SIZE));
	text = mte_map_program(test, simd_instructions, ARRAY_SIZE(simd_instructions));
	regs.pc = text; regs.regs[1] = tagged;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_ASSERT_EQ(test, 0, vm_munmap(text, PAGE_SIZE));
	KUNIT_ASSERT_EQ(test, 0L, set_tagged_addr_ctrl(current, 0));
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(data, PAGE_SIZE));
}

/* Production RX atomic paths use the same x10 address, w6 status/source, and
 * x8 data/result convention as the exclusive and LSE resume suites.  A 64-bit
 * access at +15 spans both 16-byte allocation-tag granules. */
static void mte_resume_cross_granule_atomics_do_not_commit(struct kunit *test)
{
	static const u32 ldxr_x8 = 0xc85f7d48U;
	static const u32 stxr_w6_x8 = 0xc8067d48U;
	static const u32 ldadd_x6_x8 = 0xf8260148U;
	static const u32 svc = 0xd4000001U;
	const u64 desired = 0x1122334455667788ULL;
	const u64 operand = 0x0102030405060708ULL;
	const u64 status_sentinel = 0xa5a5a5a5a5a5a5a5ULL;
	const u64 result_sentinel = 0xfeedfacecafebeefULL;
	const u64 pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_Z_BIT |
		PSR_C_BIT | PSR_V_BIT;
	struct orlix_tcti_decoded_instruction decoded;
	struct orlix_tcti_result result;
	struct pt_regs regs = {};
	struct pt_regs before;
	unsigned long data, text, tagged;
	unsigned long monitor_address;
	u64 monitor_value, monitor_value2, monitor_generation;
	u64 monitor_mapping_generation;
	unsigned long monitor_pfn;
	u8 monitor_size, monitor_valid;
	u8 initial[32], observed[32], expected[32];
	u64 old;

	decoded = orlix_tcti_decode_aarch64(ldxr_x8);
	KUNIT_ASSERT_EQ(test, ORLIX_TCTI_DECODE_LOAD_STORE_EXCLUSIVE,
			decoded.decode_class);
	KUNIT_EXPECT_TRUE(test, decoded.load);
	KUNIT_EXPECT_EQ(test, (u8)sizeof(u64), decoded.access_size);
	decoded = orlix_tcti_decode_aarch64(stxr_w6_x8);
	KUNIT_ASSERT_EQ(test, ORLIX_TCTI_DECODE_LOAD_STORE_EXCLUSIVE,
			decoded.decode_class);
	KUNIT_EXPECT_FALSE(test, decoded.load);
	KUNIT_EXPECT_EQ(test, (u8)sizeof(u64), decoded.access_size);
	decoded = orlix_tcti_decode_aarch64(ldadd_x6_x8);
	KUNIT_ASSERT_EQ(test, ORLIX_TCTI_DECODE_LSE_ATOMIC, decoded.decode_class);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_LSE_ATOMIC_ADD, decoded.lse_atomic_op);
	KUNIT_EXPECT_EQ(test, (u8)sizeof(u64), decoded.access_size);

	data = ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE | PROT_MTE,
				 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(data));
	memset(initial, 0xa5, sizeof(initial));
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_write_user_data(current->mm, data,
						     initial, sizeof(initial)));
	KUNIT_ASSERT_EQ(test, 0, orlix_mte_store_allocation_tag(current->mm, data, 5));
	KUNIT_ASSERT_EQ(test, 0, orlix_mte_store_allocation_tag(current->mm, data + 16, 5));
	KUNIT_ASSERT_EQ(test, 0L, set_tagged_addr_ctrl(current,
		PR_TAGGED_ADDR_ENABLE | PR_MTE_TCF_SYNC));
	tagged = (data + 15) | (5UL << MTE_TAG_SHIFT);

	/* LDXR establishes the production reservation before the mismatched STXR. */
	text = mte_map_program(test, (u32[]){ ldxr_x8, svc }, 2);
	regs.pc = text;
	regs.sp = STACK_TOP - 16;
	regs.pstate = pstate;
	regs.regs[10] = tagged;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_ASSERT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_ASSERT_EQ(test, 1, current->thread.user_exclusive_valid);
	KUNIT_EXPECT_EQ(test, 0xa5a5a5a5a5a5a5a5ULL, regs.regs[8]);
	monitor_address = current->thread.user_exclusive_address;
	monitor_value = current->thread.user_exclusive_value;
	monitor_value2 = current->thread.user_exclusive_value2;
	monitor_pfn = current->thread.user_exclusive_pfn;
	monitor_generation = current->thread.user_exclusive_generation;
	monitor_mapping_generation = current->thread.user_exclusive_mapping_generation;
	monitor_size = current->thread.user_exclusive_size;
	monitor_valid = current->thread.user_exclusive_valid;
	KUNIT_ASSERT_EQ(test, 0, vm_munmap(text, PAGE_SIZE));

	KUNIT_ASSERT_EQ(test, 0, orlix_mte_store_allocation_tag(current->mm, data + 16, 6));
	text = mte_map_program(test, (u32[]){ stxr_w6_x8, svc }, 2);
	regs.pc = text;
	regs.regs[6] = status_sentinel;
	regs.regs[8] = desired;
	before = regs;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_MTE_TAG_FAULT, result.reason);
	KUNIT_EXPECT_EQ(test, -EHWPOISON, result.status);
	KUNIT_EXPECT_EQ(test, tagged, result.fault_address);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_ACCESS_WRITE, result.fault_access);
	KUNIT_EXPECT_EQ(test, text, result.pc);
	KUNIT_EXPECT_EQ(test, stxr_w6_x8, result.instruction);
	KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
	KUNIT_EXPECT_EQ(test, pstate, regs.pstate);
	KUNIT_EXPECT_EQ(test, monitor_address, current->thread.user_exclusive_address);
	KUNIT_EXPECT_EQ(test, monitor_value, current->thread.user_exclusive_value);
	KUNIT_EXPECT_EQ(test, monitor_value2, current->thread.user_exclusive_value2);
	KUNIT_EXPECT_EQ(test, monitor_pfn, current->thread.user_exclusive_pfn);
	KUNIT_EXPECT_EQ(test, monitor_generation, current->thread.user_exclusive_generation);
	KUNIT_EXPECT_EQ(test, monitor_mapping_generation,
			current->thread.user_exclusive_mapping_generation);
	KUNIT_EXPECT_EQ(test, monitor_size, current->thread.user_exclusive_size);
	KUNIT_EXPECT_EQ(test, monitor_valid, current->thread.user_exclusive_valid);
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_read_user_data(current->mm, data,
						    observed, sizeof(observed)));
	KUNIT_EXPECT_MEMEQ(test, initial, observed, sizeof(initial));

	/* Both matching tags permit STXR to consume the reservation and commit. */
	KUNIT_ASSERT_EQ(test, 0, orlix_mte_store_allocation_tag(current->mm, data + 16, 5));
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_EXPECT_EQ(test, 0ULL, regs.regs[6]);
	KUNIT_EXPECT_EQ(test, text + sizeof(u32), regs.pc);
	KUNIT_EXPECT_EQ(test, pstate, regs.pstate);
	KUNIT_EXPECT_EQ(test, 0, current->thread.user_exclusive_valid);
	memcpy(expected, initial, sizeof(expected));
	memcpy(expected + 15, &desired, sizeof(desired));
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_read_user_data(current->mm, data,
						    observed, sizeof(observed)));
	KUNIT_EXPECT_MEMEQ(test, expected, observed, sizeof(expected));
	KUNIT_ASSERT_EQ(test, 0, vm_munmap(text, PAGE_SIZE));

	/* Re-establish a reservation, then ensure LDADD's fault leaves it and all
	 * architectural result state untouched. */
	text = mte_map_program(test, (u32[]){ ldxr_x8, svc }, 2);
	regs = (struct pt_regs) {};
	regs.pc = text;
	regs.sp = STACK_TOP - 16;
	regs.pstate = pstate;
	regs.regs[10] = tagged;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_ASSERT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_ASSERT_EQ(test, 1, current->thread.user_exclusive_valid);
	KUNIT_ASSERT_EQ(test, 0, vm_munmap(text, PAGE_SIZE));
	monitor_address = current->thread.user_exclusive_address;
	monitor_value = current->thread.user_exclusive_value;
	monitor_value2 = current->thread.user_exclusive_value2;
	monitor_pfn = current->thread.user_exclusive_pfn;
	monitor_generation = current->thread.user_exclusive_generation;
	monitor_mapping_generation = current->thread.user_exclusive_mapping_generation;
	monitor_size = current->thread.user_exclusive_size;
	monitor_valid = current->thread.user_exclusive_valid;

	KUNIT_ASSERT_EQ(test, 0, orlix_mte_store_allocation_tag(current->mm, data + 16, 6));
	text = mte_map_program(test, (u32[]){ ldadd_x6_x8, svc }, 2);
	regs.pc = text;
	regs.regs[6] = operand;
	regs.regs[8] = result_sentinel;
	before = regs;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_MTE_TAG_FAULT, result.reason);
	KUNIT_EXPECT_EQ(test, -EHWPOISON, result.status);
	KUNIT_EXPECT_EQ(test, tagged, result.fault_address);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_ACCESS_WRITE, result.fault_access);
	KUNIT_EXPECT_EQ(test, text, result.pc);
	KUNIT_EXPECT_EQ(test, ldadd_x6_x8, result.instruction);
	KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
	KUNIT_EXPECT_EQ(test, pstate, regs.pstate);
	KUNIT_EXPECT_EQ(test, monitor_address, current->thread.user_exclusive_address);
	KUNIT_EXPECT_EQ(test, monitor_value, current->thread.user_exclusive_value);
	KUNIT_EXPECT_EQ(test, monitor_value2, current->thread.user_exclusive_value2);
	KUNIT_EXPECT_EQ(test, monitor_pfn, current->thread.user_exclusive_pfn);
	KUNIT_EXPECT_EQ(test, monitor_generation, current->thread.user_exclusive_generation);
	KUNIT_EXPECT_EQ(test, monitor_mapping_generation,
			current->thread.user_exclusive_mapping_generation);
	KUNIT_EXPECT_EQ(test, monitor_size, current->thread.user_exclusive_size);
	KUNIT_EXPECT_EQ(test, monitor_valid, current->thread.user_exclusive_valid);
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_read_user_data(current->mm, data,
						    observed, sizeof(observed)));
	KUNIT_EXPECT_MEMEQ(test, expected, observed, sizeof(expected));

	/* The matching LDADD control returns the old value in x8 and commits once. */
	KUNIT_ASSERT_EQ(test, 0, orlix_mte_store_allocation_tag(current->mm, data + 16, 5));
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_EXPECT_EQ(test, text + sizeof(u32), regs.pc);
	KUNIT_EXPECT_EQ(test, pstate, regs.pstate);
	memcpy(&old, expected + 15, sizeof(old));
	KUNIT_EXPECT_EQ(test, old, regs.regs[8]);
	old += operand;
	memcpy(expected + 15, &old, sizeof(old));
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_read_user_data(current->mm, data,
						    observed, sizeof(observed)));
	KUNIT_EXPECT_MEMEQ(test, expected, observed, sizeof(expected));
	KUNIT_EXPECT_EQ(test, 0, current->thread.user_exclusive_valid);
	KUNIT_ASSERT_EQ(test, 0, vm_munmap(text, PAGE_SIZE));
	KUNIT_ASSERT_EQ(test, 0L, set_tagged_addr_ctrl(current, 0));
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(data, PAGE_SIZE));
}

/* A size-aware check must fail closed when the first byte is in an eligible
 * VM_MTE VMA but the final byte is not.  These are RX programs so the result
 * covers the actual resume path rather than the user-data helpers alone. */
static void mte_resume_cross_vma_boundaries_do_not_commit(struct kunit *test)
{
	static const u32 ldr_q0 = 0x3dc00020U;
	static const u32 str_q0 = 0x3d800020U;
	static const u32 svc = 0xd4000001U;
	const u64 pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_Z_BIT |
		PSR_C_BIT | PSR_V_BIT;
	struct orlix_tcti_result result;
	struct pt_regs regs = {};
	struct pt_regs before;
	unsigned long data, text, tagged;
	u8 boundary[16], observed[16];
	u64 simd_low, simd_high, expected_low, expected_high;
	unsigned long simd_valid;

	for (simd_low = 0; simd_low < ARRAY_SIZE(boundary); simd_low++)
		boundary[simd_low] = 0x80U + simd_low;

	/* A cross-page read into an unmapped hole is an ordinary read fault, before
	 * either half of Q0, any GPR, PC, or NZCV can be committed. */
	data = ksys_mmap_pgoff(0, 2 * PAGE_SIZE, PROT_READ | PROT_WRITE | PROT_MTE,
				 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(data));
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_write_user_data(current->mm,
		data + PAGE_SIZE - sizeof(boundary) / 2, boundary, sizeof(boundary)));
	KUNIT_ASSERT_EQ(test, 0, orlix_mte_store_allocation_tag(current->mm,
		data + PAGE_SIZE - ORLIX_MTE_GRANULE_SIZE, 5));
	KUNIT_ASSERT_EQ(test, 0, vm_munmap(data + PAGE_SIZE, PAGE_SIZE));
	KUNIT_ASSERT_EQ(test, 0L, set_tagged_addr_ctrl(current,
		PR_TAGGED_ADDR_ENABLE | PR_MTE_TCF_SYNC));
	tagged = (data + PAGE_SIZE - sizeof(boundary) / 2) |
		(5UL << MTE_TAG_SHIFT);
	text = mte_map_program(test, (u32[]){ ldr_q0 }, 1);
	regs.pc = text;
	regs.sp = STACK_TOP - 16;
	regs.pstate = pstate;
	regs.regs[1] = tagged;
	current->thread.user_simd[0] = 0x1111222233334444ULL;
	current->thread.user_simd[1] = 0x5555666677778888ULL;
	current->thread.user_simd_valid = 1;
	before = regs;
	simd_low = current->thread.user_simd[0];
	simd_high = current->thread.user_simd[1];
	simd_valid = current->thread.user_simd_valid;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_USER_FAULT, result.reason);
	KUNIT_EXPECT_EQ(test, -EACCES, result.status);
	KUNIT_EXPECT_EQ(test, tagged, result.fault_address);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_ACCESS_READ, result.fault_access);
	KUNIT_EXPECT_EQ(test, text, result.pc);
	KUNIT_EXPECT_EQ(test, ldr_q0, result.instruction);
	KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
	KUNIT_EXPECT_EQ(test, pstate, regs.pstate);
	KUNIT_EXPECT_EQ(test, simd_low, current->thread.user_simd[0]);
	KUNIT_EXPECT_EQ(test, simd_high, current->thread.user_simd[1]);
	KUNIT_EXPECT_EQ(test, simd_valid, current->thread.user_simd_valid);
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_read_user_data(current->mm,
		data + PAGE_SIZE - sizeof(observed) / 2, observed,
		sizeof(observed) / 2));
	KUNIT_EXPECT_MEMEQ(test, boundary, observed, sizeof(observed) / 2);
	KUNIT_ASSERT_EQ(test, 0, vm_munmap(text, PAGE_SIZE));
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(data, PAGE_SIZE));

	/* A second VM_MTE page that is read-only rejects the cross-page SIMD store
	 * as a write fault, without a partial memory or SIMD/register commit. */
	data = ksys_mmap_pgoff(0, 2 * PAGE_SIZE, PROT_READ | PROT_WRITE | PROT_MTE,
				 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(data));
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_write_user_data(current->mm,
		data + PAGE_SIZE - sizeof(boundary) / 2, boundary, sizeof(boundary)));
	KUNIT_ASSERT_EQ(test, 0, orlix_mte_store_allocation_tag(current->mm,
		data + PAGE_SIZE - ORLIX_MTE_GRANULE_SIZE, 5));
	KUNIT_ASSERT_EQ(test, 0, orlix_mte_store_allocation_tag(current->mm,
		data + PAGE_SIZE, 5));
	KUNIT_ASSERT_EQ(test, 0, sys_mprotect(data + PAGE_SIZE, PAGE_SIZE,
		PROT_READ | PROT_MTE));
	tagged = (data + PAGE_SIZE - sizeof(boundary) / 2) |
		(5UL << MTE_TAG_SHIFT);
	text = mte_map_program(test, (u32[]){ str_q0 }, 1);
	regs = (struct pt_regs) {};
	regs.pc = text;
	regs.sp = STACK_TOP - 16;
	regs.pstate = pstate;
	regs.regs[1] = tagged;
	current->thread.user_simd[0] = 0x0123456789abcdefULL;
	current->thread.user_simd[1] = 0xfedcba9876543210ULL;
	current->thread.user_simd_valid = 1;
	before = regs;
	simd_low = current->thread.user_simd[0];
	simd_high = current->thread.user_simd[1];
	simd_valid = current->thread.user_simd_valid;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_USER_FAULT, result.reason);
	KUNIT_EXPECT_EQ(test, -EACCES, result.status);
	KUNIT_EXPECT_EQ(test, tagged, result.fault_address);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_ACCESS_WRITE, result.fault_access);
	KUNIT_EXPECT_EQ(test, text, result.pc);
	KUNIT_EXPECT_EQ(test, str_q0, result.instruction);
	KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
	KUNIT_EXPECT_EQ(test, pstate, regs.pstate);
	KUNIT_EXPECT_EQ(test, simd_low, current->thread.user_simd[0]);
	KUNIT_EXPECT_EQ(test, simd_high, current->thread.user_simd[1]);
	KUNIT_EXPECT_EQ(test, simd_valid, current->thread.user_simd_valid);
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_read_user_data(current->mm,
		data + PAGE_SIZE - sizeof(observed) / 2, observed, sizeof(observed)));
	KUNIT_EXPECT_MEMEQ(test, boundary, observed, sizeof(boundary));
	KUNIT_ASSERT_EQ(test, 0, vm_munmap(text, PAGE_SIZE));
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(data, 2 * PAGE_SIZE));

	/* Both VM_MTE pages and allocation tags matching is the cross-page control. */
	data = ksys_mmap_pgoff(0, 2 * PAGE_SIZE, PROT_READ | PROT_WRITE | PROT_MTE,
				 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(data));
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_write_user_data(current->mm,
		data + PAGE_SIZE - sizeof(boundary) / 2, boundary, sizeof(boundary)));
	KUNIT_ASSERT_EQ(test, 0, orlix_mte_store_allocation_tag(current->mm,
		data + PAGE_SIZE - ORLIX_MTE_GRANULE_SIZE, 5));
	KUNIT_ASSERT_EQ(test, 0, orlix_mte_store_allocation_tag(current->mm,
		data + PAGE_SIZE, 5));
	tagged = (data + PAGE_SIZE - sizeof(boundary) / 2) |
		(5UL << MTE_TAG_SHIFT);
	text = mte_map_program(test, (u32[]){ ldr_q0, svc }, 2);
	regs = (struct pt_regs) {};
	regs.pc = text;
	regs.sp = STACK_TOP - 16;
	regs.pstate = pstate;
	regs.regs[1] = tagged;
	current->thread.user_simd[0] = 0;
	current->thread.user_simd[1] = 0;
	current->thread.user_simd_valid = 0;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_EXPECT_EQ(test, text + sizeof(u32), regs.pc);
	KUNIT_EXPECT_EQ(test, pstate, regs.pstate);
	memcpy(&expected_low, boundary, sizeof(expected_low));
	memcpy(&expected_high, boundary + sizeof(expected_low), sizeof(expected_high));
	KUNIT_EXPECT_EQ(test, expected_low, current->thread.user_simd[0]);
	KUNIT_EXPECT_EQ(test, expected_high, current->thread.user_simd[1]);
	KUNIT_EXPECT_EQ(test, 1, current->thread.user_simd_valid);
	KUNIT_ASSERT_EQ(test, 0, vm_munmap(text, PAGE_SIZE));

	/* The same mapped second page, with only its allocation tag mismatched,
	 * must retain the typed MTE fault instead of degrading to a VMA fault. */
	KUNIT_ASSERT_EQ(test, 0, orlix_mte_store_allocation_tag(current->mm,
		data + PAGE_SIZE, 6));
	text = mte_map_program(test, (u32[]){ ldr_q0 }, 1);
	regs.pc = text;
	current->thread.user_simd[0] = 0x1111222233334444ULL;
	current->thread.user_simd[1] = 0x5555666677778888ULL;
	current->thread.user_simd_valid = 1;
	before = regs;
	simd_low = current->thread.user_simd[0];
	simd_high = current->thread.user_simd[1];
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_MTE_TAG_FAULT, result.reason);
	KUNIT_EXPECT_EQ(test, -EHWPOISON, result.status);
	KUNIT_EXPECT_EQ(test, tagged, result.fault_address);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_ACCESS_READ, result.fault_access);
	KUNIT_EXPECT_EQ(test, text, result.pc);
	KUNIT_EXPECT_EQ(test, ldr_q0, result.instruction);
	KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
	KUNIT_EXPECT_EQ(test, pstate, regs.pstate);
	KUNIT_EXPECT_EQ(test, simd_low, current->thread.user_simd[0]);
	KUNIT_EXPECT_EQ(test, simd_high, current->thread.user_simd[1]);
	KUNIT_EXPECT_EQ(test, 1, current->thread.user_simd_valid);
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_read_user_data(current->mm,
		data + PAGE_SIZE - sizeof(observed) / 2, observed, sizeof(observed)));
	KUNIT_EXPECT_MEMEQ(test, boundary, observed, sizeof(boundary));
	KUNIT_ASSERT_EQ(test, 0, vm_munmap(text, PAGE_SIZE));
	KUNIT_ASSERT_EQ(test, 0L, set_tagged_addr_ctrl(current, 0));
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(data, 2 * PAGE_SIZE));
}

struct mte_pair_contention_state {
	struct mm_struct *mm;
	unsigned long data;
	unsigned long text[2];
	bool zeroing;
	atomic_t completed;
	atomic_t failed;
	struct completion ready[2];
	struct completion start;
};

struct mte_pair_contention_writer {
	struct mte_pair_contention_state *state;
	u8 tag;
	unsigned int index;
};

static int mte_pair_contention_writer(void *data)
{
	struct mte_pair_contention_writer *writer = data;
	struct mte_pair_contention_state *state = writer->state;
	unsigned int iteration;

	kthread_use_mm(state->mm);
	complete(&state->ready[writer->index]);
	wait_for_completion(&state->start);
	for (iteration = 0; iteration < 128; iteration++) {
		struct orlix_tcti_result result;
		struct pt_regs regs = {};

		regs.pc = state->text[writer->index];
		regs.sp = STACK_TOP - 16;
		regs.pstate = PSR_MODE_EL0t;
		regs.regs[2] = state->data;
		regs.regs[3] = (u64)writer->tag << MTE_TAG_SHIFT;
		result = orlix_tcti_resume_user(current, &regs, state->mm);
		if (result.reason != ORLIX_TCTI_EXIT_SYSCALL || result.status ||
		    regs.pc != state->text[writer->index] + sizeof(u32)) {
			atomic_set(&state->failed, 1);
			break;
		}
		cond_resched();
	}
	atomic_inc(&state->completed);
	kthread_unuse_mm(state->mm);
	return 0;
}

static void mte_pair_contention_runs_through_resume(struct kunit *test,
					     bool zeroing)
{
	static const u32 svc = 0xd4000001U;
	struct mte_pair_contention_state state = {
		.zeroing = zeroing,
	};
	struct mte_pair_contention_writer writers[] = {
		{ .state = &state, .tag = 3, .index = 0 },
		{ .state = &state, .tag = 6, .index = 1 },
	};
	struct task_struct *tasks[ARRAY_SIZE(writers)];
	u32 program[2];
	unsigned long mapped;
	u8 tags[2], bytes[2 * ORLIX_MTE_GRANULE_SIZE];
	unsigned int index;

	mapped = ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE | PROT_MTE,
				 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(mapped));
	memset(bytes, 0xa5, sizeof(bytes));
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_write_user_data(current->mm, mapped,
							bytes, sizeof(bytes)));
	KUNIT_ASSERT_EQ(test, 0, orlix_mte_store_allocation_tag(current->mm, mapped, 1));
	KUNIT_ASSERT_EQ(test, 0, orlix_mte_store_allocation_tag(current->mm,
							mapped + ORLIX_MTE_GRANULE_SIZE, 1));
	state.mm = current->mm;
	mmget(state.mm);
	init_completion(&state.start);
	for (index = 0; index < ARRAY_SIZE(writers); index++) {
		const struct mte_leaf *leaf = &mte_leaves[zeroing ? 14 : 10];

		init_completion(&state.ready[index]);
		program[0] = mte_instruction(leaf, 0, 3, 2, 0);
		program[1] = svc;
		state.text[index] = mte_map_program(test, program, ARRAY_SIZE(program));
		KUNIT_ASSERT_NE(test, 0UL, state.text[index]);
		tasks[index] = kthread_run(mte_pair_contention_writer, &writers[index],
						  "orlix-mte-pair-%u", index);
		KUNIT_ASSERT_FALSE(test, IS_ERR(tasks[index]));
		KUNIT_ASSERT_TRUE(test, wait_for_completion_timeout(&state.ready[index],
								    msecs_to_jiffies(5000)));
	}

	complete_all(&state.start);
	while (atomic_read(&state.completed) != ARRAY_SIZE(writers)) {
		KUNIT_ASSERT_EQ(test, 0, orlix_mte_load_allocation_tags(current->mm,
								mapped, tags, ARRAY_SIZE(tags)));
		KUNIT_EXPECT_EQ(test, tags[0], tags[1]);
		KUNIT_EXPECT_TRUE(test, tags[0] == 1 || tags[0] == 3 || tags[0] == 6);
		if (zeroing && tags[0] != 1) {
			KUNIT_ASSERT_EQ(test, 0, orlix_tcti_read_user_data(current->mm,
									mapped, bytes, sizeof(bytes)));
			KUNIT_EXPECT_MEMEQ(test, bytes, (u8[32]){}, sizeof(bytes));
		}
		cond_resched();
	}
	for (index = 0; index < ARRAY_SIZE(tasks); index++)
		KUNIT_EXPECT_EQ(test, 0, kthread_stop(tasks[index]));
	KUNIT_EXPECT_EQ(test, 0, atomic_read(&state.failed));
	KUNIT_ASSERT_EQ(test, 0, orlix_mte_load_allocation_tags(current->mm,
							mapped, tags, ARRAY_SIZE(tags)));
	KUNIT_EXPECT_EQ(test, tags[0], tags[1]);
	KUNIT_EXPECT_TRUE(test, tags[0] == 3 || tags[0] == 6);
	if (zeroing) {
		KUNIT_ASSERT_EQ(test, 0, orlix_tcti_read_user_data(current->mm, mapped,
								bytes, sizeof(bytes)));
		KUNIT_EXPECT_MEMEQ(test, bytes, (u8[32]){}, sizeof(bytes));
	}
	mmput(state.mm);
	for (index = 0; index < ARRAY_SIZE(state.text); index++)
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(state.text[index], PAGE_SIZE));
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
}

static void mte_st2g_pair_contention_has_no_torn_pair(struct kunit *test)
{
	mte_pair_contention_runs_through_resume(test, false);
}

static void mte_stz2g_pair_contention_orders_zero_before_tags(struct kunit *test)
{
	mte_pair_contention_runs_through_resume(test, true);
}

static void mte_expose_tagged_page(struct kunit *test, unsigned long mapped)
{
	u8 byte = 0;

	/* Resolve through the production user-page path, including VM_MTE setup. */
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_write_user_data(current->mm, mapped,
							     &byte, sizeof(byte)));
}

static unsigned long mte_map_tagged_page(struct kunit *test, unsigned long flags)
{
	unsigned long mapped;

	mapped = ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE | PROT_MTE,
				 flags | MAP_ANONYMOUS, -1, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(mapped));
	mte_expose_tagged_page(test, mapped);
	return mapped;
}

static unsigned long mte_page_pfn(struct kunit *test, unsigned long address)
{
	struct orlix_tcti_user_page page;
	unsigned long pfn;

	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_pin_user_page_faulting(current->mm,
				address, ORLIX_TCTI_ACCESS_READ, &page));
	pfn = page_to_pfn(page.page);
	orlix_tcti_unpin_user_page(&page);
	return pfn;
}

static void mte_allocator_and_first_exposure_start_with_zero_tags(struct kunit *test)
{
	unsigned long mapped;
	u8 tag = U8_MAX;

	mapped = mte_map_tagged_page(test, MAP_PRIVATE);
	KUNIT_EXPECT_EQ(test, 0, orlix_mte_load_allocation_tag(current->mm,
								  mapped, &tag));
	KUNIT_EXPECT_EQ(test, (u8)0, tag);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
}

static void mte_madvise_discard_and_refault_clear_page_tags(struct kunit *test)
{
	unsigned long mapped;
	unsigned long before_pfn;
	u8 tag = U8_MAX;

	mapped = mte_map_tagged_page(test, MAP_PRIVATE);
	before_pfn = mte_page_pfn(test, mapped);
	KUNIT_ASSERT_EQ(test, 0, orlix_mte_store_allocation_tag(current->mm,
								mapped, 0xd));
	KUNIT_ASSERT_EQ(test, 0L, sys_madvise(mapped, PAGE_SIZE, MADV_DONTNEED));
	/* The replacement page must be exposed through the normal fault path. */
	mte_expose_tagged_page(test, mapped);
	KUNIT_EXPECT_EQ(test, before_pfn, mte_page_pfn(test, mapped));
	KUNIT_ASSERT_EQ(test, 0, orlix_mte_load_allocation_tag(current->mm,
								  mapped, &tag));
	KUNIT_EXPECT_EQ(test, (u8)0, tag);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
}

static void mte_copy_highpage_copies_tags_then_keeps_pages_independent(struct kunit *test)
{
	struct orlix_tcti_user_page source;
	struct orlix_tcti_user_page destination;
	unsigned long mapped;
	u8 source_tag = U8_MAX;
	u8 destination_tag = U8_MAX;

	mapped = ksys_mmap_pgoff(0, 2 * PAGE_SIZE,
				 PROT_READ | PROT_WRITE | PROT_MTE,
				 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(mapped));
	mte_expose_tagged_page(test, mapped);
	mte_expose_tagged_page(test, mapped + PAGE_SIZE);
	KUNIT_ASSERT_EQ(test, 0, orlix_mte_store_allocation_tag(current->mm,
								mapped + ORLIX_MTE_GRANULE_SIZE, 0x9));
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_pin_user_page_faulting(current->mm,
				mapped, ORLIX_TCTI_ACCESS_READ, &source));
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_pin_user_page_faulting(current->mm,
				mapped + PAGE_SIZE, ORLIX_TCTI_ACCESS_READ, &destination));
	copy_highpage(destination.page, source.page);
	orlix_tcti_unpin_user_page(&destination);
	orlix_tcti_unpin_user_page(&source);
	KUNIT_ASSERT_EQ(test, 0, orlix_mte_load_allocation_tag(current->mm,
				mapped + PAGE_SIZE + ORLIX_MTE_GRANULE_SIZE, &destination_tag));
	KUNIT_EXPECT_EQ(test, (u8)0x9, destination_tag);
	KUNIT_ASSERT_EQ(test, 0, orlix_mte_store_allocation_tag(current->mm,
				mapped + PAGE_SIZE + ORLIX_MTE_GRANULE_SIZE, 0x3));
	KUNIT_ASSERT_EQ(test, 0, orlix_mte_load_allocation_tag(current->mm,
				mapped + ORLIX_MTE_GRANULE_SIZE, &source_tag));
	KUNIT_EXPECT_EQ(test, (u8)0x9, source_tag);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, 2 * PAGE_SIZE));
}

static void mte_shared_mapping_aliases_observe_one_page_tag_state(struct kunit *test)
{
	unsigned long mapped;
	unsigned long alias;
	u8 tag = U8_MAX;

	mapped = mte_map_tagged_page(test, MAP_SHARED);
	KUNIT_ASSERT_EQ(test, 0, orlix_mte_store_allocation_tag(current->mm,
								mapped, 0x6));
	alias = sys_mremap(mapped, PAGE_SIZE, PAGE_SIZE,
			   MREMAP_MAYMOVE | MREMAP_DONTUNMAP, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(alias));
	mte_expose_tagged_page(test, alias);
	KUNIT_ASSERT_EQ(test, 0, orlix_mte_load_allocation_tag(current->mm, alias, &tag));
	KUNIT_EXPECT_EQ(test, (u8)0x6, tag);
	KUNIT_ASSERT_EQ(test, 0, orlix_mte_store_allocation_tag(current->mm, alias, 0x2));
	mte_expose_tagged_page(test, mapped);
	KUNIT_ASSERT_EQ(test, 0, orlix_mte_load_allocation_tag(current->mm, mapped, &tag));
	KUNIT_EXPECT_EQ(test, (u8)0x2, tag);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(alias, PAGE_SIZE));
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
}

static void mte_mapping_teardown_does_not_reuse_stale_pfn_tags(struct kunit *test)
{
	unsigned long mapped;
	unsigned long old_pfn;
	u8 tag = U8_MAX;

	mapped = mte_map_tagged_page(test, MAP_PRIVATE);
	old_pfn = mte_page_pfn(test, mapped);
	KUNIT_ASSERT_EQ(test, 0, orlix_mte_store_allocation_tag(current->mm,
								mapped, 0xe));
	KUNIT_ASSERT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
	mapped = mte_map_tagged_page(test, MAP_PRIVATE);
	KUNIT_EXPECT_EQ(test, old_pfn, mte_page_pfn(test, mapped));
	KUNIT_ASSERT_EQ(test, 0, orlix_mte_load_allocation_tag(current->mm, mapped, &tag));
	KUNIT_EXPECT_EQ(test, (u8)0, tag);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
}

static void mte_exclusive_and_lse_atomic_check_before_access(struct kunit *test)
{
	unsigned long mapped;
	unsigned long pfn;
	u64 generation;
	u64 mapping_generation;
	u32 observed = 0;
	u32 expected = 0x11223344;
	u32 desired = 0x55667788;
	u32 old = 0;
	bool exchanged = false;
	int ret;

	mapped = mte_map_tagged_page(test, MAP_PRIVATE);
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_write_user_data(current->mm, mapped,
							     &expected, sizeof(expected)));
	KUNIT_ASSERT_EQ(test, 0, orlix_mte_store_allocation_tag(current->mm, mapped, 5));
	KUNIT_ASSERT_EQ(test, 0L, set_tagged_addr_ctrl(current,
		PR_TAGGED_ADDR_ENABLE | PR_MTE_TCF_SYNC));

	ret = orlix_tcti_load_exclusive_user_data(current->mm,
		mapped | (5UL << MTE_TAG_SHIFT), &observed, sizeof(observed), &pfn,
		&generation, &mapping_generation);
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, expected, observed);

	ret = orlix_tcti_atomic_user_data(current->mm,
		mapped | (5UL << MTE_TAG_SHIFT), ORLIX_TCTI_ATOMIC_MEMORY_CAS,
		ORLIX_TCTI_ATOMIC_MEMORY_ACQ_REL, &expected, &desired, &old,
		sizeof(desired), &exchanged);
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_TRUE(test, exchanged);
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_read_user_data(current->mm, mapped,
								 &observed, sizeof(observed)));
	KUNIT_EXPECT_EQ(test, desired, observed);

	ret = orlix_tcti_load_exclusive_user_data(current->mm,
		mapped | (4UL << MTE_TAG_SHIFT), &observed, sizeof(observed), &pfn,
		&generation, &mapping_generation);
	KUNIT_EXPECT_EQ(test, -EHWPOISON, ret);
	ret = orlix_tcti_atomic_user_data(current->mm,
		mapped | (4UL << MTE_TAG_SHIFT), ORLIX_TCTI_ATOMIC_MEMORY_CAS,
		ORLIX_TCTI_ATOMIC_MEMORY_ACQ_REL, &desired, &expected, &old,
		sizeof(expected), &exchanged);
	KUNIT_EXPECT_EQ(test, -EHWPOISON, ret);
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_read_user_data(current->mm, mapped,
								 &observed, sizeof(observed)));
	KUNIT_EXPECT_EQ(test, desired, observed);
	KUNIT_EXPECT_EQ(test, 0L, set_tagged_addr_ctrl(current, 0));
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
}

struct mte_lifetime_race_state {
	struct page *page;
	struct completion start;
	atomic_t completed;
};

static int mte_lifetime_race_worker(void *data)
{
	struct mte_lifetime_race_state *state = data;
	unsigned int index;

	wait_for_completion(&state->start);
	for (index = 0; index < 1024; index++) {
		orlix_mte_clear_page_tags(state->page);
		cond_resched();
	}
	atomic_inc(&state->completed);
	return 0;
}

static void mte_tag_lifetime_and_allocator_hooks_are_atomic_safe(struct kunit *test)
{
	struct mte_lifetime_race_state state;
	struct task_struct *worker;
	unsigned int index;

	state.page = alloc_page(GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, state.page);
	init_completion(&state.start);
	atomic_set(&state.completed, 0);
	orlix_mte_sync_page_tags(state.page);
	worker = kthread_run(mte_lifetime_race_worker, &state, "orlix-mte-lifetime");
	KUNIT_ASSERT_FALSE(test, IS_ERR(worker));
	complete_all(&state.start);
	for (index = 0; index < 1024; index++) {
		/* arch_free_page() is invoked by allocator paths that may not sleep. */
		preempt_disable();
		arch_free_page(state.page, 0);
		preempt_enable();
		orlix_mte_sync_page_tags(state.page);
		cond_resched();
	}
	KUNIT_EXPECT_EQ(test, 0, kthread_stop(worker));
	KUNIT_EXPECT_EQ(test, 1, atomic_read(&state.completed));
	arch_free_page(state.page, 0);
	__free_page(state.page);
}

static struct kunit_case orlix_tcti_memory_tagging_source_bound_test_cases[] = {
	KUNIT_CASE(mte_source_cohort_is_exact),
	KUNIT_CASE(mte_all_legal_free_fields_decode),
	KUNIT_CASE(mte_fixed_bit_neighbours_are_not_the_leaf),
	KUNIT_CASE(mte_reserved_bulk_immediates_are_undefined),
	KUNIT_CASE(mte_register_semantics_and_non_el0),
	KUNIT_CASE(mte_memory_tags_data_writeback_and_faults),
	KUNIT_CASE(mte_tagged_user_addresses_use_linux_untagged_lookup),
	KUNIT_CASE(mte_ordinary_accesses_produce_sync_fault_signal_state),
	KUNIT_CASE(mte_prctl_tcf_dispositions_are_source_classified),
	KUNIT_CASE(mte_resume_cross_granule_accesses_do_not_commit),
	KUNIT_CASE(mte_resume_cross_granule_atomics_do_not_commit),
	KUNIT_CASE(mte_resume_cross_vma_boundaries_do_not_commit),
	KUNIT_CASE(mte_st2g_pair_contention_has_no_torn_pair),
	KUNIT_CASE(mte_stz2g_pair_contention_orders_zero_before_tags),
	KUNIT_CASE(mte_allocator_and_first_exposure_start_with_zero_tags),
	KUNIT_CASE(mte_madvise_discard_and_refault_clear_page_tags),
	KUNIT_CASE(mte_copy_highpage_copies_tags_then_keeps_pages_independent),
	KUNIT_CASE(mte_shared_mapping_aliases_observe_one_page_tag_state),
	KUNIT_CASE(mte_mapping_teardown_does_not_reuse_stale_pfn_tags),
	KUNIT_CASE(mte_exclusive_and_lse_atomic_check_before_access),
	KUNIT_CASE(mte_tag_lifetime_and_allocator_hooks_are_atomic_safe),
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
