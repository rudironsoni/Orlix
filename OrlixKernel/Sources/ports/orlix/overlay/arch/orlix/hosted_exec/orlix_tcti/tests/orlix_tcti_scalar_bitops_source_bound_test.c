// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include <linux/bitops.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/sched/mm.h>
#include <linux/mman.h>
#include <linux/sched.h>
#include <linux/syscalls.h>
#include <asm/processor.h>
#include <asm/ptrace.h>
#include <asm/orlix_tcti.h>

#include "../decode_aarch64.h"

#define ORLIX_TCTI_DP1_SOURCE_MASK	0xfffffc00U
#define ORLIX_TCTI_SVC		0xd4000001U

struct orlix_tcti_scalar_bitops_leaf {
	u16 source_ordinal;
	const char *source_name;
	u32 source_pattern;
	enum orlix_tcti_data_processing_1source_op operation;
	bool is_64bit;
};

/* Exact direct leaves from pinned Arm AARCHMRS 2026-06. */
static const struct orlix_tcti_scalar_bitops_leaf orlix_tcti_scalar_bitops_leaves[] = {
	{ 3389, "RBIT_32_dp_1src", 0x5ac00000U, ORLIX_TCTI_DP1_RBIT, false },
	{ 3390, "REV16_32_dp_1src", 0x5ac00400U, ORLIX_TCTI_DP1_REV16, false },
	{ 3391, "REV_32_dp_1src", 0x5ac00800U, ORLIX_TCTI_DP1_REV, false },
	{ 3392, "CLZ_32_dp_1src", 0x5ac01000U, ORLIX_TCTI_DP1_CLZ, false },
	{ 3393, "CLS_32_dp_1src", 0x5ac01400U, ORLIX_TCTI_DP1_CLS, false },
	{ 3397, "RBIT_64_dp_1src", 0xdac00000U, ORLIX_TCTI_DP1_RBIT, true },
	{ 3398, "REV16_64_dp_1src", 0xdac00400U, ORLIX_TCTI_DP1_REV16, true },
	{ 3399, "REV32_64_dp_1src", 0xdac00800U, ORLIX_TCTI_DP1_REV32, true },
	{ 3400, "REV_64_dp_1src", 0xdac00c00U, ORLIX_TCTI_DP1_REV, true },
	{ 3401, "CLZ_64_dp_1src", 0xdac01000U, ORLIX_TCTI_DP1_CLZ, true },
	{ 3402, "CLS_64_dp_1src", 0xdac01400U, ORLIX_TCTI_DP1_CLS, true },
};

static u32 orlix_tcti_scalar_bitops_instruction(
	const struct orlix_tcti_scalar_bitops_leaf *leaf, u8 rn, u8 rd)
{
	return leaf->source_pattern | ((u32)rn << 5) | rd;
}

static const struct orlix_tcti_scalar_bitops_leaf *
orlix_tcti_scalar_bitops_source_leaf(u32 instruction)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_scalar_bitops_leaves); index++) {
		const struct orlix_tcti_scalar_bitops_leaf *leaf =
			&orlix_tcti_scalar_bitops_leaves[index];

		if ((instruction & ORLIX_TCTI_DP1_SOURCE_MASK) == leaf->source_pattern)
			return leaf;
	}
	return NULL;
}

static u64 orlix_tcti_scalar_bitops_mask(bool is_64bit)
{
	return is_64bit ? U64_MAX : U32_MAX;
}

static u64 orlix_tcti_scalar_bitops_reverse_bits(u64 value, u8 width)
{
	u64 result = 0;
	u8 bit;

	for (bit = 0; bit < width; bit++)
		result |= ((value >> bit) & 1) << (width - bit - 1);
	return result;
}

static u64 orlix_tcti_scalar_bitops_reverse_bytes(u64 value, u8 width,
					     u8 lane_width)
{
	u64 result = 0;
	u8 lane;
	u8 byte;

	for (lane = 0; lane < width; lane += lane_width)
		for (byte = 0; byte < lane_width / 8; byte++)
			result |= ((value >> (lane + byte * 8)) & 0xff) <<
				(lane + lane_width - 8 - byte * 8);
	return result;
}

static u64 orlix_tcti_scalar_bitops_clz(u64 value, u8 width)
{
	u64 count = 0;
	int bit;

	for (bit = width - 1; bit >= 0; bit--) {
		if (value & BIT_ULL(bit))
			break;
		count++;
	}
	return count;
}

static u64 orlix_tcti_scalar_bitops_cls(u64 value, u8 width)
{
	bool sign = value & BIT_ULL(width - 1);
	u64 count = 0;
	int bit;

	for (bit = width - 2; bit >= 0; bit--) {
		if (!!(value & BIT_ULL(bit)) != sign)
			break;
		count++;
	}
	return count;
}

