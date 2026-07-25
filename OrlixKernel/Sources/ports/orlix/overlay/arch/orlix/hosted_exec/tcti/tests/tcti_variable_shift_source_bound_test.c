// SPDX-License-Identifier: GPL-2.0-only
#include <asm/processor.h>
#include <asm/ptrace.h>
#include <asm/tcti.h>
#include <kunit/test.h>
#include <linux/bitops.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/sched.h>
#include <linux/syscalls.h>

#include "../decode_aarch64.h"

#define TCTI_VARIABLE_SHIFT_SOURCE_MASK 0xffe0fc00U
#define TCTI_VARIABLE_SHIFT_SVC 0xd4000001U

struct tcti_variable_shift_leaf {
	u16 source_ordinal;
	const char *source_name;
	u32 source_pattern;
	enum tcti_data_processing_2source_op operation;
	bool is_64bit;
};

/* Exact direct leaves from pinned Arm AARCHMRS 2026-06. */
static const struct tcti_variable_shift_leaf tcti_variable_shift_leaves[] = {
	{ 3358, "LSLV_32_dp_2src", 0x1ac02000U, TCTI_DP2_LSLV, false },
	{ 3359, "LSRV_32_dp_2src", 0x1ac02400U, TCTI_DP2_LSRV, false },
	{ 3360, "ASRV_32_dp_2src", 0x1ac02800U, TCTI_DP2_ASRV, false },
	{ 3361, "RORV_32_dp_2src", 0x1ac02c00U, TCTI_DP2_RORV, false },
	{ 3377, "LSLV_64_dp_2src", 0x9ac02000U, TCTI_DP2_LSLV, true },
	{ 3378, "LSRV_64_dp_2src", 0x9ac02400U, TCTI_DP2_LSRV, true },
	{ 3379, "ASRV_64_dp_2src", 0x9ac02800U, TCTI_DP2_ASRV, true },
	{ 3380, "RORV_64_dp_2src", 0x9ac02c00U, TCTI_DP2_RORV, true },
};

static u32 tcti_variable_shift_instruction(
	const struct tcti_variable_shift_leaf *leaf, u8 rd, u8 rn, u8 rm)
{
	return leaf->source_pattern | ((u32)rm << 16) | ((u32)rn << 5) | rd;
}

static const struct tcti_variable_shift_leaf *
tcti_variable_shift_source_leaf(u32 instruction)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(tcti_variable_shift_leaves); index++) {
		const struct tcti_variable_shift_leaf *leaf =
			&tcti_variable_shift_leaves[index];

		if ((instruction & TCTI_VARIABLE_SHIFT_SOURCE_MASK) ==
		    leaf->source_pattern)
			return leaf;
	}
	return NULL;
}

static u64 tcti_variable_shift_mask(bool is_64bit)
{
	return is_64bit ? U64_MAX : U32_MAX;
}

static u64 tcti_variable_shift_expected(
	const struct tcti_variable_shift_leaf *leaf, u64 value, u64 count)
{
	u8 width = leaf->is_64bit ? 64 : 32;
	u8 amount = count & (width - 1);
	u64 mask = tcti_variable_shift_mask(leaf->is_64bit);

	value &= mask;
	switch (leaf->operation) {
	case TCTI_DP2_LSLV:
		return (value << amount) & mask;
	case TCTI_DP2_LSRV:
		return value >> amount;
	case TCTI_DP2_ASRV:
		return leaf->is_64bit ? (u64)((s64)value >> amount) :
			(u32)((s32)(u32)value >> amount);
	case TCTI_DP2_RORV:
		return amount ? ((value >> amount) |
				 (value << (width - amount))) & mask : value;
	default:
		return 0;
	}
}

static unsigned long tcti_variable_shift_map_program(struct kunit *test,
						      u32 instruction)
{
	const u32 program[] = { instruction, TCTI_VARIABLE_SHIFT_SVC };
	unsigned long mapped;
	int ret;

	KUNIT_ASSERT_NOT_NULL(test, current->mm);
	mapped = ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
				 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(mapped));
	ret = tcti_write_user_data(current->mm, mapped, program, sizeof(program));
	if (ret) {
		vm_munmap(mapped, PAGE_SIZE);
		KUNIT_FAIL(test, "could not write variable shift program: %d", ret);
		return 0;
	}
	ret = sys_mprotect(mapped, PAGE_SIZE, PROT_READ | PROT_EXEC);
	if (ret) {
		vm_munmap(mapped, PAGE_SIZE);
		KUNIT_FAIL(test, "could not protect variable shift program: %d", ret);
		return 0;
	}
	return mapped;
}

