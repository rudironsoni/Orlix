// SPDX-License-Identifier: GPL-2.0-only
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

#define TCTI_SCALAR_MULDIV_SVC 0xd4000001U
#define TCTI_SCALAR_MULDIV_DP2_MASK 0xffe0fc00U
#define TCTI_SCALAR_MULDIV_DP3_MASK 0xffe08000U

enum tcti_scalar_muldiv_kind {
	TCTI_SCALAR_MULDIV_UDIV,
	TCTI_SCALAR_MULDIV_SDIV,
	TCTI_SCALAR_MULDIV_MADD,
	TCTI_SCALAR_MULDIV_MSUB,
	TCTI_SCALAR_MULDIV_SMADDL,
	TCTI_SCALAR_MULDIV_SMSUBL,
	TCTI_SCALAR_MULDIV_SMULH,
	TCTI_SCALAR_MULDIV_UMADDL,
	TCTI_SCALAR_MULDIV_UMSUBL,
	TCTI_SCALAR_MULDIV_UMULH,
};

struct tcti_scalar_muldiv_leaf {
	u16 ordinal;
	const char *name;
	u32 mask;
	u32 pattern;
	enum tcti_scalar_muldiv_kind kind;
	bool is_64bit;
	bool has_ra;
};

/* Exact direct leaves from the pinned Arm AARCHMRS 2026-06 manifest. */
static const struct tcti_scalar_muldiv_leaf tcti_scalar_muldiv_leaves[] = {
	{ 3356, "UDIV_32_dp_2src", TCTI_SCALAR_MULDIV_DP2_MASK,
	  0x1ac00800U, TCTI_SCALAR_MULDIV_UDIV, false, false },
	{ 3357, "SDIV_32_dp_2src", TCTI_SCALAR_MULDIV_DP2_MASK,
	  0x1ac00c00U, TCTI_SCALAR_MULDIV_SDIV, false, false },
	{ 3373, "UDIV_64_dp_2src", TCTI_SCALAR_MULDIV_DP2_MASK,
	  0x9ac00800U, TCTI_SCALAR_MULDIV_UDIV, true, false },
	{ 3374, "SDIV_64_dp_2src", TCTI_SCALAR_MULDIV_DP2_MASK,
	  0x9ac00c00U, TCTI_SCALAR_MULDIV_SDIV, true, false },
	{ 3495, "MADD_32A_dp_3src", TCTI_SCALAR_MULDIV_DP3_MASK,
	  0x1b000000U, TCTI_SCALAR_MULDIV_MADD, false, true },
	{ 3496, "MSUB_32A_dp_3src", TCTI_SCALAR_MULDIV_DP3_MASK,
	  0x1b008000U, TCTI_SCALAR_MULDIV_MSUB, false, true },
	{ 3497, "MADD_64A_dp_3src", TCTI_SCALAR_MULDIV_DP3_MASK,
	  0x9b000000U, TCTI_SCALAR_MULDIV_MADD, true, true },
	{ 3498, "MSUB_64A_dp_3src", TCTI_SCALAR_MULDIV_DP3_MASK,
	  0x9b008000U, TCTI_SCALAR_MULDIV_MSUB, true, true },
	{ 3499, "SMADDL_64WA_dp_3src", TCTI_SCALAR_MULDIV_DP3_MASK,
	  0x9b200000U, TCTI_SCALAR_MULDIV_SMADDL, true, true },
	{ 3500, "SMSUBL_64WA_dp_3src", TCTI_SCALAR_MULDIV_DP3_MASK,
	  0x9b208000U, TCTI_SCALAR_MULDIV_SMSUBL, true, true },
	{ 3501, "SMULH_64_dp_3src", TCTI_SCALAR_MULDIV_DP2_MASK,
	  0x9b407c00U, TCTI_SCALAR_MULDIV_SMULH, true, false },
	{ 3504, "UMADDL_64WA_dp_3src", TCTI_SCALAR_MULDIV_DP3_MASK,
	  0x9ba00000U, TCTI_SCALAR_MULDIV_UMADDL, true, true },
	{ 3505, "UMSUBL_64WA_dp_3src", TCTI_SCALAR_MULDIV_DP3_MASK,
	  0x9ba08000U, TCTI_SCALAR_MULDIV_UMSUBL, true, true },
	{ 3506, "UMULH_64_dp_3src", TCTI_SCALAR_MULDIV_DP2_MASK,
	  0x9bc07c00U, TCTI_SCALAR_MULDIV_UMULH, true, false },
};

