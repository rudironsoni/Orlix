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

#include "../decode_aarch64.h"

#define MOVE_WIDE_SOURCE_MASK 0xff800000U
#define MOVE_WIDE_SVC 0xd4000001U

struct tcti_move_wide_leaf {
	u16 source_ordinal;
	const char *source_name;
	const char *operation_id;
	u32 source_pattern;
	enum tcti_move_wide_op operation;
	bool is_64bit;
};

/*
 * Exact direct leaves from the pinned Arm AARCHMRS 2026-06 manifest.
 * MOV is an assembler alias of MOVZ or ORR, and is not another direct leaf.
 */
static const struct tcti_move_wide_leaf tcti_move_wide_leaves[] = {
	{ 2199, "MOVN_32_movewide", "MOVN", 0x12800000U,
	  TCTI_MOVE_WIDE_MOVN, false },
	{ 2200, "MOVZ_32_movewide", "MOVZ", 0x52800000U,
	  TCTI_MOVE_WIDE_MOVZ, false },
	{ 2201, "MOVK_32_movewide", "MOVK", 0x72800000U,
	  TCTI_MOVE_WIDE_MOVK, false },
	{ 2202, "MOVN_64_movewide", "MOVN", 0x92800000U,
	  TCTI_MOVE_WIDE_MOVN, true },
	{ 2203, "MOVZ_64_movewide", "MOVZ", 0xd2800000U,
	  TCTI_MOVE_WIDE_MOVZ, true },
	{ 2204, "MOVK_64_movewide", "MOVK", 0xf2800000U,
	  TCTI_MOVE_WIDE_MOVK, true },
};

static u32 tcti_move_wide_instruction(const struct tcti_move_wide_leaf *leaf,
				      u8 hw, u16 immediate, u8 rd)
{
	return leaf->source_pattern | ((u32)hw << 21) |
		((u32)immediate << 5) | rd;
}

static const struct tcti_move_wide_leaf *
tcti_move_wide_source_leaf(u32 instruction)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(tcti_move_wide_leaves); index++) {
		const struct tcti_move_wide_leaf *leaf =
			&tcti_move_wide_leaves[index];

		if ((instruction & MOVE_WIDE_SOURCE_MASK) == leaf->source_pattern)
			return leaf;
	}
	return NULL;
}

static u64 tcti_move_wide_mask(const struct tcti_move_wide_leaf *leaf)
{
	return leaf->is_64bit ? U64_MAX : U32_MAX;
}

static u64 tcti_move_wide_expected(const struct tcti_move_wide_leaf *leaf,
					  u8 hw, u16 immediate, u64 old)
{
	u8 shift = hw * 16;
	u64 lane = (u64)immediate << shift;
	u64 lane_mask = 0xffffULL << shift;
	u64 result;

	switch (leaf->operation) {
	case TCTI_MOVE_WIDE_MOVN:
		result = ~lane;
		break;
	case TCTI_MOVE_WIDE_MOVZ:
		result = lane;
		break;
	case TCTI_MOVE_WIDE_MOVK:
		result = (old & ~lane_mask) | lane;
		break;
	default:
		return 0;
	}
	return result & tcti_move_wide_mask(leaf);
}

static void tcti_move_wide_expect_decode(struct kunit *test,
					 const struct tcti_move_wide_leaf *leaf,
					 u32 instruction, u8 hw, u16 immediate,
					 u8 rd)
{
	struct tcti_decoded_instruction decoded = tcti_decode_aarch64(instruction);

	KUNIT_ASSERT_EQ_MSG(test, TCTI_DECODE_MOVE_WIDE_IMMEDIATE,
			    decoded.decode_class, "%s %#x", leaf->source_name,
			    instruction);
	KUNIT_EXPECT_EQ(test, leaf->operation, decoded.move_wide_op);
	KUNIT_EXPECT_EQ(test, leaf->is_64bit, decoded.is_64bit);
	KUNIT_EXPECT_EQ(test, (u8)(hw * 16), decoded.halfword_shift);
	KUNIT_EXPECT_EQ(test, immediate, decoded.imm16);
	KUNIT_EXPECT_EQ(test, rd, decoded.rd);
}

static unsigned long tcti_move_wide_map_program(struct kunit *test,
						 u32 instruction)
{
	const u32 program[] = { instruction, MOVE_WIDE_SVC };
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
		KUNIT_FAIL(test, "could not write move-wide program: %d", ret);
		return 0;
	}
	ret = sys_mprotect(mapped, PAGE_SIZE, PROT_READ | PROT_EXEC);
	if (ret) {
		vm_munmap(mapped, PAGE_SIZE);
		KUNIT_FAIL(test, "could not protect move-wide program: %d", ret);
		return 0;
	}
	return mapped;
}

