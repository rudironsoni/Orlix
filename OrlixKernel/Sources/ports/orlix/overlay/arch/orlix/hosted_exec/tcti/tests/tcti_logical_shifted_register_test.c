// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include <linux/bitops.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/sched/mm.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/syscalls.h>
#include <asm/processor.h>
#include <asm/ptrace.h>
#include <asm/tcti.h>

#include "../decode_aarch64.h"
#include "../gadget_program.h"
#include "tcti_test_suites.h"

#define LOGICAL_SHIFTED_REGISTER_SOURCE_MASK 0xff200000U
#define LOGICAL_SHIFTED_REGISTER_NZCV \
	(PSR_N_BIT | PSR_Z_BIT | PSR_C_BIT | PSR_V_BIT)

struct tcti_logical_shifted_register_leaf {
	u16 source_ordinal;
	const char *source_name;
	const char *operation_id;
	u32 source_pattern;
	enum tcti_logical_op operation;
	bool is_64bit;
	bool invert;
	bool set_flags;
};

/*
 * Exact direct leaves from the pinned Arm AARCHMRS 2026-06 source manifest.
 * MOV, MVN, and TST are operand aliases of these leaves, not extra direct
 * instruction-encoding leaves.
 */
static const struct tcti_logical_shifted_register_leaf
tcti_logical_shifted_register_leaves[] = {
	{ 3434, "AND_32_log_shift", "AND_log_shift", 0x0a000000U,
	  TCTI_LOGICAL_AND, false, false, false },
	{ 3435, "BIC_32_log_shift", "BIC_log_shift", 0x0a200000U,
	  TCTI_LOGICAL_AND, false, true, false },
	{ 3436, "ORR_32_log_shift", "ORR_log_shift", 0x2a000000U,
	  TCTI_LOGICAL_ORR, false, false, false },
	{ 3437, "ORN_32_log_shift", "ORN_log_shift", 0x2a200000U,
	  TCTI_LOGICAL_ORR, false, true, false },
	{ 3438, "EOR_32_log_shift", "EOR_log_shift", 0x4a000000U,
	  TCTI_LOGICAL_EOR, false, false, false },
	{ 3439, "EON_32_log_shift", "EON", 0x4a200000U,
	  TCTI_LOGICAL_EOR, false, true, false },
	{ 3440, "ANDS_32_log_shift", "ANDS_log_shift", 0x6a000000U,
	  TCTI_LOGICAL_AND, false, false, true },
	{ 3441, "BICS_32_log_shift", "BICS", 0x6a200000U,
	  TCTI_LOGICAL_AND, false, true, true },
	{ 3442, "AND_64_log_shift", "AND_log_shift", 0x8a000000U,
	  TCTI_LOGICAL_AND, true, false, false },
	{ 3443, "BIC_64_log_shift", "BIC_log_shift", 0x8a200000U,
	  TCTI_LOGICAL_AND, true, true, false },
	{ 3444, "ORR_64_log_shift", "ORR_log_shift", 0xaa000000U,
	  TCTI_LOGICAL_ORR, true, false, false },
	{ 3445, "ORN_64_log_shift", "ORN_log_shift", 0xaa200000U,
	  TCTI_LOGICAL_ORR, true, true, false },
	{ 3446, "EOR_64_log_shift", "EOR_log_shift", 0xca000000U,
	  TCTI_LOGICAL_EOR, true, false, false },
	{ 3447, "EON_64_log_shift", "EON", 0xca200000U,
	  TCTI_LOGICAL_EOR, true, true, false },
	{ 3448, "ANDS_64_log_shift", "ANDS_log_shift", 0xea000000U,
	  TCTI_LOGICAL_AND, true, false, true },
	{ 3449, "BICS_64_log_shift", "BICS", 0xea200000U,
	  TCTI_LOGICAL_AND, true, true, true },
};

