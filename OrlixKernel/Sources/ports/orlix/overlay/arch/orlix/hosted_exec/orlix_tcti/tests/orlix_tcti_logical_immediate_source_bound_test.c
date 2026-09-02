// SPDX-License-Identifier: GPL-2.0-only
#include <asm/processor.h>
#include <asm/ptrace.h>
#include <asm/orlix_tcti.h>
#include <kunit/test.h>
#include <linux/bitops.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/sched/mm.h>
#include <linux/mman.h>
#include <linux/sched.h>
#include <linux/syscalls.h>

#include "../block_cache.h"
#include "../decode_aarch64.h"
#include "orlix_tcti_test_suites.h"

#define LOGICAL_IMMEDIATE_SOURCE_MASK 0xff800000U
#define LOGICAL_IMMEDIATE_NZCV (PSR_N_BIT | PSR_Z_BIT | PSR_C_BIT | PSR_V_BIT)
#define LOGICAL_IMMEDIATE_SVC 0xd4000001U

struct orlix_tcti_logical_immediate_leaf {
	u16 source_ordinal;
	const char *source_name;
	const char *operation_id;
	u32 source_pattern;
	enum orlix_tcti_logical_op operation;
	bool is_64bit;
	bool set_flags;
};

/*
 * Exact direct leaves from the pinned Arm AARCHMRS 2026-06 source manifest.
 * MOV (bitmask immediate) and TST are operand aliases, not direct leaves.
 */
static const struct orlix_tcti_logical_immediate_leaf
    orlix_tcti_logical_immediate_leaves[] = {
	{2191, "AND_32_log_imm", "AND_log_imm", 0x12000000U, ORLIX_TCTI_LOGICAL_AND,
	 false, false},
	{2192, "ORR_32_log_imm", "ORR_log_imm", 0x32000000U, ORLIX_TCTI_LOGICAL_ORR,
	 false, false},
	{2193, "EOR_32_log_imm", "EOR_log_imm", 0x52000000U, ORLIX_TCTI_LOGICAL_EOR,
	 false, false},
	{2194, "ANDS_32S_log_imm", "ANDS_log_imm", 0x72000000U,
	 ORLIX_TCTI_LOGICAL_AND, false, true},
	{2195, "AND_64_log_imm", "AND_log_imm", 0x92000000U, ORLIX_TCTI_LOGICAL_AND,
	 true, false},
	{2196, "ORR_64_log_imm", "ORR_log_imm", 0xb2000000U, ORLIX_TCTI_LOGICAL_ORR,
	 true, false},
	{2197, "EOR_64_log_imm", "EOR_log_imm", 0xd2000000U, ORLIX_TCTI_LOGICAL_EOR,
	 true, false},
	{2198, "ANDS_64S_log_imm", "ANDS_log_imm", 0xf2000000U,
	 ORLIX_TCTI_LOGICAL_AND, true, true},
};

static u32 orlix_tcti_logical_immediate_instruction(
    const struct orlix_tcti_logical_immediate_leaf *leaf, bool n, u8 immr, u8 imms,
    u8 rn, u8 rd)
{
	return leaf->source_pattern | (n ? BIT(22) : 0) | ((u32)immr << 16) |
	       ((u32)imms << 10) | ((u32)rn << 5) | rd;
}

static const struct orlix_tcti_logical_immediate_leaf *
orlix_tcti_logical_immediate_source_leaf(u32 instruction)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_logical_immediate_leaves);
	     index++) {
		const struct orlix_tcti_logical_immediate_leaf *leaf =
		    &orlix_tcti_logical_immediate_leaves[index];

		if ((instruction & LOGICAL_IMMEDIATE_SOURCE_MASK) ==
		    leaf->source_pattern)
			return leaf;
	}
	return NULL;
}

static u64 orlix_tcti_logical_immediate_ror(u64 value, u8 rotate, u8 width)
{
	u64 mask = width == 64 ? U64_MAX : BIT_ULL(width) - 1;

	rotate %= width;
	value &= mask;
	if (!rotate)
		return value;
	return ((value >> rotate) | (value << (width - rotate))) & mask;
}