static u32 tcti_scalar_muldiv_instruction(const struct tcti_scalar_muldiv_leaf *leaf,
					  u8 rd, u8 rn, u8 rm, u8 ra)
{
	u32 instruction = leaf->pattern | ((u32)rm << 16) |
		((u32)rn << 5) | rd;

	if (leaf->has_ra)
		instruction |= (u32)ra << 10;
	return instruction;
}

static u64 tcti_scalar_muldiv_oracle(const struct tcti_scalar_muldiv_leaf *leaf,
				      u64 left, u64 right, u64 accumulator)
{
	u64 mask = leaf->is_64bit ? U64_MAX : U32_MAX;

	left &= mask;
	right &= mask;
	accumulator &= mask;
	switch (leaf->kind) {
	case TCTI_SCALAR_MULDIV_UDIV:
		return right ? left / right : 0;
	case TCTI_SCALAR_MULDIV_SDIV:
		if (!right)
			return 0;
		if (leaf->is_64bit) {
			s64 dividend = left, divisor = right;

			return dividend == S64_MIN && divisor == -1 ? (u64)dividend :
				(u64)(dividend / divisor);
		}
		{
			s32 dividend = (s32)(u32)left, divisor = (s32)(u32)right;

			return dividend == S32_MIN && divisor == -1 ? (u32)dividend :
				(u32)(dividend / divisor);
		}
	case TCTI_SCALAR_MULDIV_MADD:
		return (accumulator + left * right) & mask;
	case TCTI_SCALAR_MULDIV_MSUB:
		return (accumulator - left * right) & mask;
	case TCTI_SCALAR_MULDIV_SMADDL:
		return accumulator + (u64)((s64)(s32)(u32)left *
						 (s64)(s32)(u32)right);
	case TCTI_SCALAR_MULDIV_SMSUBL:
		return accumulator - (u64)((s64)(s32)(u32)left *
						 (s64)(s32)(u32)right);
	case TCTI_SCALAR_MULDIV_SMULH:
		return (u64)(((__int128)(s64)left * (s64)right) >> 64);
	case TCTI_SCALAR_MULDIV_UMADDL:
		return accumulator + (u64)(u32)left * (u64)(u32)right;
	case TCTI_SCALAR_MULDIV_UMSUBL:
		return accumulator - (u64)(u32)left * (u64)(u32)right;
	case TCTI_SCALAR_MULDIV_UMULH:
		return (u64)(((unsigned __int128)left * right) >> 64);
	}
	return 0;
}

static unsigned long tcti_scalar_muldiv_map(struct kunit *test, u32 instruction)
{
	const u32 program[] = { instruction, TCTI_SCALAR_MULDIV_SVC };
	unsigned long mapped;

	mapped = ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
				 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(mapped));
	KUNIT_ASSERT_EQ(test, 0, tcti_write_user_data(current->mm, mapped, program,
						       sizeof(program)));
	KUNIT_ASSERT_EQ(test, 0, sys_mprotect(mapped, PAGE_SIZE, PROT_READ | PROT_EXEC));
	return mapped;
}

static void tcti_scalar_muldiv_expect_decode(struct kunit *test,
	const struct tcti_scalar_muldiv_leaf *leaf, u32 instruction,
	u8 rd, u8 rn, u8 rm, u8 ra)
{
	struct tcti_decoded_instruction decoded = tcti_decode_aarch64(instruction);

	KUNIT_ASSERT_EQ_MSG(test, leaf->has_ra ? TCTI_DECODE_MULTIPLY_ADD_SUB :
			TCTI_DECODE_DATA_PROCESSING_2SOURCE, decoded.decode_class,
			"%s %#x", leaf->name, instruction);
	KUNIT_EXPECT_EQ(test, leaf->is_64bit, decoded.is_64bit);
	KUNIT_EXPECT_EQ(test, rd, decoded.rd);
	KUNIT_EXPECT_EQ(test, rn, decoded.rn);
	KUNIT_EXPECT_EQ(test, rm, decoded.rm);
	if (leaf->has_ra)
		KUNIT_EXPECT_EQ(test, ra, decoded.ra);
}