static void tcti_move_wide_execute(struct kunit *test,
				   const struct tcti_move_wide_leaf *leaf,
				   u8 hw, u16 immediate, u8 rd)
{
	u32 instruction = tcti_move_wide_instruction(leaf, hw, immediate, rd);
	unsigned long mapped = tcti_move_wide_map_program(test, instruction);
	unsigned int pass;

	KUNIT_ASSERT_NE(test, 0UL, mapped);
	for (pass = 0; pass < 2; pass++) {
		struct pt_regs regs = {};
		struct pt_regs before;
		struct tcti_result result;
		u64 expected;
		unsigned int reg;

		for (reg = 0; reg < 31; reg++)
			regs.regs[reg] = 0x9e3779b97f4a7c15ULL ^
				((u64)instruction << (reg & 15)) ^ reg;
		regs.sp = 0x00000001fffffff0ULL;
		regs.pc = mapped;
		regs.pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_C_BIT | PSR_D_BIT;
		regs.syscallno = NO_SYSCALL;
		before = regs;
		expected = tcti_move_wide_expected(
			leaf, hw, immediate, rd == 31 ? 0 : before.regs[rd]);

		result = tcti_resume_user(current, &regs, current->mm);
		KUNIT_ASSERT_EQ_MSG(test, TCTI_EXIT_SYSCALL, result.reason,
				    "%s %#x pass %u", leaf->source_name,
				    instruction, pass);
		KUNIT_EXPECT_EQ(test, 0L, result.status);
		KUNIT_EXPECT_EQ(test, mapped + sizeof(u32), result.pc);
		KUNIT_EXPECT_EQ(test, MOVE_WIDE_SVC, result.instruction);
		for (reg = 0; reg < 31; reg++)
			KUNIT_EXPECT_EQ_MSG(test,
				reg == rd ? expected : before.regs[reg], regs.regs[reg],
				"%s x%u pass %u", leaf->source_name, reg, pass);
		KUNIT_EXPECT_EQ(test, before.sp, regs.sp);
		KUNIT_EXPECT_EQ(test, before.pstate, regs.pstate);
		KUNIT_EXPECT_EQ(test, mapped + sizeof(u32), regs.pc);
	}
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
}

static void tcti_move_wide_source_bindings(struct kunit *test)
{
	size_t index;

	KUNIT_ASSERT_EQ(test, 6U, ARRAY_SIZE(tcti_move_wide_leaves));
	for (index = 0; index < ARRAY_SIZE(tcti_move_wide_leaves); index++) {
		const struct tcti_move_wide_leaf *leaf =
			&tcti_move_wide_leaves[index];
		u8 hw = leaf->is_64bit ? 3 : 1;
		u32 instruction = tcti_move_wide_instruction(leaf, hw, 0xa55a, 29);
		size_t previous;

		KUNIT_EXPECT_EQ(test, 2199U + index, leaf->source_ordinal);
		KUNIT_EXPECT_TRUE(test, leaf->source_name[0]);
		KUNIT_EXPECT_TRUE(test, leaf->operation_id[0]);
		KUNIT_EXPECT_EQ(test, leaf->source_pattern,
				leaf->source_pattern & MOVE_WIDE_SOURCE_MASK);
		KUNIT_EXPECT_PTR_EQ(test, leaf,
				    tcti_move_wide_source_leaf(instruction));
		tcti_move_wide_expect_decode(test, leaf, instruction, hw, 0xa55a,
					     29);
		for (previous = 0; previous < index; previous++)
			KUNIT_EXPECT_NE(test, leaf->source_pattern,
				tcti_move_wide_leaves[previous].source_pattern);
	}
}

static void tcti_move_wide_complete_legal_decode_matrix(struct kunit *test)
{
	static const u16 immediate_vectors[] = { 0x0000, 0x0001, 0x00ff,
		0x8000, 0xff00, 0xffff, 0xa55a };
	size_t index;

	for (index = 0; index < ARRAY_SIZE(tcti_move_wide_leaves); index++) {
		const struct tcti_move_wide_leaf *leaf =
			&tcti_move_wide_leaves[index];
		u8 hw;

		for (hw = 0; hw < (leaf->is_64bit ? 4 : 2); hw++) {
			size_t immediate_index;
			u8 rd;

			for (immediate_index = 0;
			     immediate_index < ARRAY_SIZE(immediate_vectors);
			     immediate_index++)
				for (rd = 0; rd < 32; rd++)
					tcti_move_wide_expect_decode(
						test, leaf,
						tcti_move_wide_instruction(
							leaf, hw,
							immediate_vectors[immediate_index], rd),
						hw, immediate_vectors[immediate_index], rd);
		}
	}
}