/* Independent test oracle for DecodeBitMasks' logical-immediate subset. */
static bool orlix_tcti_logical_immediate_mask(bool is_64bit, bool n, u8 immr, u8 imms,
					u64 *out)
{
	u8 concatenated = (n ? BIT(6) : 0) | ((~imms) & 0x3fU);
	u8 register_width = is_64bit ? 64 : 32;
	u8 length;
	u8 levels;
	u8 element_width;
	u8 offset;
	u64 element;
	u64 expanded = 0;

	if (!concatenated)
		return false;
	length = fls(concatenated) - 1;
	if (!length || (!is_64bit && length == 6))
		return false;
	levels = BIT(length) - 1;
	if ((imms & levels) == levels)
		return false;
	element_width = BIT(length);
	element = BIT_ULL((imms & levels) + 1) - 1;
	element =
	    orlix_tcti_logical_immediate_ror(element, immr & levels, element_width);
	for (offset = 0; offset < register_width; offset += element_width)
		expanded |= element << offset;
	*out = expanded;
	return true;
}

static u64
orlix_tcti_logical_immediate_result(const struct orlix_tcti_logical_immediate_leaf *leaf,
			      u64 left, u64 immediate)
{
	u64 mask = leaf->is_64bit ? U64_MAX : U32_MAX;

	left &= mask;
	immediate &= mask;
	switch (leaf->operation) {
	case ORLIX_TCTI_LOGICAL_AND:
		return left & immediate;
	case ORLIX_TCTI_LOGICAL_ORR:
		return left | immediate;
	case ORLIX_TCTI_LOGICAL_EOR:
		return left ^ immediate;
	default:
		return 0;
	}
}

static void orlix_tcti_logical_immediate_expect_decode(
    struct kunit *test, const struct orlix_tcti_logical_immediate_leaf *leaf,
    u32 instruction, u64 immediate, u8 rn, u8 rd)
{
	struct orlix_tcti_decoded_instruction decoded =
	    orlix_tcti_decode_aarch64(instruction);

	KUNIT_ASSERT_EQ_MSG(test, ORLIX_TCTI_DECODE_LOGICAL_IMMEDIATE,
			    decoded.decode_class, "%s %#x", leaf->source_name,
			    instruction);
	KUNIT_EXPECT_EQ(test, leaf->operation, decoded.logical_op);
	KUNIT_EXPECT_EQ(test, leaf->is_64bit, decoded.is_64bit);
	KUNIT_EXPECT_EQ(test, leaf->set_flags, decoded.set_flags);
	KUNIT_EXPECT_EQ(test, immediate, decoded.logical_immediate);
	KUNIT_EXPECT_EQ(test, rn, decoded.rn);
	KUNIT_EXPECT_EQ(test, rd, decoded.rd);
}

static unsigned long orlix_tcti_logical_immediate_map_program(struct kunit *test,
							u32 instruction)
{
	const u32 program[] = {instruction, LOGICAL_IMMEDIATE_SVC};
	unsigned long mapped;
	int ret;

	KUNIT_ASSERT_NOT_NULL(test, current->mm);
	mapped = ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
				 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(mapped));
	ret =
	    orlix_tcti_write_user_data(current->mm, mapped, program, sizeof(program));
	if (ret) {
		vm_munmap(mapped, PAGE_SIZE);
		KUNIT_FAIL(
		    test, "could not write logical immediate program: %d", ret);
		return 0;
	}
	ret = sys_mprotect(mapped, PAGE_SIZE, PROT_READ | PROT_EXEC);
	if (ret) {
		vm_munmap(mapped, PAGE_SIZE);
		KUNIT_FAIL(test,
			   "could not protect logical immediate program: %d",
			   ret);
		return 0;
	}
	return mapped;
}