static void tcti_scalar_muldiv_execute(struct kunit *test,
	const struct tcti_scalar_muldiv_leaf *leaf, u8 rd, u8 rn, u8 rm, u8 ra,
	u64 left, u64 right, u64 accumulator)
{
	u32 instruction = tcti_scalar_muldiv_instruction(leaf, rd, rn, rm, ra);
	unsigned long mapped = tcti_scalar_muldiv_map(test, instruction);
	struct pt_regs regs = {};
	struct pt_regs before, expected_regs;
	struct tcti_result result;
	u32 code[2] = {};
	u8 reg;

	KUNIT_ASSERT_NE(test, 0UL, mapped);
	for (reg = 0; reg < 31; reg++)
		regs.regs[reg] = 0x9e3779b97f4a7c15ULL ^ ((u64)instruction << (reg & 15)) ^ reg;
	if (rn != 31)
		regs.regs[rn] = left;
	if (rm != 31)
		regs.regs[rm] = right;
	if (leaf->has_ra && ra != 31)
		regs.regs[ra] = accumulator;
	regs.orig_x0 = 0x13579bdf2468ace0UL;
	regs.unused = 0x76543210U;
	regs.sp = 0x00000001fffffff0ULL;
	regs.pc = mapped;
	regs.pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_Z_BIT | PSR_C_BIT | PSR_V_BIT | PSR_D_BIT;
	regs.syscallno = NO_SYSCALL;
	before = regs;
	expected_regs = before;
	if (rd != 31)
		expected_regs.regs[rd] = tcti_scalar_muldiv_oracle(leaf,
			rn == 31 ? 0 : before.regs[rn], rm == 31 ? 0 : before.regs[rm],
			leaf->has_ra && ra != 31 ? before.regs[ra] : 0);
	expected_regs.pc += sizeof(u32);

	result = tcti_resume_user(current, &regs, current->mm);
	KUNIT_ASSERT_EQ_MSG(test, TCTI_EXIT_SYSCALL, result.reason, "%s %#x", leaf->name, instruction);
	KUNIT_EXPECT_EQ(test, 0L, result.status);
	KUNIT_EXPECT_EQ(test, TCTI_SCALAR_MULDIV_SVC, result.instruction);
	KUNIT_EXPECT_EQ(test, mapped + sizeof(u32), result.pc);
	KUNIT_EXPECT_MEMEQ(test, &expected_regs, &regs, sizeof(regs));
	KUNIT_EXPECT_EQ(test, 0, tcti_read_user_data(current->mm, mapped, code, sizeof(code)));
	KUNIT_EXPECT_EQ(test, instruction, code[0]);
	KUNIT_EXPECT_EQ(test, TCTI_SCALAR_MULDIV_SVC, code[1]);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
}

static void tcti_scalar_muldiv_source_bindings(struct kunit *test)
{
	size_t index;

	KUNIT_ASSERT_EQ(test, 14U, ARRAY_SIZE(tcti_scalar_muldiv_leaves));
	for (index = 0; index < ARRAY_SIZE(tcti_scalar_muldiv_leaves); index++) {
		const struct tcti_scalar_muldiv_leaf *leaf = &tcti_scalar_muldiv_leaves[index];

		KUNIT_EXPECT_GT(test, leaf->ordinal, (u16)0);
		KUNIT_EXPECT_EQ(test, leaf->pattern, leaf->pattern & leaf->mask);
		tcti_scalar_muldiv_expect_decode(test, leaf,
			tcti_scalar_muldiv_instruction(leaf, 7, 19, 11, leaf->has_ra ? 13 : 31),
			7, 19, 11, leaf->has_ra ? 13 : 31);
	}
}