static u32 tcti_logical_shifted_register_instruction(
	const struct tcti_logical_shifted_register_leaf *leaf, u8 shift,
	u8 rm, u8 amount, u8 rn, u8 rd)
{
	return leaf->source_pattern | ((u32)shift << 22) |
		((u32)rm << 16) | ((u32)amount << 10) |
		((u32)rn << 5) | rd;
}

static const struct tcti_logical_shifted_register_leaf *
tcti_logical_shifted_register_source_leaf(u32 instruction)
{
	size_t index;

	for (index = 0;
	     index < ARRAY_SIZE(tcti_logical_shifted_register_leaves);
	     index++) {
		const struct tcti_logical_shifted_register_leaf *leaf =
			&tcti_logical_shifted_register_leaves[index];

		if ((instruction & LOGICAL_SHIFTED_REGISTER_SOURCE_MASK) ==
		    leaf->source_pattern)
			return leaf;
	}
	return NULL;
}

static u64 tcti_logical_shifted_register_mask(bool is_64bit)
{
	return is_64bit ? U64_MAX : U32_MAX;
}

static u64 tcti_logical_shifted_register_shift(u64 value, bool is_64bit,
					       u8 shift, u8 amount)
{
	u8 width = is_64bit ? 64 : 32;
	u64 mask = tcti_logical_shifted_register_mask(is_64bit);

	value &= mask;
	switch (shift) {
	case 0:
		return (value << amount) & mask;
	case 1:
		return value >> amount;
	case 2:
		return (u64)(sign_extend64(value, width - 1) >> amount) & mask;
	case 3:
		if (!amount)
			return value;
		return ((value >> amount) | (value << (width - amount))) & mask;
	}
	return 0;
}

static u64 tcti_logical_shifted_register_result(
	const struct tcti_logical_shifted_register_leaf *leaf, u64 left,
	u64 right, u8 shift, u8 amount)
{
	u64 mask = tcti_logical_shifted_register_mask(leaf->is_64bit);

	left &= mask;
	right = tcti_logical_shifted_register_shift(right, leaf->is_64bit,
						    shift, amount);
	if (leaf->invert)
		right = ~right & mask;
	switch (leaf->operation) {
	case TCTI_LOGICAL_AND:
		return left & right;
	case TCTI_LOGICAL_ORR:
		return left | right;
	case TCTI_LOGICAL_EOR:
		return left ^ right;
	default:
		return 0;
	}
}

static void tcti_logical_shifted_register_expect_decode(
	struct kunit *test,
	const struct tcti_logical_shifted_register_leaf *leaf,
	u32 instruction, u8 shift, u8 amount, u8 rn, u8 rm, u8 rd)
{
	struct tcti_decoded_instruction decoded =
		tcti_decode_aarch64(instruction);

	KUNIT_ASSERT_EQ_MSG(test, TCTI_DECODE_LOGICAL_SHIFTED_REGISTER,
			    decoded.decode_class, "%s %#x", leaf->source_name,
			    instruction);
	KUNIT_EXPECT_EQ(test, leaf->operation, decoded.logical_op);
	KUNIT_EXPECT_EQ(test, leaf->is_64bit, decoded.is_64bit);
	KUNIT_EXPECT_EQ(test, leaf->invert, decoded.invert_second_operand);
	KUNIT_EXPECT_EQ(test, leaf->set_flags, decoded.set_flags);
	KUNIT_EXPECT_EQ(test, shift, decoded.shift);
	KUNIT_EXPECT_EQ(test, amount, decoded.shift_amount);
	KUNIT_EXPECT_EQ(test, rn, decoded.rn);
	KUNIT_EXPECT_EQ(test, rm, decoded.rm);
	KUNIT_EXPECT_EQ(test, rd, decoded.rd);
}