static void orlix_tcti_logical_immediate_execute(
    struct kunit *test, const struct orlix_tcti_logical_immediate_leaf *leaf, bool n,
    u8 immr, u8 imms, u8 rn, u8 rd, u64 source)
{
	u32 instruction =
	    orlix_tcti_logical_immediate_instruction(leaf, n, immr, imms, rn, rd);
	unsigned long mapped =
	    orlix_tcti_logical_immediate_map_program(test, instruction);
	u64 immediate;
	unsigned int pass;

	KUNIT_ASSERT_NE(test, 0UL, mapped);
	KUNIT_ASSERT_TRUE(test, orlix_tcti_logical_immediate_mask(
				    leaf->is_64bit, n, immr, imms, &immediate));
	for (pass = 0; pass < 2; pass++) {
		struct orlix_tcti_block *block;
		struct orlix_tcti_result result;
		struct pt_regs regs = {};
		struct pt_regs before;
		u64 expected_registers[31];
		u64 expected;
		unsigned long expected_pstate;
		unsigned int reg;

		for (reg = 0; reg < 31; reg++)
			regs.regs[reg] = 0x96a5c3e17b4d2f08ULL ^
					 ((u64)instruction << (reg & 7)) ^ reg;
		if (rn < 31)
			regs.regs[rn] = source;
		regs.sp = 0x706a865abcULL;
		regs.pc = mapped;
		regs.pstate =
		    PSR_MODE_EL0t | LOGICAL_IMMEDIATE_NZCV | PSR_D_BIT;
		regs.syscallno = NO_SYSCALL;
		before = regs;
		memcpy(expected_registers, before.regs,
		       sizeof(expected_registers));
		expected = orlix_tcti_logical_immediate_result(
		    leaf, rn == 31 ? 0 : before.regs[rn], immediate);
		if (rd < 31)
			expected_registers[rd] = expected;
		expected_pstate = before.pstate;
		if (leaf->set_flags) {
			u64 sign_bit =
			    leaf->is_64bit ? BIT_ULL(63) : BIT_ULL(31);
			unsigned long flags = 0;

			if (expected & sign_bit)
				flags |= PSR_N_BIT;
			if (!expected)
				flags |= PSR_Z_BIT;
			expected_pstate =
			    (before.pstate & ~LOGICAL_IMMEDIATE_NZCV) | flags;
		}

		result = orlix_tcti_resume_user(current, &regs, current->mm);
		KUNIT_ASSERT_EQ_MSG(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason,
				    "%s %#x pass %u", leaf->source_name,
				    instruction, pass);
		KUNIT_EXPECT_EQ(test, 0L, result.status);
		KUNIT_EXPECT_EQ(test, mapped + sizeof(u32), result.pc);
		KUNIT_EXPECT_EQ(test, LOGICAL_IMMEDIATE_SVC,
				result.instruction);
		if (!pass) {
			block = orlix_tcti_block_cache_lookup(
			    current->mm, mapped,
			    orlix_tcti_code_generation(current->mm));
			KUNIT_EXPECT_NOT_NULL(test, block);
			if (block)
				orlix_tcti_block_put(block);
		}
		KUNIT_EXPECT_MEMEQ(test, expected_registers, regs.regs,
				   sizeof(expected_registers));
		KUNIT_EXPECT_EQ(test, before.sp, regs.sp);
		KUNIT_EXPECT_EQ(test, expected_pstate, regs.pstate);
		KUNIT_EXPECT_EQ(test, mapped + sizeof(u32), regs.pc);
	}
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
}