static void tcti_scalar_muldiv_all_legal_register_fields(struct kunit *test)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(tcti_scalar_muldiv_leaves); index++) {
		const struct tcti_scalar_muldiv_leaf *leaf = &tcti_scalar_muldiv_leaves[index];
		u8 field;

		for (field = 0; field < 32; field++) {
			tcti_scalar_muldiv_expect_decode(test, leaf,
				tcti_scalar_muldiv_instruction(leaf, field, 19, 11, leaf->has_ra ? 13 : 31), field, 19, 11, leaf->has_ra ? 13 : 31);
			tcti_scalar_muldiv_expect_decode(test, leaf,
				tcti_scalar_muldiv_instruction(leaf, 7, field, 11, leaf->has_ra ? 13 : 31), 7, field, 11, leaf->has_ra ? 13 : 31);
			tcti_scalar_muldiv_expect_decode(test, leaf,
				tcti_scalar_muldiv_instruction(leaf, 7, 19, field, leaf->has_ra ? 13 : 31), 7, 19, field, leaf->has_ra ? 13 : 31);
			if (leaf->has_ra)
				tcti_scalar_muldiv_expect_decode(test, leaf,
					tcti_scalar_muldiv_instruction(leaf, 7, 19, 11, field), 7, 19, 11, field);
		}
	}
}

static void tcti_scalar_muldiv_production_path_semantics(struct kunit *test)
{
	static const u64 values[] = { 0, 1, 3, U32_MAX, U64_MAX,
		0x8000000000000000ULL, 0x8123456789abcdefULL };
	size_t index;

	for (index = 0; index < ARRAY_SIZE(tcti_scalar_muldiv_leaves); index++) {
		const struct tcti_scalar_muldiv_leaf *leaf = &tcti_scalar_muldiv_leaves[index];
		u64 divisor = values[(index + 2) % ARRAY_SIZE(values)];

		tcti_scalar_muldiv_execute(test, leaf, 7, 19, 11, leaf->has_ra ? 13 : 31,
			values[index % ARRAY_SIZE(values)], divisor, values[(index + 4) % ARRAY_SIZE(values)]);
		/* Division by zero and signed MIN/-1 have architectural result rules. */
		if (leaf->kind == TCTI_SCALAR_MULDIV_UDIV || leaf->kind == TCTI_SCALAR_MULDIV_SDIV)
			tcti_scalar_muldiv_execute(test, leaf, 7, 19, 11, 31,
				leaf->is_64bit ? U64_MAX : U32_MAX, 0, 0);
		if (leaf->kind == TCTI_SCALAR_MULDIV_SDIV)
			tcti_scalar_muldiv_execute(test, leaf, 7, 19, 11, 31,
				leaf->is_64bit ? BIT_ULL(63) : BIT_ULL(31),
				leaf->is_64bit ? U64_MAX : U32_MAX, 0);
	}
}

static void tcti_scalar_muldiv_aliases_and_register_overlap(struct kunit *test)
{
	/* ra == 31 encodes MUL/MNEG, SMULL/SMNEGL, UMULL/UMNEGL aliases. */
	tcti_scalar_muldiv_execute(test, &tcti_scalar_muldiv_leaves[4], 19, 19, 11, 31, 6, 7, 0);
	tcti_scalar_muldiv_execute(test, &tcti_scalar_muldiv_leaves[5], 11, 19, 11, 31, 6, 7, 0);
	tcti_scalar_muldiv_execute(test, &tcti_scalar_muldiv_leaves[8], 13, 19, 11, 31, U32_MAX, 3, 0);
	tcti_scalar_muldiv_execute(test, &tcti_scalar_muldiv_leaves[9], 19, 19, 11, 31, U32_MAX, 3, 0);
	tcti_scalar_muldiv_execute(test, &tcti_scalar_muldiv_leaves[11], 11, 19, 11, 31, U32_MAX, 3, 0);
	tcti_scalar_muldiv_execute(test, &tcti_scalar_muldiv_leaves[12], 19, 19, 11, 31, U32_MAX, 3, 0);
	/* All operands may overlap the destination for the non-alias forms. */
	tcti_scalar_muldiv_execute(test, &tcti_scalar_muldiv_leaves[6], 19, 19, 11, 13, 6, 7, 5);
	tcti_scalar_muldiv_execute(test, &tcti_scalar_muldiv_leaves[7], 11, 19, 11, 13, 6, 7, 50);
	tcti_scalar_muldiv_execute(test, &tcti_scalar_muldiv_leaves[6], 13, 19, 11, 13, 6, 7, 5);
	tcti_scalar_muldiv_execute(test, &tcti_scalar_muldiv_leaves[2], 19, 19, 11, 31, U64_MAX, 3, 0);
}