static void tcti_logical_shifted_register_execute(
	struct kunit *test,
	const struct tcti_logical_shifted_register_leaf *leaf,
	u8 shift, u8 amount, u8 rn, u8 rm, u8 rd)
{
	struct tcti_gadget_word
		program[TCTI_SINGLE_INSTRUCTION_PROGRAM_WORDS];
	struct tcti_decoded_instruction decoded;
	struct pt_regs regs = {};
	struct pt_regs before;
	u64 expected_registers[31];
	u64 expected_pstate;
	u64 left;
	u64 right;
	u64 result;
	u64 sign_bit = leaf->is_64bit ? BIT_ULL(63) : BIT_ULL(31);
	unsigned long fault_address = 0xdecafbadUL;
	u32 instruction = tcti_logical_shifted_register_instruction(
		leaf, shift, rm, amount, rn, rd);
	size_t word_count;
	unsigned int reg;
	int ret;

	for (reg = 0; reg < 31; reg++)
		regs.regs[reg] = 0x96a5c3e17b4d2f08ULL ^
			((u64)instruction << (reg & 7)) ^ reg;
	regs.sp = 0x706a865abcULL;
	regs.pc = 0x2468ace000ULL;
	regs.pstate = PSR_MODE_EL0t | LOGICAL_SHIFTED_REGISTER_NZCV |
		PSR_D_BIT;
	before = regs;
	memcpy(expected_registers, before.regs, sizeof(expected_registers));
	left = rn == 31 ? 0 : before.regs[rn];
	right = rm == 31 ? 0 : before.regs[rm];
	result = tcti_logical_shifted_register_result(
		leaf, left, right, shift, amount);
	if (rd < 31)
		expected_registers[rd] = result;
	expected_pstate = before.pstate;
	if (leaf->set_flags) {
		u64 flags = 0;

		if (result & sign_bit)
			flags |= PSR_N_BIT;
		if (!result)
			flags |= PSR_Z_BIT;
		expected_pstate =
			(before.pstate & ~LOGICAL_SHIFTED_REGISTER_NZCV) | flags;
	}

	decoded = tcti_decode_aarch64(instruction);
	KUNIT_ASSERT_EQ_MSG(test, TCTI_DECODE_LOGICAL_SHIFTED_REGISTER,
			    decoded.decode_class, "%s %#x", leaf->source_name,
			    instruction);
	ret = tcti_lower_decoded_instruction(&decoded, program,
					     ARRAY_SIZE(program), &word_count);
	KUNIT_ASSERT_EQ_MSG(test, 0, ret, "%s %#x", leaf->source_name,
			    instruction);
	ret = tcti_execute_gadget_program(NULL, &regs, program, word_count,
					  &fault_address);
	KUNIT_ASSERT_EQ_MSG(test, 0, ret, "%s %#x", leaf->source_name,
			    instruction);
	KUNIT_EXPECT_MEMEQ(test, expected_registers, regs.regs,
			   sizeof(expected_registers));
	KUNIT_EXPECT_EQ(test, before.sp, regs.sp);
	KUNIT_EXPECT_EQ(test, before.pc + sizeof(u32), regs.pc);
	KUNIT_EXPECT_EQ(test, expected_pstate, regs.pstate);
	KUNIT_EXPECT_EQ(test, 0xdecafbadUL, fault_address);
}