static void orlix_tcti_logical_immediate_source_bindings(struct kunit *test)
{
	size_t index;
	size_t previous;

	KUNIT_ASSERT_EQ(test, 8U, ARRAY_SIZE(orlix_tcti_logical_immediate_leaves));
	for (index = 0; index < ARRAY_SIZE(orlix_tcti_logical_immediate_leaves);
	     index++) {
		const struct orlix_tcti_logical_immediate_leaf *leaf =
		    &orlix_tcti_logical_immediate_leaves[index];
		u32 instruction = orlix_tcti_logical_immediate_instruction(
		    leaf, leaf->is_64bit, 7, 12, 5, 3);
		u64 immediate;

		KUNIT_EXPECT_EQ(test, 2191U + index, leaf->source_ordinal);
		KUNIT_EXPECT_TRUE(test, leaf->source_name[0]);
		KUNIT_EXPECT_TRUE(test, leaf->operation_id[0]);
		KUNIT_EXPECT_EQ(test, leaf->source_pattern,
				leaf->source_pattern &
				    LOGICAL_IMMEDIATE_SOURCE_MASK);
		KUNIT_EXPECT_PTR_EQ(
		    test, leaf,
		    orlix_tcti_logical_immediate_source_leaf(instruction));
		KUNIT_ASSERT_TRUE(test, orlix_tcti_logical_immediate_mask(
					    leaf->is_64bit, leaf->is_64bit, 7,
					    12, &immediate));
		orlix_tcti_logical_immediate_expect_decode(test, leaf, instruction,
						     immediate, 5, 3);
		for (previous = 0; previous < index; previous++)
			KUNIT_EXPECT_NE(test, leaf->source_pattern,
					orlix_tcti_logical_immediate_leaves[previous]
					    .source_pattern);
		if (index < 4) {
			const struct orlix_tcti_logical_immediate_leaf *wide =
			    &orlix_tcti_logical_immediate_leaves[index + 4];

			KUNIT_EXPECT_STREQ(test, leaf->operation_id,
					   wide->operation_id);
			KUNIT_EXPECT_EQ(test, leaf->operation, wide->operation);
			KUNIT_EXPECT_EQ(test, leaf->set_flags, wide->set_flags);
			KUNIT_EXPECT_EQ(test, leaf->source_pattern | BIT(31),
					wide->source_pattern);
		}
	}
}

static void orlix_tcti_logical_immediate_fixed_bit_neighbours(struct kunit *test)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_logical_immediate_leaves);
	     index++) {
		const struct orlix_tcti_logical_immediate_leaf *leaf =
		    &orlix_tcti_logical_immediate_leaves[index];
		u8 bit;

		for (bit = 0; bit < 32; bit++) {
			const struct orlix_tcti_logical_immediate_leaf *neighbour;
			struct orlix_tcti_decoded_instruction decoded;
			u32 instruction;

			if (!(LOGICAL_IMMEDIATE_SOURCE_MASK & BIT(bit)))
				continue;
			instruction = orlix_tcti_logical_immediate_instruction(
					  leaf, leaf->is_64bit, 7, 12, 5, 3) ^
				      BIT(bit);
			neighbour =
			    orlix_tcti_logical_immediate_source_leaf(instruction);
			decoded = orlix_tcti_decode_aarch64(instruction);
			if (neighbour) {
				u64 immediate;
				bool n = instruction & BIT(22);
				u8 immr = (instruction >> 16) & 0x3fU;
				u8 imms = (instruction >> 10) & 0x3fU;

				/*
				 * Source-mask neighbours can still be reserved:
				 * 32-bit AND/ORR/EOR/ANDS require N=0.
				 */
				if (!orlix_tcti_logical_immediate_mask(
					    neighbour->is_64bit, n, immr, imms,
					    &immediate)) {
					KUNIT_EXPECT_NE_MSG(
					    test,
					    ORLIX_TCTI_DECODE_LOGICAL_IMMEDIATE,
					    decoded.decode_class,
					    "%s reserved neighbour %#x",
					    leaf->source_name, instruction);
					continue;
				}
				KUNIT_EXPECT_EQ_MSG(
				    test, ORLIX_TCTI_DECODE_LOGICAL_IMMEDIATE,
				    decoded.decode_class, "%s neighbour %#x",
				    leaf->source_name, instruction);
				KUNIT_EXPECT_EQ(test, neighbour->operation,
						decoded.logical_op);
				KUNIT_EXPECT_EQ(test, neighbour->is_64bit,
						decoded.is_64bit);
				KUNIT_EXPECT_EQ(test, neighbour->set_flags,
						decoded.set_flags);
				continue;
			}
			KUNIT_EXPECT_NE_MSG(
			    test, ORLIX_TCTI_DECODE_LOGICAL_IMMEDIATE,
			    decoded.decode_class,
			    "%s retained class after fixed-bit mutation %#x",
			    leaf->source_name, instruction);
		}
	}
}