static void tcti_variable_shift_expect_decode(struct kunit *test,
	const struct tcti_variable_shift_leaf *leaf, u32 instruction, u8 rd, u8 rn,
	u8 rm)
{
	struct tcti_decoded_instruction decoded = tcti_decode_aarch64(instruction);

	KUNIT_ASSERT_EQ_MSG(test, TCTI_DECODE_DATA_PROCESSING_2SOURCE,
			    decoded.decode_class, "%s %#x", leaf->source_name,
			    instruction);
	KUNIT_EXPECT_EQ(test, leaf->operation, decoded.dp2_op);
	KUNIT_EXPECT_EQ(test, leaf->is_64bit, decoded.is_64bit);
	KUNIT_EXPECT_EQ(test, rd, decoded.rd);
	KUNIT_EXPECT_EQ(test, rn, decoded.rn);
	KUNIT_EXPECT_EQ(test, rm, decoded.rm);
}

static void tcti_variable_shift_execute(struct kunit *test,
	const struct tcti_variable_shift_leaf *leaf, u8 rd, u8 rn, u8 rm,
	u64 value, u64 count)
{
	u32 instruction = tcti_variable_shift_instruction(leaf, rd, rn, rm);
	unsigned long mapped = tcti_variable_shift_map_program(test, instruction);
	struct pt_regs regs = {};
	struct pt_regs before;
	struct pt_regs expected_regs;
	struct tcti_result result;
	const u64 gpr_x0 = 0x0123456789abcdefULL;
	const unsigned long orig_x0 = 0x13579bdf2468ace0UL;
	const u64 unused_gpr = 0xfedcba9876543210ULL;
	const u32 unused = 0x76543210U;
	u64 expected;
	u8 reg;

	KUNIT_ASSERT_NE(test, 0UL, mapped);
	for (reg = 0; reg < 31; reg++)
		regs.regs[reg] = 0x9e3779b97f4a7c15ULL ^
			((u64)instruction << (reg & 15)) ^ reg;
	if (rn != 31)
		regs.regs[rn] = value;
	if (rm != 31)
		regs.regs[rm] = count;
	regs.regs[0] = gpr_x0;
	regs.regs[29] = unused_gpr;
	regs.orig_x0 = orig_x0;
	regs.unused = unused;
	regs.sp = 0x00000001fffffff0ULL;
	regs.pc = mapped;
	regs.pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_Z_BIT | PSR_C_BIT |
		PSR_V_BIT | PSR_D_BIT;
	regs.syscallno = NO_SYSCALL;
	before = regs;
	expected = tcti_variable_shift_expected(leaf,
		rn == 31 ? 0 : before.regs[rn], rm == 31 ? 0 : before.regs[rm]);
	expected_regs = before;
	if (rd != 31)
		expected_regs.regs[rd] = expected;
	expected_regs.pc += sizeof(u32);

	result = tcti_resume_user(current, &regs, current->mm);
	KUNIT_ASSERT_EQ_MSG(test, TCTI_EXIT_SYSCALL, result.reason,
			    "%s %#x", leaf->source_name, instruction);
	KUNIT_EXPECT_EQ(test, 0L, result.status);
	KUNIT_EXPECT_EQ(test, TCTI_VARIABLE_SHIFT_SVC, result.instruction);
	KUNIT_EXPECT_EQ(test, mapped + sizeof(u32), result.pc);
	KUNIT_EXPECT_EQ(test, NO_SYSCALL, before.syscallno);
	KUNIT_EXPECT_EQ(test, NO_SYSCALL, regs.syscallno);
	KUNIT_EXPECT_EQ(test, gpr_x0, before.regs[0]);
	KUNIT_EXPECT_EQ(test, orig_x0, before.orig_x0);
	KUNIT_EXPECT_EQ(test, unused_gpr, before.regs[29]);
	KUNIT_EXPECT_EQ(test, unused, before.unused);
	KUNIT_EXPECT_MEMEQ(test, &expected_regs, &regs, sizeof(regs));
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
}