static u64 orlix_tcti_scalar_bitops_expected(
	const struct orlix_tcti_scalar_bitops_leaf *leaf, u64 value)
{
	u8 width = leaf->is_64bit ? 64 : 32;

	value &= orlix_tcti_scalar_bitops_mask(leaf->is_64bit);
	switch (leaf->operation) {
	case ORLIX_TCTI_DP1_RBIT:
		return orlix_tcti_scalar_bitops_reverse_bits(value, width);
	case ORLIX_TCTI_DP1_REV16:
		return orlix_tcti_scalar_bitops_reverse_bytes(value, width, 16);
	case ORLIX_TCTI_DP1_REV32:
		return orlix_tcti_scalar_bitops_reverse_bytes(value, width, 32);
	case ORLIX_TCTI_DP1_REV:
		return orlix_tcti_scalar_bitops_reverse_bytes(value, width, width);
	case ORLIX_TCTI_DP1_CLZ:
		return orlix_tcti_scalar_bitops_clz(value, width);
	case ORLIX_TCTI_DP1_CLS:
		return orlix_tcti_scalar_bitops_cls(value, width);
	default:
		return 0;
	}
}

static unsigned long orlix_tcti_scalar_bitops_map_program(struct kunit *test,
						     u32 instruction)
{
	const u32 program[] = { instruction, ORLIX_TCTI_SVC };
	unsigned long mapped;
	int ret;

	KUNIT_ASSERT_NOT_NULL(test, current->mm);
	mapped = ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
				 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(mapped));
	ret = orlix_tcti_write_user_data(current->mm, mapped, program, sizeof(program));
	if (ret) {
		vm_munmap(mapped, PAGE_SIZE);
		KUNIT_FAIL(test, "could not write scalar bitops program: %d", ret);
		return 0;
	}
	ret = sys_mprotect(mapped, PAGE_SIZE, PROT_READ | PROT_EXEC);
	if (ret) {
		vm_munmap(mapped, PAGE_SIZE);
		KUNIT_FAIL(test, "could not protect scalar bitops program: %d", ret);
		return 0;
	}
	return mapped;
}

static void orlix_tcti_scalar_bitops_expect_decode(struct kunit *test,
	const struct orlix_tcti_scalar_bitops_leaf *leaf, u32 instruction, u8 rn, u8 rd)
{
	struct orlix_tcti_decoded_instruction decoded = orlix_tcti_decode_aarch64(instruction);

	KUNIT_ASSERT_EQ_MSG(test, ORLIX_TCTI_DECODE_DATA_PROCESSING_1SOURCE,
			    decoded.decode_class, "%s %#x", leaf->source_name,
			    instruction);
	KUNIT_EXPECT_EQ(test, leaf->operation, decoded.dp1_op);
	KUNIT_EXPECT_EQ(test, leaf->is_64bit, decoded.is_64bit);
	KUNIT_EXPECT_EQ(test, rn, decoded.rn);
	KUNIT_EXPECT_EQ(test, rd, decoded.rd);
}

static void orlix_tcti_scalar_bitops_execute(struct kunit *test,
	const struct orlix_tcti_scalar_bitops_leaf *leaf, u8 rn, u8 rd, u64 input)
{
	u32 instruction = orlix_tcti_scalar_bitops_instruction(leaf, rn, rd);
	unsigned long mapped = orlix_tcti_scalar_bitops_map_program(test, instruction);
	struct pt_regs regs = {};
	struct pt_regs before;
	struct orlix_tcti_result result;
	u64 expected;
	u8 reg;

	KUNIT_ASSERT_NE(test, 0UL, mapped);
	for (reg = 0; reg < 31; reg++)
		regs.regs[reg] = 0x9e3779b97f4a7c15ULL ^
			((u64)instruction << (reg & 15)) ^ reg;
	regs.regs[rn == 31 ? 0 : rn] = input;
	regs.sp = 0x00000001fffffff0ULL;
	regs.pc = mapped;
	regs.pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_C_BIT | PSR_D_BIT;
	regs.syscallno = NO_SYSCALL;
	before = regs;
	expected = orlix_tcti_scalar_bitops_expected(leaf,
		rn == 31 ? 0 : before.regs[rn]) &
		orlix_tcti_scalar_bitops_mask(leaf->is_64bit);

	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_ASSERT_EQ_MSG(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason,
			    "%s %#x", leaf->source_name, instruction);
	KUNIT_EXPECT_EQ(test, 0L, result.status);
	KUNIT_EXPECT_EQ(test, mapped + sizeof(u32), result.pc);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_SVC, result.instruction);
	for (reg = 0; reg < 31; reg++)
		KUNIT_EXPECT_EQ_MSG(test,
			reg == rd ? expected : before.regs[reg], regs.regs[reg],
			"%s x%u", leaf->source_name, reg);
	KUNIT_EXPECT_EQ(test, before.sp, regs.sp);
	KUNIT_EXPECT_EQ(test, before.pstate, regs.pstate);
	KUNIT_EXPECT_EQ(test, mapped + sizeof(u32), regs.pc);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
}