static void orlix_tcti_logical_immediate_complete_decode_matrix(struct kunit *test)
{
	size_t leaf_index;
	unsigned int legal = 0;
	unsigned int reserved = 0;

	for (leaf_index = 0;
	     leaf_index < ARRAY_SIZE(orlix_tcti_logical_immediate_leaves);
	     leaf_index++) {
		const struct orlix_tcti_logical_immediate_leaf *leaf =
		    &orlix_tcti_logical_immediate_leaves[leaf_index];
		unsigned int n;

		for (n = 0; n < 2; n++) {
			u8 immr;

			for (immr = 0; immr < 64; immr++) {
				u8 imms;

				for (imms = 0; imms < 64; imms++) {
					u8 rn = (immr + leaf_index) & 0x1fU;
					u8 rd = (imms + 3 * leaf_index) & 0x1fU;
					u32 instruction =
					    orlix_tcti_logical_immediate_instruction(
						leaf, !!n, immr, imms, rn, rd);
					u64 immediate;

					if (!orlix_tcti_logical_immediate_mask(
						leaf->is_64bit, !!n, immr, imms,
						&immediate)) {
						struct orlix_tcti_decoded_instruction
						    decoded =
							orlix_tcti_decode_aarch64(
							    instruction);

						KUNIT_EXPECT_EQ_MSG(
						    test,
						    ORLIX_TCTI_DECODE_UNSUPPORTED,
						    decoded.decode_class,
						    "%s %#x", leaf->source_name,
						    instruction);
						reserved++;
						continue;
					}
					orlix_tcti_logical_immediate_expect_decode(
					    test, leaf, instruction, immediate,
					    rn, rd);
					legal++;
				}
				cond_resched();
			}
		}
	}
	KUNIT_EXPECT_GT(test, legal, 0U);
	KUNIT_EXPECT_GT(test, reserved, 0U);
}

static void orlix_tcti_logical_immediate_production_path_semantics(struct kunit *test)
{
	static const struct {
		bool n;
		u8 immr;
		u8 imms;
		u64 source;
	} vectors[] = {
	    {false, 0, 0, 0},
	    {false, 1, 12, U64_MAX},
	    {false, 7, 24, 0x8000000080000000ULL},
	    {false, 31, 30, 0x123456789abcdef0ULL},
	    {true, 0, 0, U64_MAX},
	    {true, 63, 62, 0x0123456789abcdefULL},
	};
	size_t leaf_index;
	size_t vector_index;

	for (leaf_index = 0;
	     leaf_index < ARRAY_SIZE(orlix_tcti_logical_immediate_leaves);
	     leaf_index++) {
		const struct orlix_tcti_logical_immediate_leaf *leaf =
		    &orlix_tcti_logical_immediate_leaves[leaf_index];

		for (vector_index = 0; vector_index < ARRAY_SIZE(vectors);
		     vector_index++) {
			const typeof(vectors[0]) *vector =
			    &vectors[vector_index];

			if (!orlix_tcti_logical_immediate_mask(
				leaf->is_64bit, vector->n, vector->immr,
				vector->imms, &(u64){}))
				continue;
			orlix_tcti_logical_immediate_execute(
			    test, leaf, vector->n, vector->immr, vector->imms,
			    5, 3, vector->source);
		}
	}
}

static void
orlix_tcti_logical_immediate_aliases_and_special_registers(struct kunit *test)
{
	size_t width;

	for (width = 0; width < 2; width++) {
		const struct orlix_tcti_logical_immediate_leaf *orr =
		    &orlix_tcti_logical_immediate_leaves[width ? 5 : 1];
		const struct orlix_tcti_logical_immediate_leaf *ands =
		    &orlix_tcti_logical_immediate_leaves[width ? 7 : 3];
		const struct orlix_tcti_logical_immediate_leaf *eor =
		    &orlix_tcti_logical_immediate_leaves[width ? 6 : 2];

		/* MOV (bitmask immediate), TST, and zero-register
		 * source/destination. */
		orlix_tcti_logical_immediate_execute(test, orr, false, 1, 12, 31, 3,
					       U64_MAX);
		orlix_tcti_logical_immediate_execute(test, ands, false, 7, 24, 5, 31,
					       0x123456789abcdef0ULL);
		orlix_tcti_logical_immediate_execute(test, eor, false, 31, 30, 31, 31,
					       0);
	}
}