static void tcti_variable_shift_source_bindings(struct kunit *test)
{
	static const u16 expected_ordinals[] = {
		3358, 3359, 3360, 3361, 3377, 3378, 3379, 3380,
	};
	size_t index;

	KUNIT_ASSERT_EQ(test, 8U, ARRAY_SIZE(tcti_variable_shift_leaves));
	for (index = 0; index < ARRAY_SIZE(tcti_variable_shift_leaves); index++) {
		const struct tcti_variable_shift_leaf *leaf =
			&tcti_variable_shift_leaves[index];
		u32 instruction = tcti_variable_shift_instruction(leaf, 7, 19, 11);

		KUNIT_EXPECT_TRUE(test, leaf->source_name[0]);
		KUNIT_EXPECT_EQ(test, expected_ordinals[index], leaf->source_ordinal);
		KUNIT_EXPECT_EQ(test, leaf->source_pattern,
				leaf->source_pattern & TCTI_VARIABLE_SHIFT_SOURCE_MASK);
		KUNIT_EXPECT_PTR_EQ(test, leaf,
			tcti_variable_shift_source_leaf(instruction));
		tcti_variable_shift_expect_decode(test, leaf, instruction, 7, 19,
						  11);
	}
}

static void tcti_variable_shift_all_legal_register_encodings(struct kunit *test)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(tcti_variable_shift_leaves); index++) {
		const struct tcti_variable_shift_leaf *leaf =
			&tcti_variable_shift_leaves[index];
		u8 rn;

		for (rn = 0; rn < 32; rn++) {
			u8 rm;

			for (rm = 0; rm < 32; rm++) {
				u8 rd;

				for (rd = 0; rd < 32; rd++)
					tcti_variable_shift_expect_decode(test, leaf,
							tcti_variable_shift_instruction(leaf, rd,
								rn, rm), rd, rn, rm);
			}
		}
	}
}

static void tcti_variable_shift_production_path_semantics(struct kunit *test)
{
	static const u64 counts[] = { 0, 1, 31, 32, 33, 63, 64, 65, U64_MAX };
	static const u64 values[] = {
		0, U64_MAX, 0x8123456789abcdefULL, 0x7fffffffffffffffULL,
		0x8000000000000000ULL,
	};
	size_t leaf_index;
	size_t value_index;
	size_t count_index;

	for (leaf_index = 0; leaf_index < ARRAY_SIZE(tcti_variable_shift_leaves);
	     leaf_index++)
		for (value_index = 0; value_index < ARRAY_SIZE(values); value_index++)
			for (count_index = 0; count_index < ARRAY_SIZE(counts);
			     count_index++)
				tcti_variable_shift_execute(test,
					&tcti_variable_shift_leaves[leaf_index], 7, 19,
					11, values[value_index], counts[count_index]);
}

static void tcti_variable_shift_xzr_source_and_destination(struct kunit *test)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(tcti_variable_shift_leaves); index++) {
		const struct tcti_variable_shift_leaf *leaf =
			&tcti_variable_shift_leaves[index];

		tcti_variable_shift_execute(test, leaf, 7, 31, 11,
					0x0123456789abcdefULL, 63);
		tcti_variable_shift_execute(test, leaf, 7, 19, 31,
					0x0123456789abcdefULL, 63);
		tcti_variable_shift_execute(test, leaf, 31, 19, 11,
					0x0123456789abcdefULL, 63);
	}
}

static void tcti_variable_shift_destination_source_aliases(struct kunit *test)
{
	/* Exercise rd == rn and rd == rm at both architectural data widths. */
	tcti_variable_shift_execute(test, &tcti_variable_shift_leaves[0], 19, 19,
					11, 0x81234567U, 31);
	tcti_variable_shift_execute(test, &tcti_variable_shift_leaves[3], 11, 19,
					11, 0x81234567U, 31);
	tcti_variable_shift_execute(test, &tcti_variable_shift_leaves[4], 19, 19,
					11, 0x8123456789abcdefULL, 63);
	tcti_variable_shift_execute(test, &tcti_variable_shift_leaves[7], 11, 19,
					11, 0x8123456789abcdefULL, 63);
}

static struct kunit_case tcti_variable_shift_source_bound_test_cases[] = {
	KUNIT_CASE(tcti_variable_shift_source_bindings),
	KUNIT_CASE(tcti_variable_shift_all_legal_register_encodings),
	KUNIT_CASE(tcti_variable_shift_production_path_semantics),
	KUNIT_CASE(tcti_variable_shift_xzr_source_and_destination),
	KUNIT_CASE(tcti_variable_shift_destination_source_aliases),
	{}
};

struct kunit_suite tcti_variable_shift_source_bound_test_suite = {
	.name = "orlix-tcti-variable-shift-source-bound",
	.test_cases = tcti_variable_shift_source_bound_test_cases,
};

kunit_test_suite(tcti_variable_shift_source_bound_test_suite);