static void tcti_logical_shifted_register_source_bindings(struct kunit *test)
{
	size_t index;
	size_t previous;

	KUNIT_ASSERT_EQ(test, 16U,
			ARRAY_SIZE(tcti_logical_shifted_register_leaves));
	for (index = 0;
	     index < ARRAY_SIZE(tcti_logical_shifted_register_leaves);
	     index++) {
		const struct tcti_logical_shifted_register_leaf *leaf =
			&tcti_logical_shifted_register_leaves[index];
		u32 instruction = tcti_logical_shifted_register_instruction(
			leaf, 3, 7, leaf->is_64bit ? 63 : 31, 5, 3);

		KUNIT_EXPECT_EQ(test, 3434U + index, leaf->source_ordinal);
		KUNIT_EXPECT_TRUE(test, leaf->source_name[0]);
		KUNIT_EXPECT_TRUE(test, leaf->operation_id[0]);
		KUNIT_EXPECT_EQ(test, leaf->source_pattern,
				leaf->source_pattern &
				LOGICAL_SHIFTED_REGISTER_SOURCE_MASK);
		KUNIT_EXPECT_PTR_EQ(test, leaf,
			tcti_logical_shifted_register_source_leaf(instruction));
		tcti_logical_shifted_register_expect_decode(
			test, leaf, instruction, 3,
			leaf->is_64bit ? 63 : 31, 5, 7, 3);
		for (previous = 0; previous < index; previous++)
			KUNIT_EXPECT_NE(test, leaf->source_pattern,
				tcti_logical_shifted_register_leaves[previous]
					.source_pattern);
		if (index < 8) {
			const struct tcti_logical_shifted_register_leaf *wide =
				&tcti_logical_shifted_register_leaves[index + 8];

			KUNIT_EXPECT_STREQ(test, leaf->operation_id,
					   wide->operation_id);
			KUNIT_EXPECT_EQ(test, leaf->operation, wide->operation);
			KUNIT_EXPECT_EQ(test, leaf->invert, wide->invert);
			KUNIT_EXPECT_EQ(test, leaf->set_flags, wide->set_flags);
			KUNIT_EXPECT_EQ(test, leaf->source_pattern | BIT(31),
					wide->source_pattern);
		}
	}
}

static void tcti_logical_shifted_register_fixed_bit_neighbours(
	struct kunit *test)
{
	size_t index;

	for (index = 0;
	     index < ARRAY_SIZE(tcti_logical_shifted_register_leaves);
	     index++) {
		const struct tcti_logical_shifted_register_leaf *leaf =
			&tcti_logical_shifted_register_leaves[index];
		u8 bit;

		for (bit = 0; bit < 32; bit++) {
			const struct tcti_logical_shifted_register_leaf *neighbour;
			struct tcti_decoded_instruction decoded;
			u32 instruction;

			if (!(LOGICAL_SHIFTED_REGISTER_SOURCE_MASK & BIT(bit)))
				continue;
			instruction = tcti_logical_shifted_register_instruction(
				leaf, 0, 7, 1, 5, 3) ^ BIT(bit);
			neighbour =
				tcti_logical_shifted_register_source_leaf(instruction);
			decoded = tcti_decode_aarch64(instruction);
			if (neighbour) {
				KUNIT_EXPECT_EQ_MSG(test,
					TCTI_DECODE_LOGICAL_SHIFTED_REGISTER,
					decoded.decode_class,
					"%s fixed-bit neighbour %#x",
					leaf->source_name, instruction);
				KUNIT_EXPECT_EQ(test, neighbour->operation,
						decoded.logical_op);
				KUNIT_EXPECT_EQ(test, neighbour->is_64bit,
						decoded.is_64bit);
				KUNIT_EXPECT_EQ(test, neighbour->invert,
						decoded.invert_second_operand);
				KUNIT_EXPECT_EQ(test, neighbour->set_flags,
						decoded.set_flags);
				continue;
			}
			KUNIT_EXPECT_NE_MSG(test,
				TCTI_DECODE_LOGICAL_SHIFTED_REGISTER,
				decoded.decode_class,
				"%s retained class after fixed-bit mutation %#x",
				leaf->source_name, instruction);
		}
	}
}

static void tcti_logical_shifted_register_complete_field_matrix(
	struct kunit *test)
{
	size_t index;
	unsigned int legal = 0;
	unsigned int reserved = 0;