static void orlix_tcti_logical_immediate_reserved_structured_exits(struct kunit *test)
{
	static const struct {
		bool n;
		u8 immr;
		u8 imms;
	} reserved[] = {
	    {false, 0, 63},
	    {false, 37, 31},
	    {true, 19, 63},
	};
	size_t leaf_index;
	size_t reserved_index;

	for (leaf_index = 0;
	     leaf_index < ARRAY_SIZE(orlix_tcti_logical_immediate_leaves);
	     leaf_index++) {
		const struct orlix_tcti_logical_immediate_leaf *leaf =
		    &orlix_tcti_logical_immediate_leaves[leaf_index];

		for (reserved_index = 0; reserved_index < ARRAY_SIZE(reserved);
		     reserved_index++) {
			const typeof(reserved[0]) *invalid =
			    &reserved[reserved_index];
			struct orlix_tcti_result result;
			struct pt_regs regs = {};
			struct pt_regs before;
			u32 instruction = orlix_tcti_logical_immediate_instruction(
			    leaf, invalid->n, invalid->immr, invalid->imms, 5,
			    3);
			unsigned long mapped;

			if (orlix_tcti_logical_immediate_mask(
				leaf->is_64bit, invalid->n, invalid->immr,
				invalid->imms, &(u64){}))
				continue;
			mapped = orlix_tcti_logical_immediate_map_program(
			    test, instruction);
			KUNIT_ASSERT_NE(test, 0UL, mapped);
			regs.regs[3] = 0xaaaaaaaa55555555ULL;
			regs.regs[5] = 0x0123456789abcdefULL;
			regs.sp = STACK_TOP - 16;
			regs.pc = mapped;
			regs.pstate = PSR_MODE_EL0t | LOGICAL_IMMEDIATE_NZCV;
			regs.syscallno = NO_SYSCALL;
			before = regs;

			result = orlix_tcti_resume_user(current, &regs, current->mm);
			KUNIT_EXPECT_EQ_MSG(test,
					    ORLIX_TCTI_EXIT_UNSUPPORTED_INSTRUCTION,
					    result.reason, "%s reserved %#x",
					    leaf->source_name, instruction);
			KUNIT_EXPECT_EQ(test, -EOPNOTSUPP, result.status);
			KUNIT_EXPECT_EQ(test, mapped, result.pc);
			KUNIT_EXPECT_EQ(test, instruction, result.instruction);
			KUNIT_EXPECT_MEMEQ(test, before.regs, regs.regs,
					   sizeof(before.regs));
			KUNIT_EXPECT_EQ(test, before.sp, regs.sp);
			KUNIT_EXPECT_EQ(test, before.pc, regs.pc);
			KUNIT_EXPECT_EQ(test, before.pstate, regs.pstate);
			KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
		}
	}
}

static struct kunit_case orlix_tcti_logical_immediate_source_bound_test_cases[] = {
    KUNIT_CASE(orlix_tcti_logical_immediate_source_bindings),
    KUNIT_CASE(orlix_tcti_logical_immediate_fixed_bit_neighbours),
    KUNIT_CASE(orlix_tcti_logical_immediate_complete_decode_matrix),
    KUNIT_CASE(orlix_tcti_logical_immediate_production_path_semantics),
    KUNIT_CASE(orlix_tcti_logical_immediate_aliases_and_special_registers),
    KUNIT_CASE(orlix_tcti_logical_immediate_reserved_structured_exits),
    {}};

static int orlix_tcti_logical_immediate_source_bound_test_init(struct kunit *test)
{
	struct mm_struct *mm = mm_alloc();

	if (!mm)
		return -ENOMEM;
	kthread_use_mm(mm);
	test->priv = mm;
	return 0;
}

static void orlix_tcti_logical_immediate_source_bound_test_exit(struct kunit *test)
{
	struct mm_struct *mm = test->priv;

	if (!mm)
		return;
	kthread_unuse_mm(mm);
	mmput(mm);
}

struct kunit_suite orlix_tcti_logical_immediate_source_bound_test_suite = {
    .name = "orlix-tcti-logical-immediate-source-bound",
    .init = orlix_tcti_logical_immediate_source_bound_test_init,
    .exit = orlix_tcti_logical_immediate_source_bound_test_exit,
    .test_cases = orlix_tcti_logical_immediate_source_bound_test_cases,
};

kunit_test_suite(orlix_tcti_logical_immediate_source_bound_test_suite);