static void tcti_scalar_muldiv_xzr_source_and_destination(struct kunit *test)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(tcti_scalar_muldiv_leaves); index++) {
		const struct tcti_scalar_muldiv_leaf *leaf = &tcti_scalar_muldiv_leaves[index];

		tcti_scalar_muldiv_execute(test, leaf, 7, 31, 11,
			leaf->has_ra ? 13 : 31, 0xdeadbeefULL, 3, 5);
		tcti_scalar_muldiv_execute(test, leaf, 7, 19, 31,
			leaf->has_ra ? 13 : 31, 6, 0xdeadbeefULL, 5);
		if (leaf->has_ra)
			tcti_scalar_muldiv_execute(test, leaf, 7, 19, 11, 31,
				6, 7, 0xdeadbeefULL);
		tcti_scalar_muldiv_execute(test, leaf, 31, 19, 11,
			leaf->has_ra ? 13 : 31, 6, 7, 5);
	}
}

static void tcti_scalar_muldiv_reserved_encodings_fail_before_state(struct kunit *test)
{
	static const u32 reserved[] = {
		0x1b200000U, 0x1ba00000U, 0x9b407800U, 0x9b407c00U | BIT(15),
		0x9b600000U, 0x9b800000U, 0x9be00000U,
	};
	size_t index;

	for (index = 0; index < ARRAY_SIZE(reserved); index++) {
		struct pt_regs regs = {};
		struct pt_regs before;
		struct tcti_result result;
		unsigned long mapped = tcti_scalar_muldiv_map(test, reserved[index]);
		u32 observed[2] = {};

		regs.regs[1] = 0x1234;
		regs.regs[2] = 0x5678;
		regs.pc = mapped;
		regs.pstate = PSR_MODE_EL0t | PSR_Z_BIT;
		regs.syscallno = NO_SYSCALL;
		before = regs;
		result = tcti_resume_user(current, &regs, current->mm);
		KUNIT_EXPECT_EQ(test, TCTI_EXIT_UNSUPPORTED_INSTRUCTION, result.reason);
		KUNIT_EXPECT_EQ(test, -EOPNOTSUPP, result.status);
		KUNIT_EXPECT_EQ(test, reserved[index], result.instruction);
		KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
		KUNIT_EXPECT_EQ(test, 0, tcti_read_user_data(current->mm, mapped, observed, sizeof(observed)));
		KUNIT_EXPECT_EQ(test, reserved[index], observed[0]);
		KUNIT_EXPECT_EQ(test, TCTI_SCALAR_MULDIV_SVC, observed[1]);
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
	}
}

static struct kunit_case tcti_scalar_muldiv_source_bound_test_cases[] = {
	KUNIT_CASE(tcti_scalar_muldiv_source_bindings),
	KUNIT_CASE(tcti_scalar_muldiv_all_legal_register_fields),
	KUNIT_CASE(tcti_scalar_muldiv_production_path_semantics),
	KUNIT_CASE(tcti_scalar_muldiv_aliases_and_register_overlap),
	KUNIT_CASE(tcti_scalar_muldiv_xzr_source_and_destination),
	KUNIT_CASE(tcti_scalar_muldiv_reserved_encodings_fail_before_state),
	{}
};

static struct kunit_suite tcti_scalar_muldiv_source_bound_test_suite = {
	.name = "orlix-tcti-scalar-multiply-divide-source-bound",
	.test_cases = tcti_scalar_muldiv_source_bound_test_cases,
};

kunit_test_suite(tcti_scalar_muldiv_source_bound_test_suite);