	for (index = 0;
	     index < ARRAY_SIZE(tcti_logical_shifted_register_leaves);
	     index++) {
		const struct tcti_logical_shifted_register_leaf *leaf =
			&tcti_logical_shifted_register_leaves[index];
		u8 shift;

		for (shift = 0; shift < 4; shift++) {
			u8 amount;

			for (amount = 0; amount < 64; amount++) {
				u8 rn = (amount + index) & 0x1fU;
				u8 rm = (31 - amount + shift) & 0x1fU;
				u8 rd = (amount + 3 * index + shift) & 0x1fU;
				u32 instruction =
					tcti_logical_shifted_register_instruction(
						leaf, shift, rm, amount, rn, rd);

				if (!leaf->is_64bit && amount >= 32) {
					struct tcti_decoded_instruction decoded =
						tcti_decode_aarch64(instruction);

					KUNIT_EXPECT_EQ_MSG(test,
						TCTI_DECODE_UNSUPPORTED,
						decoded.decode_class, "%s %#x",
						leaf->source_name, instruction);
					reserved++;
					continue;
				}
				tcti_logical_shifted_register_expect_decode(
					test, leaf, instruction, shift, amount,
					rn, rm, rd);
				tcti_logical_shifted_register_execute(
					test, leaf, shift, amount, rn, rm, rd);
				legal++;
			}
		}
	}
	KUNIT_EXPECT_EQ(test, 3072U, legal);
	KUNIT_EXPECT_EQ(test, 1024U, reserved);
}

static void tcti_logical_shifted_register_register_and_overlap_matrix(
	struct kunit *test)
{
	static const u8 overlaps[][3] = {
		{ 3, 5, 7 },
		{ 3, 5, 3 },
		{ 3, 5, 5 },
		{ 3, 3, 7 },
		{ 3, 3, 3 },
		{ 31, 5, 7 },
		{ 3, 31, 7 },
		{ 3, 5, 31 },
		{ 31, 31, 31 },
	};
	size_t leaf_index;

	for (leaf_index = 0;
	     leaf_index < ARRAY_SIZE(tcti_logical_shifted_register_leaves);
	     leaf_index++) {
		const struct tcti_logical_shifted_register_leaf *leaf =
			&tcti_logical_shifted_register_leaves[leaf_index];
		u8 reg;
		size_t overlap;

		for (reg = 0; reg < 32; reg++) {
			tcti_logical_shifted_register_execute(
				test, leaf, 0, 0, reg, 11, 13);
			tcti_logical_shifted_register_execute(
				test, leaf, 1, 1, 11, reg, 13);
			tcti_logical_shifted_register_execute(
				test, leaf, 2,
				leaf->is_64bit ? 63 : 31, 11, 13, reg);
		}
		for (overlap = 0; overlap < ARRAY_SIZE(overlaps); overlap++)
			tcti_logical_shifted_register_execute(
				test, leaf, overlap & 3,
				overlap % (leaf->is_64bit ? 64 : 32),
				overlaps[overlap][0], overlaps[overlap][1],
				overlaps[overlap][2]);
	}
}

static void tcti_logical_shifted_register_aliases(struct kunit *test)
{
	size_t width;

	for (width = 0; width < 2; width++) {
		const struct tcti_logical_shifted_register_leaf *orr =
			&tcti_logical_shifted_register_leaves[width ? 10 : 2];
		const struct tcti_logical_shifted_register_leaf *orn =
			&tcti_logical_shifted_register_leaves[width ? 11 : 3];
		const struct tcti_logical_shifted_register_leaf *ands =
			&tcti_logical_shifted_register_leaves[width ? 14 : 6];

		/* MOV, MVN, and TST aliases respectively. */
		tcti_logical_shifted_register_execute(test, orr, 0, 0, 31, 5, 3);
		tcti_logical_shifted_register_execute(test, orn, 0, 0, 31, 5, 3);
		tcti_logical_shifted_register_execute(test, ands, 0, 0, 5, 7, 31);
	}
}

static unsigned long tcti_logical_shifted_register_map_instructions(
	struct kunit *test, const u32 *instructions, size_t count)
{
	unsigned long mapped;
	int ret;