static void tcti_move_wide_all_immediates_decode(struct kunit *test)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(tcti_move_wide_leaves); index++) {
		const struct tcti_move_wide_leaf *leaf =
			&tcti_move_wide_leaves[index];
		u8 hw;

		for (hw = 0; hw < (leaf->is_64bit ? 4 : 2); hw++) {
			u32 immediate;

			for (immediate = 0; immediate <= U16_MAX; immediate++) {
				u8 rd;

				for (rd = 0; rd < 32; rd++) {
					struct tcti_decoded_instruction decoded =
						tcti_decode_aarch64(
							tcti_move_wide_instruction(
								leaf, hw, immediate, rd));

					if (decoded.decode_class !=
						    TCTI_DECODE_MOVE_WIDE_IMMEDIATE ||
					    decoded.move_wide_op != leaf->operation ||
					    decoded.is_64bit != leaf->is_64bit ||
					    decoded.halfword_shift != hw * 16 ||
					    decoded.imm16 != immediate ||
					    decoded.rd != rd) {
						KUNIT_FAIL(test,
							   "%s immediate %#x rd %u decoded incorrectly",
							   leaf->source_name, immediate, rd);
						return;
					}
				}
				if ((immediate & 0xfffU) == 0xfffU)
					cond_resched();
			}
		}
	}
}

static void tcti_move_wide_production_path_semantics(struct kunit *test)
{
	static const u16 immediate_vectors[] = { 0, 1, 0x8000, 0xffff, 0xa55a };
	size_t index;

	for (index = 0; index < ARRAY_SIZE(tcti_move_wide_leaves); index++) {
		const struct tcti_move_wide_leaf *leaf =
			&tcti_move_wide_leaves[index];
		u8 hw;

		for (hw = 0; hw < (leaf->is_64bit ? 4 : 2); hw++) {
			size_t immediate_index;

			for (immediate_index = 0;
			     immediate_index < ARRAY_SIZE(immediate_vectors);
			     immediate_index++) {
				tcti_move_wide_execute(test, leaf, hw,
						immediate_vectors[immediate_index], 9);
				tcti_move_wide_execute(test, leaf, hw,
						immediate_vectors[immediate_index], 31);
			}
		}
	}
}

static void tcti_move_wide_fixed_bit_neighbours_and_reserved(struct kunit *test)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(tcti_move_wide_leaves); index++) {
		const struct tcti_move_wide_leaf *leaf =
			&tcti_move_wide_leaves[index];
		u8 bit;

		for (bit = 0; bit < 32; bit++) {
			u32 instruction;
			const struct tcti_move_wide_leaf *neighbour;
			struct tcti_decoded_instruction decoded;

			if (!(MOVE_WIDE_SOURCE_MASK & BIT(bit)))
				continue;
			instruction = tcti_move_wide_instruction(leaf, 0, 1, 3) ^
				BIT(bit);
			neighbour = tcti_move_wide_source_leaf(instruction);
			decoded = tcti_decode_aarch64(instruction);
			if (neighbour) {
				KUNIT_EXPECT_EQ(test, TCTI_DECODE_MOVE_WIDE_IMMEDIATE,
						decoded.decode_class);
				KUNIT_EXPECT_EQ(test, neighbour->operation,
						decoded.move_wide_op);
				continue;
			}
			KUNIT_EXPECT_NE_MSG(test, TCTI_DECODE_MOVE_WIDE_IMMEDIATE,
					    decoded.decode_class,
					    "%s fixed bit %#x", leaf->source_name,
					    instruction);
		}
	}

	/* opc == 1 is reserved for both widths and all halfword selections. */
	for (index = 0; index < 2; index++) {
		u32 base = index ? 0x92800000U : 0x12800000U;
		u8 hw;

		for (hw = 0; hw < 4; hw++)
			KUNIT_EXPECT_EQ(test, TCTI_DECODE_UNSUPPORTED,
				tcti_decode_aarch64(base | BIT(29) |
						 ((u32)hw << 21)).decode_class);
	}

	/* 32-bit forms reserve halfword selections 2 and 3. */
	for (index = 0; index < 3; index++) {
		u8 hw;

		for (hw = 2; hw < 4; hw++)
			KUNIT_EXPECT_EQ(test, TCTI_DECODE_UNSUPPORTED,
				tcti_decode_aarch64(
					tcti_move_wide_instruction(
						&tcti_move_wide_leaves[index], hw, 0, 0))
					.decode_class);
	}
}

static struct kunit_case tcti_move_wide_source_bound_test_cases[] = {
	KUNIT_CASE(tcti_move_wide_source_bindings),
	KUNIT_CASE(tcti_move_wide_complete_legal_decode_matrix),
	KUNIT_CASE(tcti_move_wide_all_immediates_decode),
	KUNIT_CASE(tcti_move_wide_production_path_semantics),
	KUNIT_CASE(tcti_move_wide_fixed_bit_neighbours_and_reserved),
	{}
};

struct kunit_suite tcti_move_wide_source_bound_test_suite = {
	.name = "orlix-tcti-move-wide-source-bound",
	.test_cases = tcti_move_wide_source_bound_test_cases,
};

kunit_test_suite(tcti_move_wide_source_bound_test_suite);