static void orlix_tcti_scalar_bitops_source_bindings(struct kunit *test)
{
	static const u16 expected_ordinals[] = {
		3389, 3390, 3391, 3392, 3393, 3397, 3398, 3399, 3400,
		3401, 3402,
	};
	size_t index;

	KUNIT_ASSERT_EQ(test, 11U, ARRAY_SIZE(orlix_tcti_scalar_bitops_leaves));
	KUNIT_ASSERT_EQ(test, ARRAY_SIZE(orlix_tcti_scalar_bitops_leaves),
			ARRAY_SIZE(expected_ordinals));
	for (index = 0; index < ARRAY_SIZE(orlix_tcti_scalar_bitops_leaves); index++) {
		const struct orlix_tcti_scalar_bitops_leaf *leaf =
			&orlix_tcti_scalar_bitops_leaves[index];
		u32 instruction = orlix_tcti_scalar_bitops_instruction(leaf, 19, 7);

		KUNIT_EXPECT_TRUE(test, leaf->source_name[0]);
		KUNIT_EXPECT_EQ(test, expected_ordinals[index], leaf->source_ordinal);
		KUNIT_EXPECT_EQ(test, leaf->source_pattern,
				leaf->source_pattern & ORLIX_TCTI_DP1_SOURCE_MASK);
		KUNIT_EXPECT_PTR_EQ(test, leaf,
			orlix_tcti_scalar_bitops_source_leaf(instruction));
		orlix_tcti_scalar_bitops_expect_decode(test, leaf, instruction, 19, 7);
	}
}

static void orlix_tcti_scalar_bitops_all_legal_register_encodings(struct kunit *test)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_scalar_bitops_leaves); index++) {
		const struct orlix_tcti_scalar_bitops_leaf *leaf =
			&orlix_tcti_scalar_bitops_leaves[index];
		u8 rn;

		for (rn = 0; rn < 32; rn++) {
			u8 rd;

			for (rd = 0; rd < 32; rd++)
				orlix_tcti_scalar_bitops_expect_decode(test, leaf,
					orlix_tcti_scalar_bitops_instruction(leaf, rn, rd), rn, rd);
		}
	}
}

static void orlix_tcti_scalar_bitops_production_path_semantics(struct kunit *test)
{
	static const u64 inputs[] = {
		0, U64_MAX, 0x8123456789abcdefULL, 0x7fffffffffffffffULL,
		0x8000000000000000ULL,
	};
	size_t index;
	size_t input;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_scalar_bitops_leaves); index++)
		for (input = 0; input < ARRAY_SIZE(inputs); input++) {
			orlix_tcti_scalar_bitops_execute(test,
				&orlix_tcti_scalar_bitops_leaves[index], 19, 7, inputs[input]);
			orlix_tcti_scalar_bitops_execute(test,
				&orlix_tcti_scalar_bitops_leaves[index], 31, 7, inputs[input]);
			orlix_tcti_scalar_bitops_execute(test,
				&orlix_tcti_scalar_bitops_leaves[index], 19, 31, inputs[input]);
		}
}

static void orlix_tcti_scalar_bitops_reserved_encodings(struct kunit *test)
{
	u8 rn;

	/* Arm reserves the 32-bit encoding whose opcode would otherwise be REV32. */
	for (rn = 0; rn < 32; rn++) {
		u8 rd;

		for (rd = 0; rd < 32; rd++)
			KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_DECODE_UNSUPPORTED,
				orlix_tcti_decode_aarch64(0x5ac00c00U |
					((u32)rn << 5) | rd).decode_class,
				"reserved REV32 W%u, W%u", rd, rn);
	}
}

static struct kunit_case orlix_tcti_scalar_bitops_source_bound_test_cases[] = {
	KUNIT_CASE(orlix_tcti_scalar_bitops_source_bindings),
	KUNIT_CASE(orlix_tcti_scalar_bitops_all_legal_register_encodings),
	KUNIT_CASE(orlix_tcti_scalar_bitops_production_path_semantics),
	KUNIT_CASE(orlix_tcti_scalar_bitops_reserved_encodings),
	{}
};

static int orlix_tcti_scalar_bitops_source_bound_test_init(struct kunit *test)
{
	struct mm_struct *mm = mm_alloc();

	if (!mm)
		return -ENOMEM;
	kthread_use_mm(mm);
	test->priv = mm;
	return 0;
}

static void orlix_tcti_scalar_bitops_source_bound_test_exit(struct kunit *test)
{
	struct mm_struct *mm = test->priv;

	if (!mm)
		return;
	kthread_unuse_mm(mm);
	mmput(mm);
}

struct kunit_suite orlix_tcti_scalar_bitops_source_bound_test_suite = {
	.name = "orlix-tcti-scalar-bitops-source-bound",
	.init = orlix_tcti_scalar_bitops_source_bound_test_init,
	.exit = orlix_tcti_scalar_bitops_source_bound_test_exit,
	.test_cases = orlix_tcti_scalar_bitops_source_bound_test_cases,
};

kunit_test_suite(orlix_tcti_scalar_bitops_source_bound_test_suite);