	KUNIT_ASSERT_LE(test, count * sizeof(*instructions), PAGE_SIZE);
	mapped = ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
				 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(mapped));
	ret = tcti_write_user_data(current->mm, mapped, instructions,
				   count * sizeof(*instructions));
	KUNIT_ASSERT_EQ(test, 0, ret);
	return mapped;
}

static void tcti_logical_shifted_register_reserved_structured_exits(
	struct kunit *test)
{
	const size_t reserved_count = 8U * 4U * 32U;
	u32 *instructions;
	unsigned long mapped;
	size_t instruction_index = 0;
	size_t leaf_index;

	instructions = kcalloc(reserved_count, sizeof(*instructions),
			       GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, instructions);
	for (leaf_index = 0; leaf_index < 8; leaf_index++) {
		const struct tcti_logical_shifted_register_leaf *leaf =
			&tcti_logical_shifted_register_leaves[leaf_index];
		u8 shift;

		for (shift = 0; shift < 4; shift++) {
			u8 amount;

			for (amount = 32; amount < 64; amount++)
				instructions[instruction_index++] =
					tcti_logical_shifted_register_instruction(
						leaf, shift, 7, amount, 5, 3);
		}
	}
	KUNIT_ASSERT_EQ(test, reserved_count, instruction_index);
	mapped = tcti_logical_shifted_register_map_instructions(
		test, instructions, reserved_count);

	for (instruction_index = 0;
	     instruction_index < reserved_count; instruction_index++) {
		struct tcti_result result;
		struct pt_regs regs = {};
		struct pt_regs before;
		unsigned long pc = mapped +
			instruction_index * sizeof(*instructions);
		u32 instruction = instructions[instruction_index];

		regs.regs[3] = 0xaaaaaaaa55555555ULL;
		regs.regs[5] = 0x0123456789abcdefULL;
		regs.regs[7] = 0xfedcba9876543210ULL;
		regs.pc = pc;
		regs.sp = STACK_TOP - 16;
		regs.pstate = PSR_MODE_EL0t | LOGICAL_SHIFTED_REGISTER_NZCV;
		regs.syscallno = NO_SYSCALL;
		before = regs;

		result = tcti_resume_user(current, &regs, current->mm);
		KUNIT_EXPECT_EQ_MSG(test, TCTI_EXIT_UNSUPPORTED_INSTRUCTION,
				    result.reason, "reserved %#x", instruction);
		KUNIT_EXPECT_EQ(test, -EOPNOTSUPP, result.status);
		KUNIT_EXPECT_EQ(test, pc, result.pc);
		KUNIT_EXPECT_EQ(test, instruction, result.instruction);
		KUNIT_EXPECT_MEMEQ(test, before.regs, regs.regs,
				   sizeof(before.regs));
		KUNIT_EXPECT_EQ(test, before.sp, regs.sp);
		KUNIT_EXPECT_EQ(test, before.pc, regs.pc);
		KUNIT_EXPECT_EQ(test, before.pstate, regs.pstate);
	}
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
	kfree(instructions);
}

static struct kunit_case tcti_logical_shifted_register_test_cases[] = {
	KUNIT_CASE(tcti_logical_shifted_register_source_bindings),
	KUNIT_CASE(tcti_logical_shifted_register_fixed_bit_neighbours),
	KUNIT_CASE(tcti_logical_shifted_register_complete_field_matrix),
	KUNIT_CASE(tcti_logical_shifted_register_register_and_overlap_matrix),
	KUNIT_CASE(tcti_logical_shifted_register_aliases),
	KUNIT_CASE(tcti_logical_shifted_register_reserved_structured_exits),
	{}
};

struct kunit_suite tcti_logical_shifted_register_test_suite = {
	.name = "orlix-tcti-logical-shifted-register",
	.test_cases = tcti_logical_shifted_register_test_cases,
};

kunit_test_suite(tcti_logical_shifted_register_test_suite);
